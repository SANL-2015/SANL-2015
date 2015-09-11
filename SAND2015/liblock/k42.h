/*
 * k42.h
 *
 *  Created on: 2012-12-5
 *      Author: wayne
 */

#ifndef K42_H_
#define K42_H_

#include "util.h"
#include <stddef.h>


typedef struct k42lock k42lock;
struct k42lock
{
	k42lock *next;
	k42lock *tail;
};

static void k42_lock(k42lock *l)
{
	k42lock me;
	k42lock *pred, *succ;
	me.next = NULL;

	barrier();

	pred = xchg_64(&l->tail, &me);
	if (pred)
	{
		me.tail = (void *) 1;

		barrier();
		pred->next = &me;
		barrier();

		while (me.tail) cpu_relax();
	}

	succ = me.next;

	if (!succ)
	{
		barrier();
		l->next = NULL;

		if (cmpxchg_util(&l->tail, &me, &l->next) != &me)
		{
			while (!me.next) cpu_relax();

			l->next = me.next;
		}
	}
	else
	{
		l->next = succ;
	}
}


static void k42_unlock(k42lock *l)
{


	k42lock *succ = l->next;

	barrier();

	if (!succ)
	{

		if (cmpxchg_util(&l->tail, &l->next, NULL) == (void *) &l->next) return;

		while (!l->next) cpu_relax();

		succ = l->next;

	}

	succ->tail = NULL;

}

#endif /* K42_H_ */

