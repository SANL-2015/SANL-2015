#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <unistd.h>
#include <execinfo.h>
#include <stdint.h>
#include <papi.h>
#include <stdarg.h>
#include "backtrace-symbols.c"
#include "liblock.h"

#define conds 1

#define zi_lock              pthread_mutex_lock
#define zi_trylock           pthread_mutex_trylock
#define zi_unlock            pthread_mutex_unlock
#define zi_liblock_exec      liblock_exec
#define zi_liblock_cond_wait           liblock_cond_wait
#define zi_liblock_cond_timedwait      liblock_cond_timedwait
#define zi_liblock_lock_init liblock_lock_init

#define LOAD_FUNC(name, E)																								\
	do {																																	\
		real_##name = dlsym(RTLD_NEXT, S(name));														\
		if(E && !real_##name) { fprintf(stderr, "WARNING: unable to find symbol: %s\n", S(name)); } \
	} while (0)

#define LOAD_FUNC_VERSIONED(name, E, version)																								\
	do {																																	\
		real_##name = dlvsym(RTLD_NEXT, S(name), version);														\
		if(E && !real_##name) { fprintf(stderr, "WARNING: unable to find symbol: %s\n", S(name)); } \
	} while (0)




//		assert(real_##name);																								

#define real(name) real_ ## name

#define S(_) #_

#define get_cyc() PAPI_get_real_cyc()

#define PAGE_SIZE       4096
#define CACHE_LINE_SIZE 64

#define r_align(n, r)        (((n) + (r) - 1) & -(r)) 
#define cache_align(n)       r_align(n , CACHE_LINE_SIZE)
#define pad_to_cache_line(n) (cache_align(n) - (n))

#define MAX_THREADS 65536
#define N_HASH      65536
#define MAX_STACK   64

#define check_id(thread_id) if((thread_id) >= MAX_THREADS) { echo("too many thread, recompile"); exit(42); }

#define posix_id "posix-mutrace"

static void* (*real(zi_liblock_exec))(liblock_lock_t* lock, void* (*pending)(void*), void* val);
static int (*real(zi_liblock_cond_wait))(liblock_cond_t* cond, liblock_lock_t* lock);
static int (*real(zi_liblock_cond_timedwait))(liblock_cond_t* cond, liblock_lock_t* lock, struct timespec* abstime);
static int (*real(zi_liblock_lock_init))(const char* type, struct core* core, liblock_lock_t* lock, void* arg);

static int (*real_pthread_mutex_init)(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) = 0;
static int   (*real_pthread_mutex_destroy)(pthread_mutex_t* mutex);
static int   (*real(zi_lock))(pthread_mutex_t* mutex);
static int   (*real(zi_trylock))(pthread_mutex_t* mutex);
static int   (*real(zi_unlock))(pthread_mutex_t* mutex);
static int   (*real_pthread_create)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg);
#ifdef conds
static int   (*real_pthread_cond_timedwait)(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime);
static int   (*real_pthread_cond_wait)(pthread_cond_t* cond, pthread_mutex_t* mutex);
//static int   (*real_pthread_cond_signal)(pthread_cond_t* cond);
//static int   (*real_pthread_cond_broadcast)(pthread_cond_t* cond);
#endif
static void (*real_exit)(int status) __attribute__((noreturn));
static void (*real__exit)(int status) __attribute__((noreturn));
static void (*real__Exit)(int status) __attribute__((noreturn));


struct nested {
	unsigned long long first_cycle;
    
	unsigned long long real_last_cycle;

	unsigned long long depth;
	unsigned long long nb_outer_cs;

#define MAX_NESTED ((PAGE_SIZE - 4*sizeof(unsigned long long))/sizeof(unsigned long long))

	unsigned long long last_cycle[MAX_NESTED];
};

struct lock_info_thread {
	unsigned long long cycles_cs;

	char pad[pad_to_cache_line(sizeof(unsigned long long))];
};

struct lock_info {
	struct lock_info*       next;
	const char*             lib;
	void*                   lock;
	size_t                  mutex_id;
	size_t                  n_stack;
	size_t                  owner_id;

	char pad0[pad_to_cache_line(3*sizeof(void*) + 3*sizeof(size_t))];

	struct lock_info_thread per_threads[MAX_THREADS];
	void*                   stack[MAX_STACK];
};

struct hash_table {
	size_t                     n_hash;
	struct lock_info* volatile table[N_HASH];
};

static __thread size_t thread_id;
static __thread int    recurse = 1;

static pthread_mutex_t global_mutex;

static size_t          max_thread_id = 1;

struct self_id_assoc {
	size_t            thread_id;
	struct lock_info* lock_info;
	char pad[pad_to_cache_line(sizeof(size_t) + sizeof(void*))];
};

static struct self_id_assoc     self_id_assocs[MAX_THREADS];

static unsigned long long event_id; 
static int                inited = 0;

static size_t                  cur_mutex_id = 0;
static struct hash_table       table = { N_HASH };
static struct lock_info_thread info_threads[MAX_THREADS];
static struct nested           nesteds[MAX_THREADS];

static void echo(const char* msg, ...) {
	va_list va;
	va_start(va, msg);
	vfprintf(stderr, msg, va);
	va_end(va);
}

static void init_thread() {
	thread_id = __sync_fetch_and_add(&max_thread_id, 1);

	//echo("%lu %lu\n", (uintptr_t)&((struct lock_info*)0)->per_threads, sizeof(struct lock_info_thread));
	check_id(thread_id);

	PAPI_thread_init((unsigned long (*)(void))pthread_self);
}

static void init() {
	if(!__sync_val_compare_and_swap(&inited, 0, 1)) {
		LOAD_FUNC(pthread_mutex_init, 1);
		LOAD_FUNC(pthread_mutex_destroy, 1);
		LOAD_FUNC(zi_lock, 1);
		LOAD_FUNC(zi_trylock, 1);
		LOAD_FUNC(zi_unlock, 1);
#ifdef conds
		LOAD_FUNC_VERSIONED(pthread_cond_timedwait, 1, "GLIBC_2.3.2");
		LOAD_FUNC_VERSIONED(pthread_cond_wait, 1, "GLIBC_2.3.2");
//		LOAD_FUNC(pthread_cond_signal, 1);
//		LOAD_FUNC(pthread_cond_broadcast, 1);
#endif
		LOAD_FUNC(pthread_create, 1);
		LOAD_FUNC(exit, 1);
		LOAD_FUNC(_exit, 1);
		LOAD_FUNC(_Exit, 1);
		LOAD_FUNC(zi_liblock_exec, 0);
		LOAD_FUNC(zi_liblock_cond_wait, 0);
		LOAD_FUNC(zi_liblock_cond_timedwait, 0);
		LOAD_FUNC(zi_liblock_lock_init, 0);

		real_pthread_mutex_init(&global_mutex, 0);

		if (PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
			echo("WARNING: PAPI_library_init failed.\n");

		init_thread();

		const char* str_event_id = getenv("LOCK_PROFILE_EVENT");
		event_id = str_event_id ? strtol(str_event_id, 0, 0) : 0;
	}
}

/*
 *   hash table management
 */
static struct lock_info* ht_get(const char* lib, void* lock) {
	struct lock_info* volatile* entry   = &table.table[((uintptr_t)lock >> 4) % table.n_hash];
	struct lock_info*           attempt;
	struct lock_info*           cur;

	while(1) {
		attempt = *entry;

		for(cur=attempt; cur; cur=cur->next) {
			if(cur->lock == lock)
				return cur;
		}

		recurse = 0;

		struct lock_info* res = calloc(1, sizeof(struct lock_info));

		res->lock     = lock;
		res->lib      = lib ? lib : "<unknow>";
		res->next     = attempt;
		res->mutex_id = __sync_fetch_and_add(&cur_mutex_id, 1);

		if(__sync_val_compare_and_swap(entry, attempt, res) == attempt) {
			res->n_stack = backtrace(res->stack, MAX_STACK);
			recurse = 1;
			return res;
		} else {
			free(res);
			recurse = 1;
		}
	}
}

static void ht_foreach(void (*fct)(struct lock_info*)) {
	size_t i;
	struct lock_info* cur;

	for(i=0; i<table.n_hash; i++)
		for(cur=table.table[i]; cur; cur=cur->next)
			fct(cur);
}

void liblock_on_server_thread_start(const char* lib, unsigned int thread_id) {
	//printf("** on server thread start: %d\n", thread_id);
}

void liblock_on_server_thread_end(const char* lib, unsigned int thread_id) {
	//printf("** on server thread end: %d\n", thread_id);
}

void liblock_on_create(const char* lib, void* lock) {
	ht_get(lib, lock);
}

void liblock_on_destroy(const char* lib, void* lock) {
}

static void get_global_info(struct lock_info_thread* infos, double* p_in_cs) {
	size_t i;
	unsigned long long total = 0;
	unsigned long long in_cs = 0;

	for(i=0; i<max_thread_id; i++) {
		total += nesteds[i].real_last_cycle - nesteds[i].first_cycle;
		in_cs += infos[i].cycles_cs;
		//echo("thread %lu: %llu %llu %llu\n", i, total, infos[i].cycles_cs, in_cs);
	}

	//echo("---> %f %f %f %f\n", (double)total/1e9, (double)in_lock/1e9, (double)in_cs/1e9, (double)in_unlock/1e9);

	if(total) {
		*p_in_cs   = 100*(double)in_cs/(double)total;
	} else {
		*p_in_cs   = 0;
	}
}

static void print_lock_stack(struct lock_info* ginfo) {
	int    i;
	char** backtrace;

	backtrace = /*hack_*/backtrace_symbols(ginfo->stack, ginfo->n_stack);
	
	echo("%-20s mutex #%lu:\n", ginfo->lib, ginfo->mutex_id);
	for(i=2; i<(ginfo->n_stack-2); i++)
		echo("   %s\n", backtrace[i]);
	echo("\n");
}

static void print_lock_info(struct lock_info* info) {
	double in_cs;

	get_global_info(info->per_threads, &in_cs);
	echo("  %-20s #%-10lu %15.10f%%\n", info->lib, info->mutex_id, in_cs);
}

static void stop() {
	static int volatile already_stopped = 0;
	const char* str_is_full = getenv("LOCK_PROFILE_FULL");
	int is_full = str_is_full ? atoi(str_is_full) : 0;
	double in_cs;
	recurse = 0;
	
	if(__sync_val_compare_and_swap(&already_stopped, 0, 1))
		return;

	if(is_full > 1)
		ht_foreach(print_lock_stack);

	if(is_full) {
		echo("   Family              Mutex #     percentage in cs\n");
		ht_foreach(print_lock_info);
	}

	get_global_info(info_threads, &in_cs);

	echo("\nGlobal statistics: %2.2f%% in cs\n", in_cs);

}

/*
 *    hooks
 */
int zi_liblock_lock_init(const char* type, struct core* core, liblock_lock_t* lock, void* arg) {
	int res;

	if(!inited)
		init();

	res = real(zi_liblock_lock_init)(type, core, lock, arg);

	liblock_on_create(type, lock);

	return res;
}

void liblock_rcl_execute_op_for(liblock_lock_t* lock, size_t id) {
    // ht_get(0, lock)->owner_id = self_id_to_thread_id[id];
	// self_id_assocs[id].lock_info->owner_id = self_id_assocs[id].thread_id;
}

void* zi_liblock_exec(liblock_lock_t* lock, void* (*pending)(void*), void* val) {
	if(!inited)
		init();

	if(recurse) {
		struct nested* nested          = &nesteds[thread_id];
		struct lock_info* info         = ht_get(0, lock);
		struct lock_info_thread* linfo = &info->per_threads[thread_id];
		struct lock_info_thread* ginfo = &info_threads[thread_id];
		unsigned long long before, after;
		void* res;

		before = get_cyc();
		nested->real_last_cycle = before;
		
		if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;

		self_id_assocs[self.id].thread_id = thread_id;
		self_id_assocs[self.id].lock_info = info;

		res = real(zi_liblock_exec)(lock, pending, val);

		nested->depth--;

		after = get_cyc();
		nested->real_last_cycle = after;
		
        //printf("%llu %p- %p - %lld - %lld - %lld\n", nested->depth, lock, info, before, after, after - before);
		
		linfo->cycles_cs += after - before;
		if(!nested->depth)
			ginfo->cycles_cs += after - before;
		
		return res;
	} else {
		return real(zi_liblock_exec)(lock, pending, val);
	}
}

int zi_liblock_cond_wait(liblock_cond_t* cond, liblock_lock_t* lock)
{
	if(!inited)
		init();

	int res;
	size_t owner_id = thread_id;

	if (recurse) {
		struct lock_info* info         = ht_get(0, lock);
		owner_id                = info->owner_id;
		struct lock_info_thread* linfo = &info->per_threads[owner_id];
		struct nested* nested          = &nesteds[owner_id];
		struct lock_info_thread* ginfo = &info_threads[owner_id];
		unsigned long long after, last;

		last = nested->last_cycle[--nested->depth];
		after = get_cyc();
		nested->real_last_cycle = after;

		linfo->cycles_cs += after - last;
		if(!nested->depth)
			ginfo->cycles_cs += after - last;
    }

	//real(zi_lock)(&global_mutex);
	//zi_unlock(mutex);
	res = real(zi_liblock_cond_wait)(cond, /*&global_mutex*/lock);
	//real(zi_unlock)(&global_mutex);
	//zi_lock(mutex);

	if (recurse)
			{
			struct nested* nested          = &nesteds[owner_id];

		unsigned long long before;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;
	} 

	return res;

}

int zi_liblock_cond_timedwait(liblock_cond_t* cond, liblock_lock_t* lock, struct timespec* abstime)
{
   	if(!inited)
        init(); 

	int res;
	size_t owner_id = thread_id;

    if (recurse)
    {
		struct lock_info* info         = ht_get(0, lock);
		owner_id                = info->owner_id;
		struct lock_info_thread* linfo = &info->per_threads[owner_id];
		struct nested* nested          = &nesteds[owner_id];
		struct lock_info_thread* ginfo = &info_threads[owner_id];
		unsigned long long after, last;

		last = nested->last_cycle[--nested->depth];
        after = get_cyc();
        nested->real_last_cycle = after;

		linfo->cycles_cs += after - last;
		if(!nested->depth)
			ginfo->cycles_cs += after - last;
    }

	//real(zi_lock)(&global_mutex);
	//zi_unlock(mutex);
	res = real(zi_liblock_cond_timedwait)(cond, /*&global_mutex*/lock, abstime);
	//real(zi_unlock)(&global_mutex);
	//zi_lock(mutex);

    if (recurse)
    {
		struct nested* nested          = &nesteds[owner_id];

		unsigned long long before;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;
/*
		before = get_cyc();
        nested->real_last_cycle = before;
		
        if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;
*/	} 

	return res;

}

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
	int res;

	if(!inited)
		init();

	res = real_pthread_mutex_init(mutex, attr);

	liblock_on_create(posix_id, mutex);

	return res;
}

int zi_lock(pthread_mutex_t* mutex) {
	if(!inited)
		init();

	//echo("%d: zi_lock: %p\n", self.id, mutex);

	if(recurse) {
		struct nested* nested          = &nesteds[thread_id];

		unsigned long long before;
		int res;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		res = real(zi_lock)(mutex);

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;

		return res;
	} else {
		return real(zi_lock)(mutex);
	}
}

int zi_trylock(pthread_mutex_t* mutex) {
	if(!inited)
		init();

	if(recurse) {
		struct nested* nested          = &nesteds[thread_id];

		unsigned long long before;
		int res;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		res = real(zi_trylock)(mutex);

		if(res)
			return res;

		nested->last_cycle[nested->depth++] = before;
		nested->real_last_cycle = before;
		
        return res;
	} else {
		return real(zi_trylock)(mutex);
	}
}

int zi_unlock(pthread_mutex_t* mutex) {
	if(!inited)
		init();

	//echo("%d: zi_unlock: %p\n", self.id, mutex);

	if(recurse) {
		struct lock_info* info         = ht_get(posix_id, mutex);
		struct lock_info_thread* linfo = &info->per_threads[thread_id];
		struct nested* nested          = &nesteds[thread_id];
		struct lock_info_thread* ginfo = &info_threads[thread_id];
		unsigned long long after, last;
		int res;

		res = real(zi_unlock)(mutex);

		last = nested->last_cycle[--nested->depth];
        after = get_cyc();
        nested->real_last_cycle = after;

		linfo->cycles_cs += after - last;
		if(!nested->depth)
			ginfo->cycles_cs += after - last;

		return res;
	} else
		return real(zi_unlock)(mutex);
}

#ifdef conds
int pthread_cond_timedwait(pthread_cond_t* cond, pthread_mutex_t* mutex, const struct timespec* abstime) {
	if(!inited)
        init(); 

	int res;

    if (recurse)
    {
		struct lock_info* info         = ht_get(posix_id, mutex);
		struct lock_info_thread* linfo = &info->per_threads[thread_id];
		struct nested* nested          = &nesteds[thread_id];
		struct lock_info_thread* ginfo = &info_threads[thread_id];
		unsigned long long after, last;

		last = nested->last_cycle[--nested->depth];
        after = get_cyc();
        nested->real_last_cycle = after;

		linfo->cycles_cs += after - last;
		if(!nested->depth)
			ginfo->cycles_cs += after - last;
    }

	//real(zi_lock)(&global_mutex);
	//zi_unlock(mutex);
	res = real_pthread_cond_timedwait(cond, /*&global_mutex*/mutex, abstime);
	//real(zi_unlock)(&global_mutex);
	//zi_lock(mutex);

    if (recurse)
    {
		struct nested* nested          = &nesteds[thread_id];

		unsigned long long before;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;
	} 

	return res;
}

int pthread_cond_wait(pthread_cond_t* cond, pthread_mutex_t* mutex) {
	if(!inited)
        init();

	int res;

    if (recurse)
    {
		struct lock_info* info         = ht_get(posix_id, mutex);
		struct lock_info_thread* linfo = &info->per_threads[thread_id];
		struct nested* nested          = &nesteds[thread_id];
		struct lock_info_thread* ginfo = &info_threads[thread_id];
		unsigned long long after, last;

		last = nested->last_cycle[--nested->depth];
        after = get_cyc();
        nested->real_last_cycle = after;

		linfo->cycles_cs += after - last;
		if(!nested->depth)
			ginfo->cycles_cs += after - last;
    }

	//real(zi_lock)(&global_mutex);
	//zi_unlock(mutex);
	res = real_pthread_cond_wait(cond, /*&global_mutex*/mutex);
	//real(zi_unlock)(&global_mutex);
	//zi_lock(mutex);

    if (recurse)
    {
		struct nested* nested          = &nesteds[thread_id];

		unsigned long long before;

		if(nested->depth >= MAX_NESTED) {
			echo("too many nested");
			exit(42);
		}

		before = get_cyc();

		if(!nested->first_cycle)
			nested->first_cycle = before;

		nested->last_cycle[nested->depth++] = before;
        nested->real_last_cycle = before;
	} 

	return res;
}

/*
int pthread_cond_broadcast(pthread_cond_t *cond) {
	//init();

	int res;

	//real(zi_lock)(&global_mutex);
	res = real_pthread_cond_broadcast(cond);
	//real(zi_unlock)(&global_mutex);

	return res;
}

int pthread_cond_signal(pthread_cond_t *cond) {
	//init();

	int res;

	//real(zi_lock)(&global_mutex);
	res = real_pthread_cond_signal(cond);
	//real(zi_unlock)(&global_mutex);

	return res;
}
*/
#endif

struct routine {
	void* (*fct)(void*);
	void* arg;
};

static void* my_start_routine(void* _arg) {
	struct routine* r = _arg;
	void* (*fct)(void*) = r->fct;
	void* arg = r->arg;
	void* res;

	free(r);

	init_thread();

	res = fct(arg);

	return res;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg) {
	struct routine* r = malloc(sizeof(struct routine));
	r->fct = start_routine;
	r->arg = arg;
	return real_pthread_create(thread, attr, my_start_routine, r);
}

static __attribute__ ((destructor)) void destroy() {
	stop();
}

static __attribute ((constructor (1000))) void start() {
	init();
}

__attribute__((noreturn))  void exit(int status) {
	stop();
	real_exit(status);
}

__attribute__((noreturn)) void _exit(int status) {
	stop();
	real__exit(status);
}

__attribute__((noreturn)) void _Exit(int status) {
	stop();
	real__Exit(status);
}



