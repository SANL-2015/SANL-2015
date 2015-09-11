/* ########################################################################## */
/* mcs_lock.c                                                                 */
/* (C) Jean-Pierre Lozi, 2010-2011                                            */
/* (C) Gaël Thomas, 2010-2011                                                 */
/* -------------------------------------------------------------------------- */
/* ########################################################################## */
#include "mcs_lock.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))
#define cmpxchg(P, O, N) __sync_val_compare_and_swap((P), (O), (N))
#define atomic_inc(P) __sync_add_and_fetch((P), 1)
#define atomic_dec(P) __sync_add_and_fetch((P), -1) 
#define atomic_add(P, V) __sync_add_and_fetch((P), (V))
#define atomic_set_bit(P, V) __sync_or_and_fetch((P), 1<<(V))
#define atomic_clear_bit(P, V) __sync_and_and_fetch((P), ~(1<<(V)))

/* Compile read-write barrier */
#define barrier() asm volatile("": : :"memory")

/* Pause instruction to prevent excess processor bus usage */ 
//#define cpu_relax() asm volatile("pause\n": : :"memory")
#define cpu_relax() asm volatile("rep; nop": : :"memory")


/* Atomic exchange (of various sizes) */
static inline void *xchg_64(void *ptr, void *x)
{
	__asm__ __volatile__("xchg %0,%1"
											 :"=r" ((uintptr_t)x)
											 :"m" (*(volatile long long *)ptr), "0" ((uintptr_t) x)
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

void lock_mcs(mcs_lock *m, mcs_lock_t *me)
{
	mcs_lock_t *tail;
	
	me->next = NULL;
	me->spin = 0;

	tail = xchg_64(m, me);
	
	/* No one there? */
	if (!tail) return;

	/* Someone there, need to link in */
	tail->next = me;

	/* Make sure we do the above setting of next. */
	barrier();
	
	/* Spin on my spin variable */
	while (!me->spin) cpu_relax();
	
	return;
}

void unlock_mcs(mcs_lock *m, mcs_lock_t *me)
{
	/* No successor yet? */
	if (!me->next)
	{
		/* Try to atomically unlock */
		if (cmpxchg(m, me, NULL) == me) return;
	
		/* Wait for successor to appear */
		while (!me->next) cpu_relax();
	}

	/* Unlock next one */
	me->next->spin = 1;	
}

int trylock_mcs(mcs_lock *m, mcs_lock_t *me)
{
	mcs_lock_t *tail;
	
	me->next = NULL;
	me->spin = 0;
	
	/* Try to lock */
	tail = cmpxchg(m, NULL, &me);
	
	/* No one was there - can quickly return */
	if (!tail) return 0;
	
	return /*EBUSY*/128;
}

