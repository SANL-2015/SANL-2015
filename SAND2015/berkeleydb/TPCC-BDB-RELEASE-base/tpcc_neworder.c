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

/* This file contains the implementation of TPCC NEW ORDER transaction
 *  on Berkeley DB */

#include <errno.h>
#include <time.h>

#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_schema.h"
#include "tpcc_trans.h"


/////////////////////////////////////////////////////////////////////////
int
neworder_transaction(DB_ENV *db_envp,
		 DB *dbp_warehouse,
		 DB *dbp_district, 
		 DB *dbp_customer, 
		 DB *dbp_neworder, 
		 DB *dbp_stock,
		 DB *dbp_orderline, 
		 DB *dbp_order, 
		 DB *dbp_item, 
		 NEWORDER_TRANSACTION_DATA  *no_trans_data)
{
    const int dt_size = 26;
    char datetime[dt_size];
    int err, i;
    time_t t_clock;
    char *STATUS_SUCCESS_STRING = "Transaction successful";

    DB_TXN *db_txn = 0;
    DBT key, data;    

    /* Remember OL_DIST_INFO fields, which we retrieve in the
     * middle of transaction, but don't actually write until
     * the end. */
    char ol_dist_info[15][25];


    WAREHOUSE_PRIMARY_KEY   w_key;
    WAREHOUSE_PRIMARY_DATA  w_data;
    DISTRICT_PRIMARY_KEY    d_key;
    DISTRICT_PRIMARY_DATA   d_data;
    CUSTOMER_PRIMARY_KEY    c_key;
    CUSTOMER_PRIMARY_DATA   c_data;
    NEWORDER_PRIMARY_KEY    no_key;
    ORDER_PRIMARY_KEY       o_key;
    ORDER_PRIMARY_DATA      o_data;
    
    int w_tax, d_tax, d_next_o_oid;
    
    if((int)time(&t_clock) == -1)
    {
	error("time");
	return errno;
    }

    ctime_r(&t_clock, (char*)datetime);

    assert( (dbp_warehouse && dbp_district && dbp_customer && dbp_neworder
	     && dbp_stock && dbp_orderline && dbp_order && dbp_item 
	     && no_trans_data) );

    /* Begin transaction */
#if TRANSACTIONS
    if((err = db_envp->txn_begin(db_envp, 0, &db_txn, 0)))
    {
	db_error("DB_ENV->txn_begin", err);
	goto done;
    }
#endif

    /* The row in the WAREHOUSE table with matching W_ID is selected and W_TAX
     * rate is retrieved. */
    w_key.W_ID = no_trans_data->w_id;
    
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
		 db_error("neworder-warehouse:DB->get", err);
		 goto abort;
    }

    w_tax = w_data.W_TAX;
    
    
    /* The row in the DISTRICT table with matching D_W_ID and D_ID is selected, 
     * D_TAX, the district tax rate is retrieved, and D_NEXT_OID, the next available 
     * order number for the district, is retireved and incremented by one. 
     */
    d_key.D_W_ID = w_key.W_ID;
    d_key.D_ID   = no_trans_data->d_id;
    
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
		 db_error("neworder-district:DB->get", err);
		 goto abort;
    }
    
    d_tax = d_data.D_TAX;
    d_next_o_oid = d_data.D_NEXT_O_ID;

    d_data.D_NEXT_O_ID++;

    /* The row in the customer table with matching C_W_ID, C_D_ID and C_ID 
     * is selected
     * and C_DISCOUNT, the customer's discount rate, C_LAST, the customer's 
     * last name, 
     * and C_CREDIT, the customer's credit status are retrieved.
     */
    c_key.C_W_ID = w_key.W_ID;
    c_key.C_D_ID = d_key.D_ID;
    c_key.C_ID   = no_trans_data->c_id;


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
		 db_error("neworder-customer:DB->get", err);
		 goto abort;
    }
    
    /* A new row is inserted into both the NEW_ORDER table and the ORDER 
     * table to reflect the creation of the new order. O_CARRIER_ID is set
     * to a null value. If the order includes only home order-lines, then 
     * O_ALL_LOCAL is set to 1, otherwise O_ALL_LOCAL is set to 0.
     */
    no_key.NO_W_ID = w_key.W_ID;
    no_key.NO_D_ID = d_key.D_ID;
    no_key.NO_O_ID = d_next_o_oid;
    
    o_key.O_W_ID = w_key.W_ID;
    o_key.O_D_ID = d_key.D_ID;
    o_key.O_ID   = d_next_o_oid;


    memset(&o_data, 0, sizeof(ORDER_PRIMARY_DATA));
    o_data.O_C_ID = c_key.C_ID;
    o_data.O_CARRIER_ID = 0;
    memcpy(o_data.O_ENTRY_D, datetime, 19); o_data.O_ENTRY_D[19] = 0;
    o_data.O_OL_CNT = no_trans_data->o_ol_cnt;
    o_data.O_ALL_LOCAL = 1;    

    /* Are all order lines local? */
    for(i = 0; i<no_trans_data->o_ol_cnt; i++)
    {
	if(no_trans_data->order_data[i].ol_supply_w_id != w_key.W_ID)
	{
	    o_data.O_ALL_LOCAL = 0;
	    break;
	}	    
    }
    
    /* For each O_OL_CNT item in the order */
    for(i = 0; i<no_trans_data->o_ol_cnt; i++)
    {
	/* The row in the ITEM table with matching I_ID
	 * (equals OL_I_ID) is selcted and I_PRICE, the price
	 * of the item, I_NAME, the name of the item, and I_DATA
	 * are retrieved. If I_ID has an unused value (see
	 * Clause 2.4.1.5), a "not-found" condition is signaled, 
	 * resulting in a rollback of the database transaction.
	 */
	ITEM_PRIMARY_KEY       i_key;
	ITEM_PRIMARY_DATA      i_data;
	STOCK_PRIMARY_KEY      s_key;
	STOCK_PRIMARY_DATA     s_data;
	ORDERLINE_PRIMARY_KEY  ol_key;
	ORDERLINE_PRIMARY_DATA ol_data;
	


	i_key.I_ID = no_trans_data->order_data[i].ol_i_id;
	
	memset(&key, 0, sizeof(DBT));
	key.data = &i_key;
	key.size = sizeof(ITEM_PRIMARY_KEY);
	key.ulen = sizeof(ITEM_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;
    
	memset(&data, 0, sizeof(DBT));
	data.data = &i_data;
	data.ulen = sizeof(ITEM_PRIMARY_DATA);
	data.flags = DB_DBT_USERMEM;
    
	if(err = dbp_item->get(dbp_item, db_txn, &key, &data, 0))
	{
	    db_error("neworder-item:DB->get", err);
	    goto abort;
	}
	
	/* The row in the STOCK table with matching S_I_ID (equals OL_I_ID) and 
	 * S_W_ID (equals OL_SUPPLY_W_ID) is selected. S_QUANTITY, the quantity in stock, 
	 * S_DIST_xx, where xx represents the district number, and S_DATA are retrieved.
	 */

	s_key.S_W_ID = no_trans_data->order_data[i].ol_supply_w_id;
	s_key.S_I_ID = i_key.I_ID;
	
	memset(&key, 0, sizeof(DBT));
	key.data = &s_key;
	key.size = sizeof(STOCK_PRIMARY_KEY);
	key.ulen = sizeof(STOCK_PRIMARY_KEY);
	key.flags = DB_DBT_USERMEM;
    
	memset(&data, 0, sizeof(DBT));
	data.data = &s_data;
	data.ulen = sizeof(STOCK_PRIMARY_DATA);
	data.flags = DB_DBT_USERMEM;

	if(err = dbp_stock->get(dbp_stock, db_txn, &key, &data, 0))
	{
	    db_error("neworder-stock:DB->get", err);
	    goto abort;
	}

 
	/* If the retrieved value for S_QUANTITY exceeds OL_QUANTITY by 10 or more, then 
	 * S_QUANTITY is decreased by OL_QUANTITY; otherwise S_QUANTITY is updated to 
	 * (S_QUANTITY - OL_QUANTITY) + 91. S_YTD is increased
	 * by OL_QUANTITY and S_ORDER_CNT is incremented by 1. If the order-line is remote, 
	 * then S_REMOTE_CNT is incremented by 1. 
	 */
	
	if(s_data.S_QUANTITY - no_trans_data->order_data[i].ol_quantity >= 10)
	{
	    s_data.S_QUANTITY -= no_trans_data->order_data[i].ol_quantity;
	}
	else
	{
	    s_data.S_QUANTITY = 
		(s_data.S_QUANTITY - no_trans_data->order_data[i].ol_quantity) +91;
	}
	s_data.S_YTD += no_trans_data->order_data[i].ol_quantity;
	s_data.S_ORDER_CNT++;

	if(no_trans_data->order_data[i].ol_supply_w_id != w_key.W_ID)
	{
	    s_data.S_REMOTE_CNT++;
	}
	
	    
	if(err = dbp_stock->put(dbp_stock, db_txn, &key, &data, 0))
	{
	    db_error("DB->put", err);
	    goto abort;
	}
	
	/* The amount for the item in the order (OL_AMOUNT) is computed as:
	 * OL_QUANTITY * I_PRICE */
	ol_data.OL_AMOUNT =  
	    no_trans_data->order_data[i].ol_quantity * i_data.I_PRICE;
	
	no_trans_data->total += ol_data.OL_AMOUNT;
	
	/* The strings I_DATA and S_DATA are examined. If they both include
	 * the string "ORIGINAL", the brand-generic field for that item is
	 * set to "B", otherwise, the brand-generic field is set to "G".
	 */
	if(strstr((const char*)&i_data.I_DATA, "original") 
	   && strstr((const char*) &s_data.S_DATA, "original"))
	{
	    no_trans_data->order_data[i].brand[0] = 'B';
	}
	else
	{
	    no_trans_data->order_data[i].brand[0] = 'G';
	}
	no_trans_data->order_data[i].brand[1] = '\0';
	
	memcpy(ol_dist_info[i], (s_data.S_DIST_01) + (d_key.D_ID-1)*25, 25);
	
	/* Fill the remaining output data structures */
	no_trans_data->order_data[i].ol_supply_w_id  = ol_data.OL_SUPPLY_W_ID;
	no_trans_data->order_data[i].ol_i_id         = i_key.I_ID;
	memcpy(no_trans_data->order_data[i].i_name,  i_data.I_NAME, 25);
	no_trans_data->order_data[i].ol_quantity     = ol_data.OL_QUANTITY;
	no_trans_data->order_data[i].s_quantity      = s_data.S_QUANTITY;
	no_trans_data->order_data[i].i_price         = i_data.I_PRICE;
	no_trans_data->order_data[i].ol_amount       = ol_data.OL_AMOUNT;
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
	db_error("DB->put", err);
	goto abort;
    }
    
    /*************************/
    /* A new row is inserted into the ORDER_LINE table to reflect the
     * item on the order. OL_DELIVERY_D is set to a null value, OL_NUMBER
     * is set to a unique value within all the ORDER_LINE rows that have
     * the same OL_O_ID value, and OL_DIST_INFO is set to the content of
     * S_DIST_xx, where xx represents the district number OL_D_ID.
     */
    for( i = 0; i<no_trans_data->o_ol_cnt; i++)
    {
	ORDERLINE_PRIMARY_KEY  ol_key;
	ORDERLINE_PRIMARY_DATA ol_data;
    	
	ol_key.OL_W_ID   = w_key.W_ID;
	ol_key.OL_D_ID   = d_key.D_ID;
	ol_key.OL_O_ID   = o_key.O_ID;
	ol_key.OL_NUMBER = i+1;
	
	memset(&ol_data, 0, sizeof(ORDERLINE_PRIMARY_DATA));	
	
	
	ol_data.OL_I_ID          =  no_trans_data->order_data[i].ol_i_id  ;
	ol_data.OL_SUPPLY_W_ID   =  no_trans_data->order_data[i].ol_supply_w_id;
	ol_data.OL_DELIVERY_D[0] = '\0';
	ol_data.OL_QUANTITY      =  no_trans_data->order_data[i].ol_quantity;
	memcpy(ol_data.OL_DIST_INFO, ol_dist_info[i], 25);
	

	memset(&key, 0, sizeof(DBT));
	key.data = &ol_key;
	key.size = sizeof(ORDERLINE_PRIMARY_KEY);
    
	memset(&data, 0, sizeof(DBT));
	data.data = &ol_data;
	data.size = sizeof(ORDERLINE_PRIMARY_DATA);
	

	if(err = dbp_orderline->put(dbp_orderline, db_txn, &key, &data, 0))
	{
	    db_error("DB->put", err);
	    goto abort;
	}
    }

    /*************************/

    memset(&key, 0, sizeof(DBT));
    key.data = &no_key;
    key.size = sizeof(NEWORDER_PRIMARY_KEY);
    
    memset(&data, 0, sizeof(DBT));
    data.flags = DB_DBT_USERMEM;

    if(err = dbp_neworder->put(dbp_neworder, db_txn, &key, &data, 0))
    {
	db_error("DB->put", err);
	goto abort;
    }

    memset(&key, 0, sizeof(DBT));
    key.data = &o_key;
    key.size = sizeof(ORDER_PRIMARY_KEY);
    
    memset(&data, 0, sizeof(DBT));
    data.data = &o_data;
    data.size = sizeof(ORDER_PRIMARY_DATA);

    if(err = dbp_order->put(dbp_order, db_txn, &key, &data, 0))
    {
	db_error("DB->put", err);
	goto abort;
    }
    /*************************/

    /* End transaction */
#if TRANSACTIONS
    if((err = db_txn->commit(db_txn, 0)))
    {
	db_error("DB_TXN->commit", err);
	goto abort;
    }
#endif
    
    /* Fill the non-repeating field in the output data structure */
    no_trans_data->o_id         = o_key.O_ID;
    memcpy(no_trans_data->c_last, c_data.C_LAST, 17);
    memcpy(no_trans_data->c_credit, c_data.C_CREDIT, 3);
    no_trans_data->c_discount   = c_data.C_DISCOUNT;
    no_trans_data->w_tax        = w_tax;
    no_trans_data->d_tax        = d_tax;
    memcpy(no_trans_data->o_entry_d, o_data.O_ENTRY_D, 19);
    no_trans_data->total *= ((1-c_data.C_DISCOUNT)*(1 + w_tax * d_tax));
    memcpy(no_trans_data->status, STATUS_SUCCESS_STRING, 
	   strlen(STATUS_SUCCESS_STRING)); 
    
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

