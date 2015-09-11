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

/* This file contains the implementation of TPCC transactions on Berkeley DB */

#include <sys/param.h>
#include "msgh.h"
#include "tpcc_trans.h"

#define PRINTALOT 0
#define PRINT_TX_STATUS 0

/////////////////////////////////////////////////////////////////////////
/*
 * Initialize the environment, open and set up the databases.
 */
int prepare_for_xactions(char *home_dir, int start_thread);

/* Cleanup on exit */
void cleanup(void);

/* Wrappers for transactions */
int do_neworder(NEWORDER_TRANSACTION_DATA *notd);
int do_payment(PAYMENT_TRANSACTION_DATA *ptd);
int do_orderstatus(ORDERSTATUS_TRANSACTION_DATA *ostd);
int do_delivery(DELIVERY_TRANSACTION_DATA *dtd);
int do_stocklevel(STOCKLEVEL_TRANSACTION_DATA *sltd);

/* Byteorder converstion functions */
void ntoh_msg_header(struct msgh_req *msg);
void hton_msg_header(struct msgh_req *msg);

void ntoh_delivery(DELIVERY_QUEUE_RECORD *qr);

void ntoh_stock(STOCKLEVEL_TRANSACTION_DATA *std);
void hton_stock(STOCKLEVEL_TRANSACTION_DATA *std);

void ntoh_neworder(NEWORDER_TRANSACTION_DATA *ntd);
void hton_neworder(NEWORDER_TRANSACTION_DATA *ntd);

void ntoh_payment( PAYMENT_TRANSACTION_DATA *ptd);
void hton_payment(PAYMENT_TRANSACTION_DATA *ptd);

void ntoh_orderstatus(ORDERSTATUS_TRANSACTION_DATA *otd);
void hton_orderstatus(ORDERSTATUS_TRANSACTION_DATA *otd);
