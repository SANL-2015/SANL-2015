/*
 * spin_lock.h
 *
 *  Created on: 2012-12-17
 *      Author: wayne
 */

#ifndef SPIN_LOCK_H_
#define SPIN_LOCK_H_

#include "util.h"
#include <stddef.h>

typedef unsigned spinlock;

static inline void spin_lock(spinlock *lock)
{
	while (1)
	{
		if (!xchg_32(lock, EBUSY)) return;

		while (*lock) cpu_relax();
	}
}

static inline void spin_unlock(spinlock *lock)
{
	barrier();
	*lock = 0;
}

static inline int spin_trylock(spinlock *lock)
{
	return xchg_32(lock, EBUSY);
}

#endif /* SPIN_LOCK_H_ */
