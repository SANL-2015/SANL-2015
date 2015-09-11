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

/* This file contains the implementation of TPCC ORDER STATUS transaction
 *  on Berkeley DB */

#include <errno.h>


#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_trans.h"
#include "tpcc_schema.h"

/////////////////////////////////////////////////////////////////////////
int
orderstatus_transaction(DB_ENV *db_envp, 
			DB     *dbp_customer, 
			DB     *dbp_customer_sec,
			DB     *dbp_order, 
			DB     *dbp_order_sec,
			DB     *dbp_orderline,
			ORDERSTATUS_TRANSACTION_DATA  *os_trans_data)
{
    int err, count = 0;
    DB_TXN *db_txn = 0;

    CUSTOMER_PRIMARY_KEY    c_key;
    CUSTOMER_PRIMARY_DATA   c_data;
    ORDER_PRIMARY_KEY       o_key;
    ORDER_SECONDARY_KEY     o_key_sec;
    ORDER_PRIMARY_DATA      o_data;
    ORDERLINE_PRIMARY_KEY   ol_key;
    ORDERLINE_PRIMARY_DATA  ol_data;

    DBT  key, data, skey;
    DBC *ol_cursor, *o_cursor;

    assert( (dbp_customer && dbp_customer_sec && dbp_order && dbp_order_sec
	     && dbp_orderline && os_trans_data));
    
    
    /* Begin transaction */
#if TRANSACTIONS
    if((err = db_envp->txn_begin(db_envp, 0, &db_txn, 0)))
    {
	db_error("DB_ENV->txn_begin", err);
	goto done;
    }
#endif
    
    if(os_trans_data->c_id <= 0)
    {
	/* Case 2: the customer is selected based on customer last name:
	 * all rows in the customer table with matching C_W_ID, C_D_ID
	 * and C_LAST are selected sorted by C_FIRST in ascending order. 
	 * Let n be the number of rows selected. C_BALANCE, C_FIRST, 
	 * C_MIDDLE, C_LAST are retrieved from the row at position n/2
	 * rounded up in the sorted set of selected rows from the 
	 * CUSTOMER table
	 */
	
	DBC *cursor;
	int count = 0, position = 0;
	CUSTOMER_SECONDARY_KEY  c_key_sec;
	DBT sec_key, p_data, p_key;
	

	/* Make sure that the last name is there */
	assert(os_trans_data->c_last[0] != '\0');

	/* Open the cursor */
	if ((err = dbp_customer_sec->cursor(dbp_customer_sec, db_txn, &cursor, 0)))
	{
	    db_error("DB->cursor", err);
	    goto abort;
	}
	
	/* Prepare the secondary key */
	memset(&sec_key, 0, sizeof(DBT));
	c_key_sec.C_W_ID = os_trans_data->w_id;
	c_key_sec.C_D_ID = os_trans_data->d_id;
	memcpy(c_key_sec.C_LAST, os_trans_data->c_last, 17);
	c_key_sec.C_FIRST[0] = 'A';   /* to find all first names */
	c_key_sec.C_FIRST[1] = '\0';
	c_key_sec.C_ID = 0;

	memset(&sec_key, 0, sizeof(DBT));
	sec_key.data = &c_key_sec;
	sec_key.size = sizeof(CUSTOMER_SECONDARY_KEY);
	sec_key.ulen = sizeof(CUSTOMER_SECONDARY_KEY);
	sec_key.flags = DB_DBT_USERMEM;
	
	/* Prepare the primary data  
	 * We do not want to retrieve any primary data at this time */
	memset(&p_data, 0, sizeof(DBT));
	p_data.dlen = 0;          
	p_data.doff = 0;
	p_data.flags = DB_DBT_PARTIAL;  

	count = 0;
	/* Cycle through the cursor until last name remains the same */
	while(1)
	{
	    err = cursor->c_get(cursor, &sec_key, &p_data, count == 0?DB_SET_RANGE:DB_NEXT);
	    if(err)
	    {
		if(err != DB_NOTFOUND)
		{
		    db_error("os: DBC->c_get", err);
		    if((err = cursor->c_close(cursor)))
		    {
			db_error("os: DBC->c_close", err);
		    }
		    goto abort;
		}
		else break;
	    }
	    /* Check that we have not skipped to a new last name */
	    if(strcmp(c_key_sec.C_LAST, os_trans_data->c_last))
	    {
		break;
	    }
	    count++;
	}
	
	/* Ok, now we know how many records with this last name there are */
	if(count == 0)
	{
	    /* Did not find anything with this last name? */  
	    if((err = cursor->c_close(cursor)))
	    {
		db_error("os: DBC->c_close", err);
	    }
	    goto abort;
	}
	else count = count/2+1;

	/* Now traverse the cursor back until count/2 items are passed */
	while(count > 0)
	{
	    err = cursor->c_get(cursor, &sec_key, &p_data, DB_PREV);
	    if(err)
	    {
		/* should not get any errors now! */
		db_error("os: DBC->c_get",err);
		if((err = cursor->c_close(cursor)))
		{
		    db_error("os: DBC->c_close", err);
		}
		goto abort;
	    }
	    count --;
	}
	
	/* OK, now we have the primary key for the customer, proceed */
	os_trans_data->c_id = c_key_sec.C_ID;
	
	/* We don't need the cursor, so close it */
	if(err = cursor->c_close(cursor))
	{
	    db_error("DBC->c_close", err);
	    goto abort;
	}
    }
    
    
    /* Case 1: the customer is selected based on customer number:
     * the row in the customer table with matching C_W_ID, C_D_ID
     * and C_ID is selected. C_BALANCE, C_FIRST, C_MIDDLE and C_LAST
     * are retrieved.
     */
    c_key.C_W_ID = os_trans_data->w_id;
    c_key.C_D_ID = os_trans_data->d_id;
    c_key.C_ID   = os_trans_data->c_id;

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
		 db_error("order-status DB->db_get", err);
		 goto abort;
    }

    memcpy(os_trans_data->c_first, c_data.C_FIRST, 17);
    memcpy(os_trans_data->c_middle, c_data.C_MIDDLE, 3);
    memcpy(os_trans_data->c_last, c_data.C_LAST, 17);
    os_trans_data->c_balance = c_data.C_BALANCE;

    /* The row in the ORDER table with matching O_W_ID (equals C_W_ID), 
     * O_D_ID (equals C_D_ID), O_C_ID (equals C_ID), and with the largest
     * existing O_ID is selected. This is the most recent order placed by
     * that customer. O_ID, O_ENTRY_D, and O_CARRIER_ID are retrieved. 
     */
    o_key_sec.O_W_ID = c_key.C_W_ID;
    o_key_sec.O_D_ID = c_key.C_D_ID;
    o_key_sec.O_C_ID = c_key.C_ID;
    
    memset(&skey, 0, sizeof(DBT));
    skey.data = &o_key_sec;
    skey.size = sizeof(ORDER_SECONDARY_KEY);
    skey.ulen = sizeof(ORDER_SECONDARY_KEY);
    skey.flags = DB_DBT_USERMEM;
    
    memset(&key, 0, sizeof(DBT));
    key.data = &o_key;
    key.ulen = sizeof(ORDER_PRIMARY_KEY);
    key.size = sizeof(ORDER_PRIMARY_KEY);
    key.flags = DB_DBT_USERMEM;

    memset(&data, 0, sizeof(DBT));
    data.data = &o_data;
    data.ulen = sizeof(ORDER_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;

    /* Init the cursor on the order secondary index */
    if ((err = dbp_order_sec->cursor(dbp_order_sec, db_txn, &o_cursor, 0)))
    {
	db_error("DB->cursor", err);
	goto abort;
    }
    
    /* We need to get the latest order made by this customer. This should
     * be the last duplicate */
    count = 0;
    while(1)
    {
	if(err = o_cursor->c_pget(o_cursor, &skey, &key, &data, count==0?DB_SET:DB_NEXT_DUP))
	{
	    if(err == DB_NOTFOUND && count > 0)
		break;
	    else
	    {
		char msg_buf[50];
		db_error("os: DB->db_pget", err);
		write_log("WID=%d, DID=%d, CID=%d\n", os_trans_data->w_id,
			  os_trans_data->d_id, os_trans_data->c_id);
		if((err = o_cursor->c_close(o_cursor)))
		{
		    db_error("os: DBC->c_close", err); 
		}
		goto abort;
	    }
	}
	count++;
    }


    /* Close the cursor */
    if(err = o_cursor->c_close(o_cursor))
    {
	db_error("DBC->c_close", err);
	goto abort;
    }
    
    os_trans_data->o_id = o_key.O_ID;
    memcpy(os_trans_data->o_entry_d, o_data.O_ENTRY_D, 20);
    os_trans_data->o_carrier_id = o_data.O_CARRIER_ID;
    
    /* All rows in the ORDER-LINE table with matching OL_W_ID (equals O_W_ID), 
     * OL_D_ID (equals O_D_ID), and OL_O_ID (equals O_ID) are selected and the 
     * corresponding sets of OL_I_ID, OL_SUPPLY_W_ID, 
     * OL_QUANTITY, OL_AMOUNT, and OL_DELIVERY_D are retrieved.
     */
    
    /* Init the cursor */
    if ((err = dbp_orderline->cursor(dbp_orderline, db_txn, &ol_cursor, 0)))
    {
	db_error("DB->cursor", err);
	goto abort;
    }
    ol_key.OL_W_ID = o_key.O_W_ID;
    ol_key.OL_D_ID = o_key.O_D_ID;
    ol_key.OL_O_ID = o_key.O_ID;
    ol_key.OL_NUMBER = 0; /* to get all OL_NUMBERS */
    

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
	    db_error("os: DBC->c_get", err);
	    if((err = ol_cursor->c_close(ol_cursor)))
	    {
		db_error("os: DBC->c_close", err);
	    }
	    goto abort;
	}
	
	if(! (ol_key.OL_W_ID == o_key.O_W_ID &&
	      ol_key.OL_D_ID == o_key.O_D_ID &&
	      ol_key.OL_O_ID == o_key.O_ID))
	{
	    break;
	}
	
	if(count == 15)
	{
	    error("Maximum orderline count exceeded in orderstatus transaction ");
	    break;
	}

	os_trans_data->order_data[count].ol_supply_w_id = ol_data.OL_SUPPLY_W_ID;
	os_trans_data->order_data[count].ol_i_id = ol_data.OL_I_ID;
	os_trans_data->order_data[count].ol_quantity = ol_data.OL_QUANTITY;
	os_trans_data->order_data[count].ol_amount = ol_data.OL_AMOUNT;
	memcpy(os_trans_data->order_data[count].ol_delivery_d, ol_data.OL_DELIVERY_D, 11);
  
	count++;	  
    }
    os_trans_data->item_cnt = count;

    /* Close the cursor */
    if(err = ol_cursor->c_close(ol_cursor))
    {
	db_error("DBC->c_close", err);
	goto abort;
    }
    
    /* End transaction */
#if TRANSACTIONS
    if((err = db_txn->commit(db_txn, 0)))
    {
	db_error("DB_TXN->commit", err);
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





  


