/*
 * mcs_lock.h
 *
 *  Created on: 2012-12-17
 *      Author: wayne
 */

#ifndef MCS_LOCK_H_
#define MCS_LOCK_H_

#include "util.h"
#include <stddef.h>
#include <time.h>

typedef struct mcs_lock_t mcs_lock_t;
struct mcs_lock_t {
	mcs_lock_t *next;
	long spin;
};
typedef struct mcs_lock_t *mcs_lock;



static void unlock_mcs(mcs_lock *m, mcs_lock_t *me) {
	/* No successor yet? */
	if (!me->next) {
		//printf("$$$$ No need Unlock next me addre  %x next addr %x ori address %x\n", me, me->next, *m);
		printf("$$$$ No need Unlock next me addre");
		/* Try to atomically unlock */
		if (cmpxchg_util(m, me, NULL) == me)
			return;

		/* Wait for successor to appear */
		while (!me->next)
			cpu_relax();
	}
	//printf("$$$$ Unlock next one lock next address  %x\n", me->next);
	/* Unlock next one */
	me->next->spin = 1;
}

static int trylock_mcs(mcs_lock *m, mcs_lock_t *me) {
	mcs_lock_t *tail;

	me->next = NULL;
	me->spin = 0;

	/* Try to lock */
	tail = cmpxchg_util(m, NULL, me);

	/* No one was there - can quickly return */
	if (!tail)
		return 0;

	return EBUSY;
}

#endif /* MCS_LOCK_H_ */
