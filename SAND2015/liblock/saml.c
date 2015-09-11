#include <stdint.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <ucontext.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <numa.h>
#include <assert.h>
#include <linux/futex.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "liblock.h"
#include "liblock-fatal.h"
//#include "fqueue.h"

#include <papi.h>

/*
 *      constants
 */
#define COND_DEAL -1
#define COND_DONE 0

#define SERVER_DOWN     0
#define SERVER_UP       1

#define MAX_THREAD		256

#define MAX_WAITING_TIME 100000
#define MAX_SERVING_TIME MAX_WAITING_TIME

int counter_bound = 1000;

/*
 * Default SAML instance maximum number in evaluation
 */
#define NB_POOLSIZE		20

#define mb()    asm volatile("mfence":::"memory")
#define likely(x) __builtin_expect(!!(x),1)
#define unlikely(x) __builtin_expect(!!(x),0)

/*
 * NODE topology definition
 * For a machine with four 10-core intel Xeon cpu
 * Can be detected automatically in do_liblock_init_lock(saml)
 */
#define NODE_NUM		4

int node_distance[4][4] = { { 10, 20, 20, 20 }, { 20, 10, 20, 20 }, { 20, 20,
		10, 20 }, { 20, 20, 20, 10 } };

/*
 *  structures
 */
struct request {
	void* volatile val; /* argument of the pending request */
	void* (* volatile pending)(void*); /* pending request or null if no pending request */
	char pad[pad_to_cache_line(2 * sizeof(void*))];
	int cond_wait;
	char pad2[pad_to_cache_line(sizeof(int))];
};

struct mcs_node {
	struct mcs_node* volatile next;
	int volatile spin;
	char __pad[pad_to_cache_line(sizeof(void*) + sizeof(int))];
};

struct liblock_profile {
	long cycles_b;
	long cycles_e;
	long lib_delay;
	long lib_exe;
	int spin_counter;
	int nonspin_counter;
	int nonnuma_counter;
	int numa_counter;
};

struct liblock_impl {
	pthread_mutex_t glock;
	char pad[pad_to_cache_line(sizeof(pthread_mutex_t))];
	struct server** servers; /* request arrays */
	char pad2[pad_to_cache_line(sizeof(struct server**))];
	int contention_num; /* Signal for sudden low contention */
	int spin_global_vote;
	int numa_global_vote;
	struct liblock_profile* profile_datas; /* profiling data */
	char pad3[pad_to_cache_line(
			3 * sizeof(int) + sizeof(struct liblock_profile*))];
	int spin_local_vote[MAX_THREAD];
	int numa_local_vote[MAX_THREAD];
	char pad4[pad_to_cache_line(2 * MAX_THREAD * sizeof(int))];
	struct mcs_node* volatile tail; /* Combined code-based lock */
	char pad5[pad_to_cache_line(sizeof(void*))];
	int node_lock[NODE_NUM]; /* NUMA-aware signal to check if a near core is a server */
	char pad6[pad_to_cache_line(NODE_NUM * sizeof(int))];

};

struct server {
	/* always used by the server in non blocked case, also private in this case */
	int volatile state; /* state of the server (running, starting...) */
	char pad0[pad_to_cache_line(sizeof(int))];

	/* always shared (in read) in non blocked case */
	struct core* core; /* core where the server run (read by client) */
	struct request* requests; /* the request array (read by client) */
	char pad1[pad_to_cache_line(2 * sizeof(void*))];
};

static struct server*** servers_pool = 0; /* array of array of server (one per core) */
static int servers_pool_index = 0;
static int thread_num = 0; /* global thread number using SAML */

__thread struct mcs_node* my_node_saml = 0;

/*
 *   Code-based lock API
 */
static void lock_mcs(struct liblock_impl* impl) {
	struct mcs_node *tail, *me = my_node_saml;

	me->next = 0;
	me->spin = 0;

	tail = __sync_lock_test_and_set(&impl->tail, me); //xchg(&impl->tail, me);

	/* No one there? */
	if (!tail)
		return;

	/* Someone there, need to link in */
	tail->next = me;

	/* Spin on my spin variable */
	while (!me->spin) {
		PAUSE();
	}

	return;
}

static int trylock_mcs(struct liblock_impl* impl) {
	struct mcs_node *tail, *me = my_node_saml;

	me->next = NULL;
	me->spin = 0;

	tail = __sync_val_compare_and_swap(&impl->tail, NULL, me);

	/* No one there? */
	if (!tail)
		return 0;

	return 1;
}

