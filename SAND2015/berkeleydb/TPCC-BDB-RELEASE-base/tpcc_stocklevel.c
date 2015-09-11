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

/* This file contains the implementation of TPCC STOCK LEVEL transaction
 *  on Berkeley DB */

#include <errno.h>
#include <time.h>

#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_schema.h"
#include "tpcc_trans.h"

#define HASH_SIZE 50

struct item
{
    int id;
    int quantity;
    struct item *next;
};


int   add_item(struct item *hash_table[], int id, int quantity);
void init_hash_table(struct item *hash_table[]);
int  uninit_and_count(struct item *hash_table[], int threshold);

/////////////////////////////////////////////////////////////////////////
int
stocklevel_transaction(DB_ENV *db_envp, 
		       DB     *dbp_district, 
		       DB     *dbp_orderline, 
		       DB     *dbp_stock,
		       STOCKLEVEL_TRANSACTION_DATA *sl_trans_data)
{
    int err, i, count;
    struct item *hash_table[HASH_SIZE];
    
    DB_TXN *db_txn = 0;
    DBT key, data;

    DISTRICT_PRIMARY_KEY   d_key;
    DISTRICT_PRIMARY_DATA  d_data;
    ORDERLINE_PRIMARY_KEY  ol_key;
    ORDERLINE_PRIMARY_DATA ol_data;
    STOCK_PRIMARY_KEY      s_key;
    STOCK_PRIMARY_DATA     s_data;

    DBC *ol_cursor;
    struct timespec before, after;
    
    assert( (dbp_orderline && dbp_district && dbp_stock
	     && sl_trans_data));


    init_hash_table(hash_table);

    /* Begin transaction */
#if TRANSACTIONS
    if((err = db_envp->txn_begin(db_envp, 0, &db_txn, 0)))
    {
	db_error("DB_ENV->txn_begin", err);
	goto done;
    }
#endif
 
    /* The row in the DISTRICT table with matching D_W_ID and D_ID
     * is selected and D_NEXT_O_OID is retrieved.
     */
    d_key.D_W_ID = sl_trans_data->w_id;
    d_key.D_ID = sl_trans_data->d_id;
    
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
	db_error("stocklevel 1 DB->db_get", err);
	goto abort;
    }
    
    /* All rows in the ORDER_LINE table with matching OL_W_ID (equals W_ID), 
     * OL_D_ID (equals D_ID), and OL_O_ID (lower than D_NEXT_O_ID and greater
     * than or equal to D_NEXT_O_ID minus 20) are selected. They are the items
     * for 20 recent orders of the district. 
     */
    
    /* Init the cursor */
    if ((err = dbp_orderline->cursor(dbp_orderline, db_txn, &ol_cursor, 0)))
    {
	db_error("DB->cursor", err);
	goto abort;
    }
    
    ol_key.OL_W_ID = sl_trans_data->w_id;
    ol_key.OL_D_ID = sl_trans_data->d_id;
    ol_key.OL_O_ID = d_data.D_NEXT_O_ID;
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

    clock_gettime(CLOCK_REALTIME, &before);
    count = 0;
    while(1)
    {
	DBT stock_key, stock_data;

	if(err = ol_cursor->c_get(ol_cursor, &key, &data, 
			       count == 0? DB_SET_RANGE:DB_NEXT))
	{
	    db_error("stock-level DBC->c_get", err);
	    ol_cursor->c_close(ol_cursor);
	    goto abort;
	}
	
	if(! (ol_key.OL_W_ID == d_key.D_W_ID &&
	      ol_key.OL_D_ID == d_key.D_ID &&
	      ol_key.OL_O_ID >= (d_data.D_NEXT_O_ID-20)))
	{
	    break;
	}
	count++;

	/* All rows in the STOCK table with matching S_I_ID (equals OL_I_ID) and S_W_ID
	 * (equals W_ID) from the list of distinct item numbers and with S_QUANTITY lower than 
	 * threshold are counted (giving low_stock).
	 */
	s_key.S_W_ID = sl_trans_data->w_id;
	s_key.S_I_ID = ol_data.OL_I_ID;
	
	memset(&stock_key, 0, sizeof(DBT));
	stock_key.data = &s_key;
	stock_key.ulen = sizeof(STOCK_PRIMARY_KEY);
	stock_key.size = sizeof(STOCK_PRIMARY_KEY);
	stock_key.flags = DB_DBT_USERMEM;

	memset(&stock_data, 0, sizeof(DBT));
	stock_data.data = &s_data;
	stock_data.ulen = sizeof(STOCK_PRIMARY_DATA);
	stock_data.flags = DB_DBT_USERMEM;
	
	if(err = dbp_stock->get(dbp_stock, db_txn, &stock_key, &stock_data, 0))
	{
	    db_error("stocklevel 2 DB->db_get", err);
	    ol_cursor->c_close(ol_cursor);
	    goto abort;
	}
	
	if(add_item(hash_table, s_key.S_I_ID, s_data.S_QUANTITY))
	{
	    ol_cursor->c_close(ol_cursor);
	    err = errno;
	    db_error("add_item", err);
	    goto abort;
	}
    }

    clock_gettime(CLOCK_REALTIME, &after);
    //printf("spent %" PRId64 " ns in first PART (count=%d)\n", 
    //	   after - before, count);
    //    if(count == 0)
      //      printf("Warning! Count = 0 in stocklevel!\n");

    /* Close the cursor */
    if(err= ol_cursor->c_close(ol_cursor))
    {
	db_error("DBC->c_close", err);
	goto abort;
    }

    /* Calculate the items that are below the threshold */
    {
      clock_gettime(CLOCK_REALTIME, &before);
      sl_trans_data->low_stock = uninit_and_count(hash_table, sl_trans_data->threshold);
      clock_gettime(CLOCK_REALTIME, &after);
      //printf("spent %" PRId64 " ns in uninit_and_count\n", after - before);
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
	err = db_txn->abort(db_txn);
	if(err)
	{
	    db_error("FATAL ERROR: DB_TXN->abort", err);
	}
    }

 done:
    return err;
}

