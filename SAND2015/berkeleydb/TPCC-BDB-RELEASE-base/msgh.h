/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 */

#ifndef MSGH_H
#define MSGH_H

#pragma ident "@(#)msgh.h	1.5	94/12/08	SMI"

/*
 * Title : msgh.h
 * Defines for the Custom Message Handler
 * All defines should be prefixed with msgh_
 *
 * Author : Shanti S
 * Date Written : 3/29/94
 */


/* These defines are for TPCC and can be changed for any other
 * future benchmark
 */
#define	MSGH_MSGKEY		50000	/* ipckey of msg-passer's msgqueue */
#define	MSGH_SHMKEY		60000	/* ipckey of msgh shm */
#define	MSGH_SHMADR	0x2000000	/* Attach address */
#define	MSGH_MAXMSG		1200	/* Max. length of a message */
#define	MSGH_SRVNMLEN	20	/* Max. length of server name */

#define	FAIL		1		/* in reply structure */
#define	SUCCESS		0
#define	FATAL		-1

/* Pre-defined and reserved messages */
#define	MSGH_STARTRUN	995	/* Start of run */
#define MSGH_ENDRUN		996 /* End of run */
#define MSGH_SHUT		997	/* Shutdown servers message */

/* Structure of a message request passed on the msg-queue */
struct msgh_req {
	int		type;			/* Type of message */
	int		len;			/* Length of message */
	int		client_id;		/* Id of client sending this msg */
	int		pad;		/* Never change this, it's for message alignment*/
	/* Actual message follows this */
};	


/* Structure for each server in shared memory */
struct msgh_status {
	short	num_in_queue;	/* Number of messages queued */
	short	alive;		/* Is server alive ? */
	int		msgq_id;		/* Message-queue id of this server */
	long	stamp;		/* Status of this server : timestamp  */
};

/* Structure for each type of server */
struct msgh_type {
	int		num_servers;	/* Number of servers of this type */
	char	name[MSGH_SRVNMLEN];	/* name of server */
};

/* Structure of shared memory segment used by message handler */
struct msgh_shm {
	/* Server Info */
	int		num_srvtype;	/* Number of server types */
	struct msgh_type *type_info;
	struct msgh_status *srv_info;

	/* Client info */
	int		num_client;		/* Number of clients configured for */
	struct msgh_reply *clnt_info;
};

extern int errno;

#endif /* MSGH_H */
