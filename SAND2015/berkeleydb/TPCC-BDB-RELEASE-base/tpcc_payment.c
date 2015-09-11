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

/* This file contains the implementation of TPCC PAYMENT transaction
 *  on Berkeley DB */

#include <errno.h>
#include <time.h>

#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_schema.h"
#include "tpcc_trans.h"


/////////////////////////////////////////////////////////////////////////
int
payment_transaction(DB_ENV *db_envp,
		    DB *dbp_warehouse, 
		    DB *dbp_district,
		    DB *dbp_customer, 
		    DB *dbp_customer_sec,
		    DB *dbp_history,
		    PAYMENT_TRANSACTION_DATA *p_trans_data)
{
    DB_TXN *db_txn = NULL;
    DBT key, data;

    int err = 0;
    const int dt_size = 26;
    char datetime[dt_size];
    time_t t_clock;

    WAREHOUSE_PRIMARY_KEY   w_key;
    WAREHOUSE_PRIMARY_DATA  w_data;
    DISTRICT_PRIMARY_KEY    d_key;
    DISTRICT_PRIMARY_DATA   d_data;
    CUSTOMER_PRIMARY_KEY    c_key;
    CUSTOMER_PRIMARY_DATA   c_data;
    HISTORY_PRIMARY_KEY     h_key;

    assert( (dbp_warehouse && dbp_district && dbp_customer && dbp_history
	     && p_trans_data) );

    if((int)time(&t_clock) == -1)
    {
	db_error("time", errno);
	return errno;
    }

    ctime_r(&t_clock, (char*)datetime);

    /* Begin transaction */
#if TRANSACTIONS
    if((err = db_envp->txn_begin(db_envp, 0, &db_txn, 0)))
    {
	db_error("DB_ENV->txn_begin", err);
	goto done;
    }
#endif

   /* The row in the WAREHOUSE table with matching W_ID is selected. 
     * W_NAME, W_STREET_1, W_STREET_2, W_CITY, W_STATE, and W_ZIP are
     * retrieved and W_YTD, the warehouse's year-to-date balance is 
     * increased by H_AMOUNT.
     */

    w_key.W_ID = p_trans_data->w_id;
    
    memset(&key, 0, sizeof(DBT));
    key.data = &w_key;
    key.size = sizeof(WAREHOUSE_PRIMARY_KEY);
    key.ulen = sizeof(WAREHOUSE_PRIMARY_KEY);
    key.flags = DB_DBT_USERMEM;
    
    memset(&data, 0, sizeof(DBT));
    data.data = &w_data;
    data.ulen = sizeof(WAREHOUSE_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;
    
    if(err = dbp_warehouse->get(dbp_warehouse, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->get", err);
	goto abort;
    }
    
    memcpy(p_trans_data->w_street_1, w_data.W_STREET_1, 21);
    memcpy(p_trans_data->w_street_2, w_data.W_STREET_2, 21);
    memcpy(p_trans_data->w_city, w_data.W_CITY, 21);
    memcpy(p_trans_data->w_state, w_data.W_STATE, 3);
    memcpy(p_trans_data->w_zip, w_data.W_ZIP, 10);
    
    w_data.W_YTD += p_trans_data->h_amount;


    if(err = dbp_warehouse->put(dbp_warehouse, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->db_put", err);
	goto abort;
    }
    


    /* The row in the DISTRICT table with matching D_W_ID and D_ID is selected.
     * D_NAME, D_STREET_1, D_STREET_2, D_CITY, D_STATE, and D_ZIP are retrieved
     * and D_YTD, the district's year-to-date balance, is increased by H_AMOUNT
     */
    d_key.D_W_ID = p_trans_data->w_id;
    d_key.D_ID = p_trans_data->d_id;
    
    memset(&key, 0, sizeof(DBT));
    key.data = &d_key;
    key.size = sizeof(DISTRICT_PRIMARY_KEY);
    key.ulen = sizeof(DISTRICT_PRIMARY_KEY);
    key.flags = DB_DBT_USERMEM;
	
    memset(&data, 0, sizeof(DBT));
    data.data = &d_data;
    data.ulen = sizeof(DISTRICT_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;
    
    if(err = dbp_district->get(dbp_district, db_txn, &key, &data, 0))
    {
	db_error("payment 1 DB->db_get", err);
	goto abort;
    }

    memcpy(p_trans_data->d_street_1, d_data.D_STREET_1, 21);
    memcpy(p_trans_data->d_street_2, d_data.D_STREET_2, 21);
    memcpy(p_trans_data->d_city, d_data.D_CITY, 21);
    memcpy(p_trans_data->d_state, d_data.D_STATE, 3);
    memcpy(p_trans_data->d_zip, d_data.D_ZIP, 10);
    
    d_data.D_YTD += p_trans_data->h_amount;
    
    if(err = dbp_district->put(dbp_district, db_txn, &key, &data, 0))
    {
	db_error("DB->db_put", err);
	goto abort;
    }


    if(p_trans_data->c_id <= 0)
    {
	/* Case 2: the customer is selected based on customer last name:
	 * all rows in the customer table with matching C_W_ID, C_D_ID
	 * and C_LAST are selected sorted by C_FIRST in ascending order. 
	 * Let n be the number of rows selected. C_ID, C_FIRST, C_MIDDLE, 
	 * C_LAST, C_STREET_1,C_STREET_2, C_CITY, C_STATE, C_ZIP, C_PHONE, 
	 * C_SINCE, C_CREDIT, C_CREDIT_LIM, C_DISCOUNT, and C_BALANCE 
	 * are retrieved from the row at position (n/2 rounded up to 
	 * the next integer) in the sorted set of selected rows from the
	 * CUSTOMER table. 
	 * C_BALANCE is decreased by H_AMOUNT. C_YTD_PAYMENT is increased
	 * by H_AMOUNT. C_PAYMENT_CNT is incremented by 1.
	 */
	
	/* In the secondary customer index, the records given C_W_ID, 
	 * C_D_ID, C_LAST are sorted in ascending order by the C_FIRST. 
	 * So we go through those records twice: first to find out how 
	 * many there are, and second to get the one at the right position. 
	 * Is there a more efficient way to do this? 
	 */
	
	DBC *cursor;
	int count = 0, position = 0;
	CUSTOMER_SECONDARY_KEY  c_key_sec;
	DBT sec_key, p_data, p_key;
	

	/* Make sure that the last name is there */
	assert(p_trans_data->c_last[0] != '\0');

	/* Open the cursor */
	if ((err = dbp_customer_sec->cursor(dbp_customer_sec, 
					    db_txn, &cursor, 0)))
	{
	    db_error("payment: DB->cursor", err);
	    goto abort;
	}
	
	/* Prepare the secondary key */
	memset(&sec_key, 0, sizeof(DBT));
	c_key_sec.C_W_ID = p_trans_data->c_w_id;
	c_key_sec.C_D_ID = p_trans_data->c_d_id;
	memcpy(c_key_sec.C_LAST, p_trans_data->c_last, 17);
	c_key_sec.C_FIRST[0] = 'A'; 
	c_key_sec.C_FIRST[0] = '\0'; 
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
	    err = cursor->c_get(cursor, &sec_key, &p_data, count == 0? 
				DB_SET_RANGE:DB_NEXT);
	    if(err)
	    {
		if(err != DB_NOTFOUND)
		{
		    db_error("payment: DBC->c_get", err);
		    if((err=cursor->c_close(cursor)))
		    {
			db_error("payment: DBC->c_close", err);
		    }
		    goto abort;
		}
		else break;
	    }
	    /* Check that we have not skipped to a new last name */
	    if(strcmp(c_key_sec.C_LAST, p_trans_data->c_last))
	    {
		break;
	    }
	    count++;
	}
	
	/* Ok, now we know how many records with this last name there are */
	if(count == 0)
	{
	    /* Did not find anything with this last name? */
	    db_error("Found nothing with last name", 0);
	    if(err = cursor->c_close(cursor))
	    {
		db_error("payment: DBC->c_close()", err);
	    }
	    goto abort;
	}
	else count = count/2 + 1;

	/* Now traverse the cursor back until count/2 items are passed */
	while(count > 0)
	{
	    err = cursor->c_get(cursor, &sec_key, &p_data, DB_PREV);
	    if(err)
	    {
		/* should not get any errors now! */
		db_error("payment: DBC->c_get",err);
		if( (err = cursor->c_close(cursor)))
		{
		    db_error("payment: DBC->c_close",err);
		}
		goto abort;
	    }
	    count--;
	}
	
	/* OK, now we have the primary key for the customer, proceed */
	p_trans_data->c_id = c_key_sec.C_ID;
	
	/* We don't need the cursor, so close it */
	if(err = cursor->c_close(cursor))
	{
	    db_error("payment: DBC->c_close", err);
	    goto abort;
	}
    }
    
    
    /* Case 1: the customer is selected based on customer number:
     * the row in the customer table with matching C_W_ID, C_D_ID
     * and C_ID is selected. C_FIRST, C_MIDDLE, C_LAST, C_STREET_1,
     * C_STREET_2, C_CITY, C_STATE, C_ZIP, C_PHONE, C_SINCE, C_CREDIT,
     * C_CREDIT_LIM, C_DISCOUNT, and C_BALANCE are retrieved. 
     * C_BALANCE is decreased by H_AMOUNT. C_YTD_PAYMENT is increased
     * by H_AMOUNT. C_PAYMENT_CNT is incremented by 1.
     */
    c_key.C_W_ID = p_trans_data->c_w_id;
    c_key.C_D_ID = p_trans_data->c_d_id;
    c_key.C_ID   = p_trans_data->c_id;

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
	db_error("payment 2: DB->db_get", err);
	goto abort;
    }
    
    

    memcpy(p_trans_data->c_first, c_data.C_FIRST, 17);
    memcpy(p_trans_data->c_middle, c_data.C_MIDDLE, 3);
    memcpy(p_trans_data->c_last, c_data.C_LAST, 17);
    memcpy(p_trans_data->c_street_1, c_data.C_STREET_1, 21);
    memcpy(p_trans_data->c_street_2, c_data.C_STREET_2, 21);
    memcpy(p_trans_data->c_city, c_data.C_CITY, 21);
    memcpy(p_trans_data->c_state, c_data.C_STATE, 2);
    memcpy(p_trans_data->c_zip, c_data.C_ZIP, 10);
    memcpy(p_trans_data->c_phone, c_data.C_PHONE, 16);
    memcpy(p_trans_data->c_credit, c_data.C_CREDIT, 3);
    memcpy(p_trans_data->c_since, c_data.C_SINCE, 11);
    p_trans_data->c_id         = c_key.C_ID;

    c_data.C_BALANCE -= p_trans_data->h_amount;

    p_trans_data->c_balance    = c_data.C_BALANCE;
    p_trans_data->c_credit_lim = c_data.C_CREDIT_LIM;
    p_trans_data->c_discount   = c_data.C_DISCOUNT;

    c_data.C_YTD_PAYMENT += p_trans_data->h_amount;
    c_data.C_PAYMENT_CNT++;

    /* If the value of C_CREDIT is equal to "BC", then C_DATA is also 
     * retrieved from the selected customer and the following history 
     * information: C_ID, C_D_ID, C_W_ID, D_ID W_ID, and H_AMOUNT are 
     * inserted at the left  of the C_DATA field by shifting the
     * existing content fo C_DATA to the right side of the C_DATA field. 
     * The content of the C_DATA field never exceeds 500 characters. 
     * The selected customer is updated with the new C_DATA field. 
     */
    
    if(!strcmp(c_data.C_CREDIT, "BC"))
    {
	char new_data[sizeof(int)*5 + sizeof(double)];
	
	memcpy(&new_data, &c_key.C_ID, sizeof(int)); 
	memcpy(&new_data[4], &c_key.C_D_ID, sizeof(int)); 
	memcpy(&new_data[8], &c_key.C_W_ID, sizeof(int)); 
	memcpy(&new_data[12], &d_key.D_ID, sizeof(int)); 
	memcpy(&new_data[16], &w_key.W_ID, sizeof(int)); 
	memcpy(&new_data[20], &p_trans_data->h_amount, sizeof(double)); 
	
	memcpy(c_data.C_DATA, 
	       c_data.C_DATA + 5 + sizeof(double)/sizeof(int), 
	       500 - sizeof(int)*5 + sizeof(double));
	memcpy(c_data.C_DATA, new_data, sizeof(int)*5 + sizeof(double));
    }

    memcpy(p_trans_data->c_data_1, c_data.C_DATA, 50);
    p_trans_data->c_data_1[50] = 0;

    memcpy(p_trans_data->c_data_2, &c_data.C_DATA[50], 50);
    p_trans_data->c_data_2[50] = 0;

    memcpy(p_trans_data->c_data_3, &c_data.C_DATA[100], 50);
    p_trans_data->c_data_3[50] = 0;

    memcpy(p_trans_data->c_data_4, &c_data.C_DATA[150], 50);
    p_trans_data->c_data_4[50] = 0;

    if(err = dbp_customer->put(dbp_customer, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->db_put", err);
	goto abort;
    }
    

    /* H_DATA is built by concatenating W_NAME and D_NAME separated by 
     * 4 spaces */
    memcpy(h_key.H_DATA, w_data.W_NAME, 10);
    memcpy(&h_key.H_DATA[10], "    ", 4);
    memcpy(&h_key.H_DATA[14], d_data.D_NAME, 11);


    /* A new row is inserted into the HISTORY table with H_C_ID = C_ID, 
     * H_C_D_ID = C_D_ID, 
     * H_C_W_ID = C_W_ID, H_D_ID = D_ID, and H_W_ID = W_ID.
     */
    h_key.H_C_ID   = c_key.C_ID;
    h_key.H_C_D_ID = c_key.C_D_ID;
    h_key.H_C_W_ID = c_key.C_W_ID;
    h_key.H_D_ID   = d_key.D_ID;
    h_key.H_W_ID   = w_key.W_ID;
    h_key.H_AMOUNT = p_trans_data->h_amount;
    memcpy(h_key.H_DATE, datetime, dt_size);
    
    memset(&key, 0, sizeof(DBT));
    key.data = &h_key;
    key.size = sizeof(HISTORY_PRIMARY_KEY);
	
    memset(&data, 0, sizeof(DBT));

    if(err = dbp_history->put(dbp_history, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->put", err);
	goto abort;
    }


    /* The row in the WAREHOUSE table with matching W_ID is selected. 
     * W_NAME, W_STREET_1, W_STREET_2, W_CITY, W_STATE, and W_ZIP are
     * retrieved and W_YTD, the warehouse's year-to-date balance is 
     * increased by H_AMOUNT.
     */

    w_key.W_ID = p_trans_data->w_id;
    
    memset(&key, 0, sizeof(DBT));
    key.data = &w_key;
    key.size = sizeof(WAREHOUSE_PRIMARY_KEY);
    key.ulen = sizeof(WAREHOUSE_PRIMARY_KEY);
    key.flags = DB_DBT_USERMEM;
    
    memset(&data, 0, sizeof(DBT));
    data.data = &w_data;
    data.ulen = sizeof(WAREHOUSE_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;
    
    if(err = dbp_warehouse->get(dbp_warehouse, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->get", err);
	goto abort;
    }
    
    memcpy(p_trans_data->w_street_1, w_data.W_STREET_1, 21);
    memcpy(p_trans_data->w_street_2, w_data.W_STREET_2, 21);
    memcpy(p_trans_data->w_city, w_data.W_CITY, 21);
    memcpy(p_trans_data->w_state, w_data.W_STATE, 3);
    memcpy(p_trans_data->w_zip, w_data.W_ZIP, 10);
    
    w_data.W_YTD += p_trans_data->h_amount;


    /* The row in the DISTRICT table with matching D_W_ID and D_ID is selected.
     * D_NAME, D_STREET_1, D_STREET_2, D_CITY, D_STATE, and D_ZIP are retrieved
     * and D_YTD, the district's year-to-date balance, is increased by H_AMOUNT
     */
    d_key.D_W_ID = p_trans_data->w_id;
    d_key.D_ID = p_trans_data->d_id;
    
    memset(&key, 0, sizeof(DBT));
    key.data = &d_key;
    key.ulen = sizeof(DISTRICT_PRIMARY_KEY);
    key.size = sizeof(DISTRICT_PRIMARY_KEY);
    key.flags = DB_DBT_USERMEM;
	
    memset(&data, 0, sizeof(DBT));
    data.data = &d_data;
    data.ulen = sizeof(DISTRICT_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;
    
    if(err = dbp_district->get(dbp_district, db_txn, &key, &data, 0))
    {
	db_error("payment 3 DB->db_get", err);
	goto abort;
    }

    memcpy(p_trans_data->d_street_1, d_data.D_STREET_1, 21);
    memcpy(p_trans_data->d_street_2, d_data.D_STREET_2, 21);
    memcpy(p_trans_data->d_city, d_data.D_CITY, 21);
    memcpy(p_trans_data->d_state, d_data.D_STATE, 3);
    memcpy(p_trans_data->d_zip, d_data.D_ZIP, 10);
    
    d_data.D_YTD += p_trans_data->h_amount;

    memset(&key, 0, sizeof(DBT));
    key.data = &w_key;
    key.size = sizeof(WAREHOUSE_PRIMARY_KEY);
    
    memset(&data, 0, sizeof(DBT));
    data.data = &w_data;
    data.ulen = sizeof(WAREHOUSE_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;

    if(err = dbp_warehouse->put(dbp_warehouse, db_txn, &key, &data, 0))
    {
	db_error("payment: DB->db_put", err);
	goto abort;
    }
    
    memset(&key, 0, sizeof(DBT));
    key.data = &d_key;
    key.size = sizeof(DISTRICT_PRIMARY_KEY);
	
    memset(&data, 0, sizeof(DBT));
    data.data = &d_data;
    data.ulen = sizeof(DISTRICT_PRIMARY_DATA);
    data.flags = DB_DBT_USERMEM;

    if(err = dbp_district->put(dbp_district, db_txn, &key, &data, 0))
    {
	db_error("DB->db_put", err);
	goto abort;
    }

    /* End transaction */
#if TRANSACTIONS
    if((err = db_txn->commit(db_txn, 0)))
    {
	db_error("payment: DB_TXN->commit", err);
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
