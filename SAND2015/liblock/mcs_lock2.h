/*
 * mcs_lock.h
 *
 *  Created on: 2012-12-17
 *      Author: wayne
 */

#ifndef MCS_LOCK_H_
#define MCS_LOCK_H_

#include "liblock.h"
#include "util.h"
#include <stddef.h>
#include <time.h>

typedef struct mcs_lock_t mcs_lock_t;
struct mcs_lock_t {
	mcs_lock_t *next;
	long spin;
	char pad[pad_to_cache_line(sizeof(mcs_lock_t *) + sizeof(long))];
};
typedef struct mcs_lock_t *mcs_lock;

static int lock_mcs(mcs_lock *m, mcs_lock_t *me) {
	mcs_lock_t *tail;

	me->next = NULL;
	me->spin = 0;

	tail = xchg_64(m, me);
	/* No one there? */
	if (!tail)
		return 0;

	/* Someone there, need to link in */
	tail->next = me;
	/* Make sure we do the above setting of next. */barrier();

	/* Spin on my spin variable */
	while (!me->spin) {


		cpu_relax();
	}

	return 1;
}

static int unlock_mcs(mcs_lock *m, mcs_lock_t *me) {
	/* No successor yet? */
	if (!me->next) {
		/* Try to atomically unlock */
		if (cmpxchg_util(m, me, NULL) == me)
			return 0;

		/* Wait for successor to appear */
		while (!me->next)
			cpu_relax();
	}
	//printf("$$$$ Unlock next one lock next address  %x\n", me->next);
	/* Unlock next one */
	me->next->spin = 1;
	return EBUSY;
}


#endif /* MCS_LOCK_H_ */
