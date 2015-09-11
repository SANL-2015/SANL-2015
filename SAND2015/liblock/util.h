/*
 * util.h
 *
 *  Created on: 2012-12-11
 *      Author: wayne
 */

#ifndef UTIL_H_
#define UTIL_H_

#ifndef EBUSY
#define EBUSY 1
#endif

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg_util(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1)
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */
#define cpu_relax() asm volatile("pause\n": : :"memory")



/* Atomic exchange (of various sizes) */
static inline void *xchg_64(void *ptr, void *x)
{
	__asm__ __volatile__("xchgq %0,%1"
				:"=r" ((unsigned long long) x)
				:"m" (*(volatile long long *)ptr), "0" ((unsigned long long) x)
				:"memory");

	return x;
}

static inline unsigned xchg_32(void *ptr, unsigned x)
{
	__asm__ __volatile__("xchgl %0,%1"
				:"=r" ((unsigned) x)
				:"m" (*(volatile unsigned *)ptr), "0" (x)
				:"memory");

	return x;
}

static inline unsigned short xchg_16(void *ptr, unsigned short x)
{
	__asm__ __volatile__("xchgw %0,%1"
				:"=r" ((unsigned short) x)
				:"m" (*(volatile unsigned short *)ptr), "0" (x)
				:"memory");

	return x;
}

/* Test and set a bit */
static inline char atomic_bitsetandtest(void *ptr, int x)
{
	char out;
	__asm__ __volatile__("lock; bts %2,%1\n"
						"sbb %0,%0\n"
				:"=r" (out), "=m" (*(volatile long long *)ptr)
				:"Ir" (x)
				:"memory");

	return out;
}

#endif /* UTIL_H_ */