static void unlock_mcs(struct liblock_impl* impl) {
	struct mcs_node* me = my_node_saml;
	/* No successor yet? */
	if (!me->next) {
		/* Try to atomically unlock */
		if (__sync_val_compare_and_swap(&impl->tail, me, 0) == me)
			return;

		/* Wait for successor to appear */
		while (!me->next)
			PAUSE();
	}

	/* Unlock next one */
	me->next->spin = 1;
}

/*
 *   SAML API
 */
static int is_near_node(int node1, int node2) {
	if (node_distance[node1][node2] < 20) {
		return 1;
	}
	return 0;
}

static void* do_liblock_execute_operation(saml)(liblock_lock_t* lock,
		void* (*pending)(void*), void* val) {
	struct liblock_impl* impl = lock->impl;
	int core_id = self.running_core->core_id;
	int server_down_threshold = 1;
	double cs_ratio = 0;
	double cs_ratio_1 = 0;

	void* res;

	/* Collect execution info */
	impl->profile_datas[core_id].cycles_b = PAPI_get_real_cyc();

	impl->profile_datas[core_id].lib_delay =
			impl->profile_datas[core_id].cycles_b
					- impl->profile_datas[core_id].cycles_e;

	/* Compute server_down_threshold from server downgrading threshold adaptive function */
	if (impl->profile_datas[core_id].lib_exe != 0) {
		if (cs_ratio == 0) {
			cs_ratio = impl->profile_datas[core_id].lib_exe * 1.0
					/ impl->profile_datas[core_id].lib_delay;
			cs_ratio_1 = (1 / cs_ratio + cs_ratio_1) / 2;
		} else {
			cs_ratio = (cs_ratio
					+ impl->profile_datas[core_id].lib_exe * 1.0
							/ impl->profile_datas[core_id].lib_delay) / 2;
			cs_ratio_1 = (1 / cs_ratio + cs_ratio_1) / 2;
		}

		/* server downgrading threshold adaptive function for Xeon machine */
		if (cs_ratio_1 > 1 && cs_ratio_1 < 18) {
			server_down_threshold = -3.2026 * cs_ratio_1 * cs_ratio_1
					+ 125 * cs_ratio_1 - 172.81;
			if (server_down_threshold < 1)
				server_down_threshold = 1;
		} else if (cs_ratio_1 >= 18) {
			server_down_threshold = 10000;
		} else if (cs_ratio_1 <= 1 && cs_ratio_1 >= 0.2) {
			server_down_threshold = 10;
		} else {
			server_down_threshold = 1;
		}

		if (impl->spin_local_vote[self.id] == 0) {
			if (cs_ratio_1 >= 18) {
				impl->profile_datas[core_id].spin_counter++;
				if (impl->profile_datas[core_id].spin_counter > 10) {
					impl->spin_local_vote[self.id] = 1;
					__sync_fetch_and_add(&impl->spin_global_vote, 1);
					mb();
					impl->profile_datas[core_id].spin_counter = 0;
				}
			} else {
				if (impl->profile_datas[core_id].spin_counter > 0)
					impl->profile_datas[core_id].spin_counter--;
			}
		} else {
			if (cs_ratio_1 < 5) {
				impl->profile_datas[core_id].nonspin_counter++;
				if (impl->profile_datas[core_id].nonspin_counter
						> counter_bound) {
					impl->spin_local_vote[self.id] = 0;
					__sync_fetch_and_add(&impl->spin_global_vote, -1);
					mb();
					impl->profile_datas[core_id].nonspin_counter = 0;
				}
			} else {
				if (impl->profile_datas[core_id].nonspin_counter > 0)
					impl->profile_datas[core_id].nonspin_counter--;
			}
		}

		/* tuning for counter_bound */
		if ((cs_ratio_1 > 15 && impl->spin_local_vote[self.id] == 0)
				|| (cs_ratio_1 > 5 && impl->spin_local_vote[self.id] == 1)) {
			if (counter_bound < 10000000)
				counter_bound *= 2;
		} else {
			if (counter_bound > 100)
				counter_bound /= 2;
		}

		if (impl->numa_local_vote[self.id] == 1) {
			if (cs_ratio_1 >= 0.8) {
				impl->profile_datas[core_id].nonnuma_counter++;
				if (impl->profile_datas[core_id].nonnuma_counter > 10) {
					impl->numa_local_vote[self.id] = 0;
					__sync_fetch_and_add(&impl->numa_global_vote, 1);
					mb();
					impl->profile_datas[core_id].nonnuma_counter = 0;
				}
			} else {
				if (impl->profile_datas[core_id].nonnuma_counter > 0)
					impl->profile_datas[core_id].nonnuma_counter--;
			}
		} else {
			if (cs_ratio_1 < 1) {
				impl->profile_datas[core_id].numa_counter++;
				if (impl->profile_datas[core_id].numa_counter > 5000) {
					impl->numa_local_vote[self.id] = 1;
					__sync_fetch_and_add(&impl->numa_global_vote, -1);
					mb();
					impl->profile_datas[core_id].numa_counter = 0;
				}
			} else {
				if (impl->profile_datas[core_id].numa_counter > 0)
					impl->profile_datas[core_id].numa_counter--;
			}
		}

	}

	/* Adaptation between code-based and migration modes */
	if ((impl->spin_global_vote > (thread_num - 5) / 2)
			|| (thread_num - 1) < 4) {
		lock_mcs(impl);

		res = pending(val);

		unlock_mcs(impl);

		impl->profile_datas[core_id].cycles_e = PAPI_get_real_cyc();
		impl->profile_datas[core_id].lib_exe =
				(impl->profile_datas[core_id].lib_exe == 0) ?
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b) :
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b
								+ impl->profile_datas[core_id].lib_exe) / 2;
		return res;
	}

	/* Optimization for sudden low contention */
	if (impl->contention_num == 0) {
		if (__sync_fetch_and_add(&impl->contention_num, 1) != 1) {
			__sync_fetch_and_add(&impl->contention_num, -1);
			goto retry_server;
		}
		while (trylock_mcs(impl)) {
			if (impl->contention_num != 1) {
				__sync_fetch_and_add(&impl->contention_num, -1);
				goto retry_server;
			}
		}

		res = pending(val);
		unlock_mcs(impl);
		__sync_fetch_and_add(&impl->contention_num, -1);

		impl->profile_datas[core_id].cycles_e = PAPI_get_real_cyc();
		impl->profile_datas[core_id].lib_exe =
				(impl->profile_datas[core_id].lib_exe == 0) ?
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b) :
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b
								+ impl->profile_datas[core_id].lib_exe) / 2;
		return res;
	}

	int client_wait_time = 0;

	retry_server:

	/* Dynamic server nomination */
	if (!self.isclient && ((struct server*) lock->r0)->state == SERVER_DOWN
			&& !trylock_mcs(impl)) {
		reget_server1:

		__sync_fetch_and_add(&impl->contention_num, 1);

		res = pending(val);

		reget_server2:

		impl->profile_datas[core_id].cycles_e = PAPI_get_real_cyc();
		impl->profile_datas[core_id].lib_exe =
				(impl->profile_datas[core_id].lib_exe == 0) ?
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b) :
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b
								+ impl->profile_datas[core_id].lib_exe) / 2;

		struct server* server;

		/* Stop former server */
		if (lock->r0 != impl->servers[self.running_core->core_id]) {
			server = lock->r0;
			server->state = SERVER_DOWN;
			lock->r0 = impl->servers[self.running_core->core_id];
			mb();
		}

		server = lock->r0;

		server->state = SERVER_UP;

		struct request* request, *last;
		void* (*pending_r)(void*);
		pending_r = 0;
		int req_num = 0;
		int waiting_loop_num = 0;
		int server_execution_times = 0;

		/* Iteratively serve requests of clients */
		while (server->state == SERVER_UP) {
			last = &server->requests[id_manager.first_free];

			for (request = &server->requests[id_manager.first]; request < last;
					request++) {
				pending_r = request->pending;

				if (pending_r) {
					request->cond_wait = COND_DEAL;
					request->val = pending_r(request->val);

					request->pending = NULL;
					request->cond_wait = COND_DONE;
					req_num++;
				}
			}

			/* Check request density */
			if (req_num) {
				req_num = 0;
				waiting_loop_num--;
			} else {
				waiting_loop_num++;
				if (waiting_loop_num > server_down_threshold) {
					server->state = SERVER_DOWN;
				}
			}

			/* Check serving time */
			server_execution_times++;
			if (unlikely(server_execution_times > MAX_SERVING_TIME)) {
				server->state = SERVER_DOWN;
			}

		}

		unlock_mcs(impl);
		__sync_fetch_and_add(&impl->contention_num, -1);

		return res;
	} else {
		/* client side */
		PAUSE();

		int self_node_id = self.running_core->node->node_id;
		int server_node_id = 0;

		while (impl->numa_local_vote[self.id]
				&& (40 - impl->numa_global_vote) > (thread_num / 2)
				&& ((struct server*) lock->r0)->state == SERVER_UP
				&& !is_near_node(
						((struct server*) lock->r0)->core->node->node_id,
						self_node_id)) {
			server_node_id =
					impl->node_lock[((struct server*) lock->r0)->core->node->node_id];

			client_wait_time++;
			if (unlikely(client_wait_time > MAX_WAITING_TIME)) {
				if (__sync_val_compare_and_swap(
						&impl->node_lock[server_node_id], 0, 1) == 0) {
					self.isclient = 0;
					lock_mcs(impl);
					impl->node_lock[server_node_id] = 0;
					goto reget_server1;
				} else {
					client_wait_time = 0;
				}

			}
			PAUSE();
		}

		if (((struct server*) lock->r0)->state == SERVER_DOWN) {
			self.isclient = 0;
			goto retry_server;
		}

		struct server* server;
		struct request* req;
		//refind_server:

		server = lock->r0;
		req = &(server->requests[self.id]);

		req->val = val;
		req->pending = pending;

		while (req->pending) {
			client_wait_time++;
			if (unlikely(client_wait_time > MAX_WAITING_TIME)) {
				if (server != lock->r0 && (req->cond_wait != COND_DEAL)) {
					mb();
					if (((struct server*) lock->r0)->state == SERVER_DOWN) {
						if (((struct server*) lock->r0)->state == SERVER_DOWN
								&& !trylock_mcs(impl)) {
							self.isclient = 0;
							if (req->pending) {
								req->pending = NULL;

								goto reget_server1;
							} else {
								__sync_fetch_and_add(&impl->contention_num, 1);
								res = req->val;
								goto reget_server2;
							}
						}
					}
				} else if (server->state == SERVER_DOWN
						&& (req->cond_wait != COND_DEAL)) {

					mb();

					if (((struct server*) lock->r0)->state == SERVER_DOWN
							&& !trylock_mcs(impl)) {
						self.isclient = 0;
						if (req->pending) {
							req->pending = NULL;

							goto reget_server1;
						} else {
							__sync_fetch_and_add(&impl->contention_num, 1);
							res = req->val;

							goto reget_server2;
						}
					}
				}
			}

			if (unlikely(client_wait_time > MAX_WAITING_TIME)) {
				mb();
			}

			PAUSE();
		}
		self.isclient = 1;

		impl->profile_datas[core_id].cycles_e = PAPI_get_real_cyc();
		impl->profile_datas[core_id].lib_exe =
				(impl->profile_datas[core_id].lib_exe == 0) ?
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b) :
						(impl->profile_datas[core_id].cycles_e
								- impl->profile_datas[core_id].cycles_b
								+ impl->profile_datas[core_id].lib_exe) / 2;
		return req->val;
	}
}

