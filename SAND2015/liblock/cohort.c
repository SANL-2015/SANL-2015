/*
 * cohort.c
 *
 *  Created on: Oct 30, 2014
 *      Author: wayne
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include "ticket_lock.h"
#include "liblock.h"
#include "mcs_lock2.h"
#include "liblock-fatal.h"

#define NODE_NUM		4

struct liblock_impl {
	pthread_mutex_t posix_lock;
	ticketlock glock;
	mcs_lock llock[NODE_NUM];
	char pad[pad_to_cache_line(
			sizeof(pthread_mutex_t) + sizeof(ticketlock) + sizeof(mcs_lock) * 4)];
};

static struct liblock_impl* do_liblock_init_lock(cohort)(liblock_lock_t* lock,
		struct core* server, pthread_mutexattr_t* attr) {
	int i;
	struct liblock_impl* impl = liblock_allocate(sizeof(struct liblock_impl));

	impl->glock.u = 0;

	for (i = 0; i < NODE_NUM; i++) {
		impl->llock[i] = NULL;
	}

	pthread_mutex_init(&impl->posix_lock, 0);

	return impl;
}

static int do_liblock_destroy_lock(cohort)(liblock_lock_t* lock) {
	pthread_mutex_destroy(&lock->impl->posix_lock);
	return 0;
}

__thread mcs_lock_t my_node_c;

static void* do_liblock_execute_operation(cohort)(liblock_lock_t* lock,
		void* (*pending)(void*), void* val) {
	struct liblock_impl* impl = lock->impl;
	void* res;

	int node_id = self.running_core->node->node_id;

	if (!lock_mcs(&impl->llock[node_id], &my_node_c)) {
		ticket_lock(&impl->glock);

		res = pending(val);

		if (!unlock_mcs(&impl->llock[node_id], &my_node_c)) {
			ticket_unlock(&impl->glock);
		}
	} else {
		res = pending(val);

		if (!unlock_mcs(&impl->llock[node_id], &my_node_c)) {
			ticket_unlock(&impl->glock);
		}
	}

	return res;
}

static void do_liblock_init_library(cohort)() {
}

static void do_liblock_kill_library(cohort)() {
}

static void do_liblock_run(cohort)(void (*callback)()) {
	if (__sync_val_compare_and_swap(&liblock_start_server_threads_by_hand, 1, 0)
			!= 1)
		fatal("servers are not managed by hand");
	if (callback)
		callback();
}

static int do_liblock_cond_init(cohort)(liblock_cond_t* cond) {
	return cond->has_attr ?
			pthread_cond_init(&cond->impl.posix_cond, &cond->attr) :
			pthread_cond_init(&cond->impl.posix_cond, 0);
}

static int cond_timedwait(liblock_cond_t* cond, liblock_lock_t* lock,
		const struct timespec* ts) {
	struct liblock_impl* impl = lock->impl;
	int res;

	pthread_mutex_lock(&impl->posix_lock);
	ticket_unlock(&impl->glock);
	if (ts)
		res = pthread_cond_timedwait(&cond->impl.posix_cond, &impl->posix_lock,
				ts);
	else
		res = pthread_cond_wait(&cond->impl.posix_cond, &impl->posix_lock);
	pthread_mutex_unlock(&impl->posix_lock);

	ticket_lock(&impl->glock);

	return res;
}

static int do_liblock_cond_timedwait(cohort)(liblock_cond_t* cond,
		liblock_lock_t* lock, const struct timespec* ts) {
	return cond_timedwait(cond, lock, ts);
}

static int do_liblock_cond_wait(cohort)(liblock_cond_t* cond,
		liblock_lock_t* lock) {
	return cond_timedwait(cond, lock, 0);
}

static int do_liblock_cond_signal(cohort)(liblock_cond_t* cond) {
	return pthread_cond_signal(&cond->impl.posix_cond);
}

static int do_liblock_cond_broadcast(cohort)(liblock_cond_t* cond) {
	return pthread_cond_broadcast(&cond->impl.posix_cond);
}

static int do_liblock_cond_destroy(cohort)(liblock_cond_t* cond) {
	return pthread_cond_destroy(&cond->impl.posix_cond);
}

static void do_liblock_on_thread_exit(cohort)(struct thread_descriptor* desc) {
}

static void do_liblock_on_thread_start(cohort)(struct thread_descriptor* desc) {
}

static void do_liblock_unlock_in_cs(cohort)(liblock_lock_t* lock) {
	struct liblock_impl* impl = lock->impl;
	ticket_unlock(&impl->glock);
}

static void do_liblock_relock_in_cs(cohort)(liblock_lock_t* lock) {
}

static void do_liblock_declare_server(cohort)(struct core* core) {
}

liblock_declare(cohort);

