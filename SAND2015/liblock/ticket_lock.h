/*
 * ticket_lock.h
 *
 *  Created on: 2012-12-17
 *      Author: wayne
 */

#ifndef TICKET_LOCK_H_
#define TICKET_LOCK_H_

#include "util.h"
#include <stddef.h>
#include <stdint.h>

typedef union ticketlock ticketlock;

union ticketlock
{
	unsigned u;
	struct
	{
		unsigned short ticket;
		unsigned short users;
	} s;
};

static void ticket_lock(ticketlock *t)
{
	unsigned short me = atomic_xadd(&t->s.users, 1);

	while (t->s.ticket != me) cpu_relax();
}

static void ticket_unlock(ticketlock *t)
{
	barrier();
	t->s.ticket++;
}



#endif /* TICKET_LOCK_H_ */