static void destroy_server(struct server* server) {
	if (server->state == SERVER_UP) {
		server->state = SERVER_DOWN;
	}
}

static int do_liblock_cond_signal(saml)(liblock_cond_t* cond) {
	return pthread_cond_signal(&cond->impl.posix_cond);
}

static int do_liblock_cond_broadcast(saml)(liblock_cond_t* cond) {
	return pthread_cond_broadcast(&cond->impl.posix_cond);
}

static int do_liblock_cond_timedwait(saml)(liblock_cond_t* cond,
		liblock_lock_t* lock, const struct timespec* ts) {
	struct liblock_impl* impl = lock->impl;

	struct server* server = lock->r0;

	pthread_mutex_lock(&impl->glock);

	server->state = SERVER_DOWN;

	unlock_mcs(impl);

	int res = pthread_cond_timedwait(&cond->impl.posix_cond, &impl->glock, ts);

	pthread_mutex_unlock(&impl->glock);

	lock_mcs(impl);

	server->state = SERVER_UP;

	if (lock->r0 != &impl->servers[self.running_core->core_id]) {
		server = lock->r0;
		server->state = SERVER_DOWN;
		lock->r0 = impl->servers[self.running_core->core_id];
		mb();
	}

	return res;
}

static int do_liblock_cond_wait(saml)(liblock_cond_t* cond,
		liblock_lock_t* lock) {
	struct liblock_impl* impl = lock->impl;

	struct server* server = lock->r0;

	pthread_mutex_lock(&impl->glock);

	server->state = SERVER_DOWN;

	unlock_mcs(impl);

	int res = pthread_cond_wait(&cond->impl.posix_cond, &impl->glock);

	pthread_mutex_unlock(&impl->glock);

	lock_mcs(impl);

	server->state = SERVER_UP;

	if (lock->r0 != &impl->servers[self.running_core->core_id]) {
		server = lock->r0;
		server->state = SERVER_DOWN;
		lock->r0 = impl->servers[self.running_core->core_id];
		mb();
	}
	return res;
}

