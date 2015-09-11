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
 *      This product includes software developed by Duke University
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

/* This file contains the implementation of TPCC DELIVERY transaction
 *  on Berkeley DB */

#include <errno.h>
#include <time.h>

#include "tpcc_globals.h"
#include "tpcc_schema.h"
#include "tpcc_trans.h"


/////////////////////////////////////////////////////////////////////////
int
delivery_transaction(DB_ENV *db_envp, 
		     DB  *dbp_neworder, 
		     DB  *dbp_order,
		     DB  *dbp_orderline,
		     DB  *dbp_customer,
		     DELIVERY_TRANSACTION_DATA *d_trans_data)
{
    int err, i, count;
    float sum_ol_amount = 0;
    time_t t_clock;
    const int dt_size = 26;
    char datetime[dt_size];
 

    DB_TXN *db_txn = 0;
    DBT  key, data;

    NEWORDER_PRIMARY_KEY  no_key;
    ORDER_PRIMARY_KEY     o_key;
    ORDER_PRIMARY_DATA    o_data;
    CUSTOMER_PRIMARY_KEY  c_key;
    CUSTOMER_PRIMARY_DATA c_data;

    assert( (dbp_customer && dbp_neworder && dbp_order
	      && dbp_orderline && d_trans_data) );

    /* Begin transaction */
#if TRANSACTIONS
    if((err = db_envp->txn_begin(db_envp, 0, &db_txn, 0)))
    {
	db_error("DB_ENV->txn_begin", err);
	goto done;
    }
#endif

    /* For a given warehouse number (W_ID), for each of the 10 districts
     * (D_W_ID, D_ID) within that warehouse, and for a given carrier number
     * O_CARRIER_ID:
     */
    for(i = 1; i<=DISTRICTS_PER_WAREHOUSE; i++)
    {
	DBC *cursor, *ol_cursor;
	ORDERLINE_PRIMARY_KEY ol_key;
	ORDERLINE_PRIMARY_DATA ol_data;

	/* The row int the NEW-ORDER table with matching NO_W_ID (equals W_ID), 
	 * and NO_D_ID (equals D_ID) and with the lowest NO_O_ID value is
	 * selected. This is the oldest undelivered order data of that district.
	 */
	no_key.NO_W_ID = d_trans_data->w_id;
	no_key.NO_D_ID = i;
	no_key.NO_O_ID = 0; /* to get the smallest */

	memset(&key, 0, sizeof(DBT));
	key.data = &no_key;
	key.size = sizeof(NEWORDER_PRIMARY_KEY);
	key.ulen = sizeof(NEWORDER_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;

	memset(&data, 0, sizeof(DBT));
	data.flags = DB_DBT_PARTIAL;
     
	/* Open the cursor */
	if ((err = dbp_neworder->cursor(dbp_neworder, db_txn, &cursor, 0)))
	{
	    db_error("DB->cursor", err);
	    goto abort;
	}
	
	if(err = cursor->c_get(cursor, &key, &data, DB_SET_RANGE))
	{
	    if(err != DB_NOTFOUND)
	    {
		db_error("DBC->c_get", err);
		cursor->c_close(cursor);
		goto abort;
	    }
	}
	/* If no matching row is found, then the delivery of an order for this
	 * district is skipped. */
	if(err || !(no_key.NO_W_ID == d_trans_data->w_id && no_key.NO_D_ID == i))
	{
	    printf("Nothing found: err=%d, no_key.NO_W_ID=%d, no_key.NO_D_ID=%d\n",
		   err, no_key.NO_W_ID, no_key.NO_D_ID);
	    cursor->c_close(cursor);
	    continue;
	}

	

	/* The selected row in the NEW-ORDER table is deleted */
	if(err = cursor->c_del(cursor, 0))
	{
	    db_error("DBC->c_get", err);
	    cursor->c_close(cursor);
	    goto abort;
	}
	
	/* Don't need the cursor, close it */
	if(err = cursor->c_close(cursor))
	{
	    db_error("DBC->c_close", err);
	    goto abort;
	}
	
	/* The row in the ORDER table with matching O_W_ID (equals W_ID), O_D_ID (equals D_ID), 
	 * and O_ID (equals NO_O_ID) is selected. O_C_ID, the customer number, is retrieved, and
	 * O_CARRIER_ID is updated.
	 */
	o_key.O_W_ID = d_trans_data->w_id;
	o_key.O_D_ID = i;
	o_key.O_ID = no_key.NO_O_ID;

	memset(&key, 0, sizeof(DBT));
	key.data = &o_key;
	key.ulen = sizeof(ORDER_PRIMARY_KEY);
	key.size = sizeof(ORDER_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;
	
	memset(&data, 0, sizeof(DBT));
	data.data = &o_data;
	data.ulen = sizeof(ORDER_PRIMARY_DATA);
	data.flags = DB_DBT_USERMEM;

	if(err = dbp_order->get(dbp_order, db_txn, &key, &data, 0))
	{
	    db_error("delivery-order index: DB->get", err);
	    goto abort;
	}
	
	o_data.O_CARRIER_ID = d_trans_data->o_carrier_id;
	
	if(err = dbp_order->put(dbp_order, db_txn, &key, &data, 0))
	{
	    db_error("DB->put", err);
	    goto abort;
	}
	
	/* All rows in ORDER-LINE table with matching OL_W_ID (equals O_W_ID), OL_D_ID,
	 * equals (O_D_ID), and OL_O_ID (equals O_ID) are selected. All OL_DELIVERY_D,
	 * the delivery dates, are updated to the current system time as returned by the
	 * operating system and the sum OL_AMOUNT is retrieved.
	 */
	
	/* Init the cursor */
	if ((err = dbp_orderline->cursor(dbp_orderline, db_txn, &ol_cursor, 
					 0)))
	{
	    db_error("DB->cursor", err);
	    goto abort;
	}
	
	ol_key.OL_W_ID = d_trans_data->w_id;
	ol_key.OL_D_ID = o_key.O_D_ID;
	ol_key.OL_O_ID = o_key.O_ID;
	ol_key.OL_NUMBER = 0; /* to get all matches */
	
	memset(&key, 0, sizeof(DBT));
	key.data = &ol_key;
	key.size = sizeof(ORDERLINE_PRIMARY_KEY);
	key.ulen = sizeof(ORDERLINE_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;

	memset(&data, 0, sizeof(DBT));
	data.data = &ol_data;
	data.ulen = sizeof(ORDERLINE_PRIMARY_DATA);
	data.flags = DB_DBT_USERMEM;

	count = 0;
	while(1)
	{
	    if(err = ol_cursor->c_get(ol_cursor, &key, &data, 
				      count == 0? DB_SET_RANGE:DB_NEXT))
	    {
		db_error("DBC->c_get", err);
		ol_cursor->c_close(ol_cursor);
		goto abort;
	    }
	
	    if(! (ol_key.OL_W_ID == o_key.O_W_ID &&
		  ol_key.OL_D_ID == o_key.O_D_ID &&
		  ol_key.OL_O_ID == o_key.O_ID))
	    {
		break;
	    } 

	    count++;
	    sum_ol_amount += ol_data.OL_AMOUNT;

	    if((int)time(&t_clock) == -1)
	    {
		error("time");
		ol_cursor->c_close(ol_cursor);
		goto abort;
	    }

	    ctime_r(&t_clock, (char*)datetime);

	    memcpy(ol_data.OL_DELIVERY_D, datetime, dt_size);
	    
	    if(err = ol_cursor->c_put(ol_cursor, &key, &data, DB_CURRENT))
	    {
		db_error("DBC->c_put", err);
		ol_cursor->c_close(ol_cursor);
		goto abort;
	    }
	}

	/* Close the cursor */
	if(err = ol_cursor->c_close(ol_cursor))
	{
	    db_error("DBC->c_close", err);
	    goto abort;
	}
	
	/* The row in the customer table with matching C_W_ID (equals W_ID), C_D_ID
	 * (equals (D_ID), and C_ID (equals O_C_ID) is selected and C_BALANCE is increased
	 * by the sum of all order-line amounts (OL_AMOUNT) previously retrieved. 
	 * C_DELIVERY_CNT is incremented by 1.
	 */
	c_key.C_W_ID = d_trans_data->w_id;
	c_key.C_D_ID = i;
	c_key.C_ID = o_data.O_C_ID;
	
	memset(&key, 0, sizeof(DBT));
	key.data = &c_key;
	key.size = sizeof(CUSTOMER_PRIMARY_KEY);
	key.ulen = sizeof(CUSTOMER_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;
	
	memset(&data, 0, sizeof(DBT));
	data.data = &c_data;
	data.ulen = sizeof(CUSTOMER_PRIMARY_DATA);
	data.flags = DB_DBT_USERMEM;
	
	if(err = dbp_customer->get(dbp_customer, db_txn, &key, &data, 0))
	{
	    db_error("delivery-customer:DB->get", err);
	    goto abort;
	}

	c_data.C_BALANCE += sum_ol_amount;
	c_data.C_DELIVERY_CNT++;
	
	if(err = dbp_customer->put(dbp_customer, db_txn, &key, &data, 0))
	{
	    db_error("DB->put", err);
	    goto abort;
	}
    }

    /* End transaction */
#if TRANSACTIONS
    if((err = db_txn->commit(db_txn, 0)))
    {
	db_error("DB_TXN->txn_commit", err);
	goto abort;
    }
#endif
    goto done;
    
 abort:
    if(db_txn != NULL)
    {
	int ret = err;
	err = db_txn->abort(db_txn);
	if(err)
	{
	    db_error("FATAL ERROR: DB_TXN->abort", err);
	}
	err = ret;
    }
    

 done:
    return err;
}
