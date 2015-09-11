/* ########################################################################## */
/* (C) UPMC, 2010-2011                                                        */
/*     Authors:                                                               */
/*       Jean-Pierre Lozi <jean-pierre.lozi@lip6.fr>                          */
/*       Gaël Thomas <gael.thomas@lip6.fr>                                    */
/*       Florian David <florian.david@lip6.fr>                                */
/*       Julia Lawall <julia.lawall@lip6.fr>                                  */
/*       Gilles Muller <gilles.muller@lip6.fr>                                */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
#include <string.h>
#include <errno.h>
#include "k42.h"
#include "liblock.h"
#include "liblock-fatal.h"

struct liblock_impl {
	pthread_mutex_t       posix_lock;
	k42lock lock;
	char                  pad[pad_to_cache_line(sizeof(pthread_mutex_t) + sizeof(k42lock))];
};

static struct liblock_impl* do_liblock_init_lock(k42)(liblock_lock_t* lock, struct core* server, pthread_mutexattr_t* attr) {
	struct liblock_impl* impl = liblock_allocate(sizeof(struct liblock_impl));
	impl->lock.next = NULL;
	impl->lock.tail = NULL;
	pthread_mutex_init(&impl->posix_lock, 0);

	return impl;
}

static int do_liblock_destroy_lock(k42)(liblock_lock_t* lock) {
	pthread_mutex_destroy(&lock->impl->posix_lock);
	return 0;
}

static void* do_liblock_execute_operation(k42)(liblock_lock_t* lock, void* (*pending)(void*), void* val) {
	struct liblock_impl* impl = lock->impl;
	void* res;
	k42_lock(&impl->lock);

	res = pending(val);

	k42_unlock(&impl->lock);

	return res;
}

static void do_liblock_init_library(k42)() {
}

static void do_liblock_kill_library(k42)() {
}

static void do_liblock_run(k42)(void (*callback)()) {
	if(__sync_val_compare_and_swap(&liblock_start_server_threads_by_hand, 1, 0) != 1)
		fatal("servers are not managed by hand");
	if(callback)
		callback();
}

static int do_liblock_cond_init(k42)(liblock_cond_t* cond) {
	return cond->has_attr ?
		pthread_cond_init(&cond->impl.posix_cond, &cond->attr) :
		pthread_cond_init(&cond->impl.posix_cond, 0);
}

static int cond_timedwait(liblock_cond_t* cond, liblock_lock_t* lock, const struct timespec* ts) {
	struct liblock_impl* impl = lock->impl;
	int res;

	pthread_mutex_lock(&impl->posix_lock);
	k42_unlock(&impl->lock);
	if(ts)
		res = pthread_cond_timedwait(&cond->impl.posix_cond, &impl->posix_lock, ts);
	else
		res = pthread_cond_wait(&cond->impl.posix_cond, &impl->posix_lock);
	pthread_mutex_unlock(&impl->posix_lock);

	k42_lock(&impl->lock);

	return res;
}

static int do_liblock_cond_timedwait(k42)(liblock_cond_t* cond, liblock_lock_t* lock, const struct timespec* ts) {
	return cond_timedwait(cond, lock, ts);
}

static int do_liblock_cond_wait(k42)(liblock_cond_t* cond, liblock_lock_t* lock) {
	return cond_timedwait(cond, lock, 0);
}

static int do_liblock_cond_signal(k42)(liblock_cond_t* cond) {
	return pthread_cond_signal(&cond->impl.posix_cond);
}

static int do_liblock_cond_broadcast(k42)(liblock_cond_t* cond) {
	return pthread_cond_broadcast(&cond->impl.posix_cond);
}

static int do_liblock_cond_destroy(k42)(liblock_cond_t* cond) {
	return pthread_cond_destroy(&cond->impl.posix_cond);
}

static void do_liblock_on_thread_exit(k42)(struct thread_descriptor* desc) {
}

static void do_liblock_on_thread_start(k42)(struct thread_descriptor* desc) {
}

static void do_liblock_unlock_in_cs(k42)(liblock_lock_t* lock) {
	struct liblock_impl* impl = lock->impl;
	k42_unlock(&impl->lock);
}

static void do_liblock_relock_in_cs(k42)(liblock_lock_t* lock) {
}

static void do_liblock_declare_server(k42)(struct core* core) {
}

liblock_declare(k42);

