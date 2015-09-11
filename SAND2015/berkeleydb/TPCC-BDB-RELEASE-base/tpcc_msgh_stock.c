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

/* This file contains the implementation of the STOCKLEVEL transaction
 * server. This server is forked by the inetd upon arrival of a
 * request from a benchmark client on a known port. 
 */



#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#include "msgh_common.h"
#include <db.h>
#include "msgh.h" /* from the TPC distribution */
#include "log.h"

int mypid;


struct 
{
    struct msgh_req hdr;
    STOCKLEVEL_TRANSACTION_DATA stockmsg;
}message;

int 
main(int argc, char **argv)
{
    int len;
    static int errcnt = 0;

    
    mypid = getpid();
    
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);


    if(init_log(mypid))
    {
	fprintf(stderr, 
		"STOCK %d could not init logging\n", mypid);
	exit(1);
    }
    
    if(prepare_for_xactions("/dafsmnt", 1))
    {
	write_log("STOCK %d: could not prepare for transactions\n", 
		  mypid);
	exit(1);
    }
    write_log("STOCK %d: is UP\n", mypid);
    
    /* Send ack back to message server */
    write(1, "I'm ready", 10);
        
    /* Main loop: read request, do transaction and send result back */

    while(1)
    {
	if((len = read(0, &message, sizeof(message))) <= 0)
	{
	    write_log
		("STOCK %d: read message failed: errno = %d\n",
		 mypid, errno);
	    if(++errcnt > 10)
	    {
		write_log
		    ("STOCK %d: too many errors, exiting\n", mypid);
		break;
	    }
	    else
		continue;
	}
	ntoh_msg_header(&message.hdr);
	if(len != sizeof(message))
	{
	    if(message.hdr.type == MSGH_SHUT)
	    {
		write_log("STOCK %d: shutting down\n", mypid);
		break;
	    }
	    write_log("STOCK %d: read %d, expected %d bytes\n", mypid, 
		      len, sizeof(message));
	    message.hdr.type = FAIL;
	    goto ret;
	}
	
	ntoh_stock(&message.stockmsg);
#if PRINTALOT
	write_log("STOCK: %d: thr: %d\n", mypid, message.stockmsg.threshold);
#endif
	if(do_stocklevel(&message.stockmsg))
	{

	    message.hdr.type = FAIL;
#if PRINT_TX_STATUS
	    write_log("STOCK %d: TRANSACTION FAILED\n", mypid);
#endif
	    hton_stock(&message.stockmsg);
	}
	else
	{
#if PRINT_TX_STATUS
	    write_log("STOCK %d: TRANSACTION SUCCEEDED\n", mypid);
#endif
	    message.hdr.type = SUCCESS;
#if PRINTALOT
	    write_log("STOCK %d: Writing to server: w_id = %d, d_id = %d, thh = %d, ls = %d\n", 
		      mypid, message.stockmsg.w_id, message.stockmsg.d_id, message.stockmsg.threshold,
		      message.stockmsg.low_stock);
#endif
	    hton_stock(&message.stockmsg);
	}
    ret:
	hton_msg_header(&message.hdr);	
	if(write(1, &message, sizeof(message)) <= 0)
	{
	    write_log("STOCK %d: write message failed, errno = %d\n", 
		      mypid, errno);
	    if(++errcnt > 10)
	    {
		("STOCK %d: too many errors, exiting\n", mypid);
		break;
	    }
	}
    }

    cleanup();
    exit(1);
}