static int do_liblock_cond_init(saml)(liblock_cond_t* cond) {
	return cond->has_attr ?
			pthread_cond_init(&cond->impl.posix_cond, &cond->attr) :
			pthread_cond_init(&cond->impl.posix_cond, 0);
}

static int do_liblock_cond_destroy(saml)(liblock_cond_t* cond) {
	return pthread_cond_destroy(&cond->impl.posix_cond);
}

static void do_liblock_unlock_in_cs(saml)(liblock_lock_t* lock) {
}

static void do_liblock_relock_in_cs(saml)(liblock_lock_t* lock) {
}

static void liblock_init_lock_servers(struct liblock_impl* impl) {
	int pool_index = __sync_fetch_and_add(&servers_pool_index, 1);

	if (pool_index > NB_POOLSIZE) {
		fatal("SAML instance number overflow");
	}

	impl->servers = servers_pool[pool_index];
}

static void liblock_init_lock_profile_datas(struct liblock_impl* impl) {
	int i;

	impl->profile_datas = liblock_allocate(
			sizeof(struct liblock_profile) * topology->nb_cores);

	struct liblock_profile* init_datas = impl->profile_datas;

	for (i = 0; i < topology->nb_cores; i++) {
		init_datas[i].cycles_b = 0;
		init_datas[i].cycles_e = 0;
		init_datas[i].lib_delay = 0;
		init_datas[i].lib_exe = 0;
		init_datas[i].spin_counter = 0;
		init_datas[i].nonspin_counter = 0;
		init_datas[i].nonnuma_counter = 0;
		init_datas[i].numa_counter = 0;
	}
}

