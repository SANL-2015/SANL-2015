/*
 * opt_util.h
 *
 *  Created on: 2013-2-22
 *      Author: wayne
 */

#ifndef OPT_UTIL_H_
#define OPT_UTIL_H_

#include <sys/mman.h>
#include <errno.h>
#include <ucontext.h>

#define CACHE_LINE_SIZE 64
#define STACK_SIZE            r_align(1024*1024, PAGE_SIZE)

#define r_align(n, r)        (((n) + (r) - 1) & -(r))
#define cache_align(n)       r_align(n , CACHE_LINE_SIZE)
#define pad_to_cache_line(n) (cache_align(n) - (n))

struct mini_thread_t {
	ucontext_t context; /* context of the mini thread */
	void* stack;
};

typedef struct mini_thread_t mini_thread;

struct native_thread_t {
	ucontext_t initial_context; /* initial context of the thread */
};

typedef struct native_thread_t native_thread;



#endif /* OPT_UTIL_H_ */
