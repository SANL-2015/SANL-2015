/*
 * Copyright (c) 2002 Harvard University - Alexandra Fedorova
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Harvard University
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* This file has Berkeley DB checkpoint thread */

#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <db.h>
#include "db_checkpoint.h"
#include "log.h"
#include <errno.h>
/** +EDIT */
#ifdef BENCHMARK
#include "liblock.h"
#endif
/** -EDIT */

void *
checkpoint_thread(void *arg)
{
    DB_ENV *dbenv;
    int ret;
    int my_pid = (int)pthread_self();

    dbenv = arg;
    
    if(init_log(my_pid))
    {  
	exit(1);
    }
    
    write_log("CHP %d: starting\n", my_pid);


    /* Checkpoint once in 30 seconds. */
    for (;; poll(0,0,30000))
    {
	switch (ret = dbenv->txn_checkpoint(dbenv, 0, 0, 0)) 
	{
	case 0:
	  write_log("CHP %d: checkpoint...\n", my_pid);
	  break;
	default:
	    write_log("CHP %d: checkpoint error - %s\n",
		      my_pid, db_strerror(ret));
	    break;
	}
	
	if(pthread_mutex_lock(&checkpoint_mutex))
	{
	    write_log("CHP %d: pthread_mutex_lock error - %s\n",
		      my_pid, strerror(errno));
	    goto error;
	}
	
	if(checkpoint_exit_flag)
	{
	    /* The thread was told to exit */
	    if(pthread_mutex_unlock(&checkpoint_mutex))
	    {
		write_log("CHP %d: pthread_mutex_unlock error - %s\n",
			  my_pid, strerror(errno));
		goto error;
	    }
	    write_log("CHP %d: exiting gracefully\n", my_pid);
	    goto done;
	}
	if(pthread_mutex_unlock(&checkpoint_mutex))
	{
	    write_log("CHP %d: pthread_mutex_unlock error - %s\n",
		      my_pid, strerror(errno));
	    goto error;
	}
    }
    
 error:
    write_log("CHP %d: exiting from ERROR\n", my_pid);
 done:
    return (void *)0;
}

/* Starts the checkpoint thread and initializes its mutex */
int
start_checkpoint_thread(DB_ENV *db_env, pthread_t *ptid)
{
    int errno;

    /* Start a checkpoint thread. */
/** +EDIT */
#ifndef BENCHMARK
    if ((errno = pthread_create(
	ptid, NULL, checkpoint_thread, (void *)db_env)) != 0)
#else
    if ((errno = liblock_thread_create(
	ptid, NULL, checkpoint_thread, (void *)db_env)) != 0)
#endif
/** -EDIT */
    {
	return -1;
    }
    if(pthread_mutex_init(&checkpoint_mutex, 0))
    {
	return -1;
    }

    return 0;
}
 