static struct liblock_impl* do_liblock_init_lock(saml)(liblock_lock_t* lock,
		struct core* core, pthread_mutexattr_t* attr) {
	struct liblock_impl* impl = liblock_allocate(sizeof(struct liblock_impl));

	liblock_init_lock_servers(impl);
	struct server* server = impl->servers[core->core_id];

	lock->r0 = server;

	liblock_init_lock_profile_datas(impl);
	impl->tail = 0;
	/* default non-NUMA migration mode */
	impl->spin_global_vote = 0;
	impl->numa_global_vote = 40;

	int i = 0;

	for (; i < NODE_NUM; i++) {
		impl->node_lock[i] = 0;
	}

	i = 0;

	for (; i < MAX_THREAD; i++) {
		impl->spin_local_vote[i] = 0;
		impl->numa_local_vote[i] = 0;
	}

	pthread_mutex_init(&impl->glock, NULL);

	return impl;
}

static int do_liblock_destroy_lock(saml)(liblock_lock_t* lock) {
	struct server* server = lock->r0;

	destroy_server(server);

	return 0;
}

static void do_liblock_run(saml)(void (*callback)()) {
	callback();
}

static void do_liblock_declare_server(saml)(struct core* core) {
}

static void do_liblock_on_thread_exit(saml)(struct thread_descriptor* desc) {
	__sync_fetch_and_add(&thread_num, -1);
	munmap(my_node_saml, r_align(sizeof(struct mcs_node), PAGE_SIZE));
}

static void do_liblock_on_thread_start(saml)(struct thread_descriptor* desc) {
	__sync_fetch_and_add(&thread_num, 1);
	my_node_saml = anon_mmap(r_align(sizeof(struct mcs_node), PAGE_SIZE));
}

static void do_liblock_init_library(saml)() {
	servers_pool = liblock_allocate(sizeof(struct server**) * NB_POOLSIZE);
	int pool_i;
	int i;
	for (pool_i = 0; pool_i < NB_POOLSIZE; pool_i++) {
		servers_pool[pool_i] = liblock_allocate(
				sizeof(struct server*) * topology->nb_cores);

		struct server** init_servers = servers_pool[pool_i];

		for (i = 0; i < topology->nb_cores; i++) {
			struct core* core = &topology->cores[i];
			int cid = core->core_id;
			size_t request_size = r_align(sizeof(struct request) * MAX_THREAD,
					PAGE_SIZE);
			size_t server_size = r_align(sizeof(struct server), PAGE_SIZE);
			void* ptr = anon_mmap_huge(request_size + server_size);

			memset(ptr, 0, request_size + server_size);
			((uint64_t *) ptr)[0] = 0; // To avoid a page fault later.

			init_servers[cid] = ptr + request_size;
			init_servers[cid]->core = core;

			init_servers[cid]->state = SERVER_DOWN;
			init_servers[cid]->requests = ptr;
		}

	}
}

static void do_liblock_kill_library(saml)() {
	fatal("implement me");
}

liblock_declare(saml);