/////////////////////////////////////////////////////////////////////////

/* Some hash table routines */

void
init_hash_table(struct item *table[])
{
    memset(table, 0, sizeof(struct item *)*HASH_SIZE);
}

int
uninit_and_count(struct item *table[], int threshold)
{
    int i, low_stock = 0;

    for(i = 0; i< HASH_SIZE; i++)
    {
	struct item *it = table[i];
	
	if(it == NULL)
	    continue;
	    
	do
	{
	    struct item *tmp = it;
	    
	    if(it->quantity < threshold)
		low_stock++;

	    it = tmp->next;
	    free(tmp);

	}while(it != NULL);
    }
    return low_stock;
}

int
add_item(struct item *hash_table[], int id, int quantity)
{
    int slot;
    struct item *it, *item;
    
    slot = id%HASH_SIZE;
    
    /* Chain the item to this slot */
    if(hash_table[slot] == NULL)
    {
	item = (struct item*)malloc(sizeof(struct item));

	if(item == NULL)
	{
	    error("malloc");
	    return -1;
	}

	memset(item, 0, sizeof(struct item));
	item->id = id;
	item->quantity = quantity;
	
	hash_table[slot] = item;
	return 0;
    }

    /* Slot is occupied, have to chain the item */
    it = hash_table[slot];

    while(1)
    {
	if(it->id == id)
	{
	    it->quantity += quantity;
	    return 0;
	}
	if(it->next == NULL)
	{
	    item = (struct item*)malloc(sizeof(struct item));

	    if(item == NULL)
	    {
		error("malloc");
		return -1;
	    }

	    memset(item, 0, sizeof(struct item));
	    item->id = id;
	    item->quantity = quantity;
	    
	    it->next = item;
	    return 0;
	}
	it = it->next;
    }
    
}
	    
		
