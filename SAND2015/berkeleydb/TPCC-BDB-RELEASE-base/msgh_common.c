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

#include <sys/types.h>
#include <netinet/in.h>
#include <inttypes.h>
#include <errno.h>

#include "msgh_common.h"
#include <db.h>
#include "tpcc_globals.h"
#include "db_checkpoint.h"


/* Berkeley DB environment */
DB_ENV *db_envp;   


/* Global database handles */
DB  *dbp_item;
DB  *dbp_warehouse;
DB  *dbp_customer;
DB  *dbp_customer_sec;
DB  *dbp_district;
DB  *dbp_order;
DB  *dbp_order_sec;
DB  *dbp_neworder;
DB  *dbp_stock;
DB  *dbp_orderline;
DB  *dbp_history;


/* External declarations of functions that execute transactions */
extern int 
neworder_transaction(DB_ENV *db_envp,
                 DB *dbp_warehouse,
                 DB *dbp_district, 
                 DB *dbp_customer, 
                 DB *dbp_neworder, 
                 DB *dbp_stock,
                 DB *dbp_orderline, 
                 DB *dbp_order, 
                 DB *dbp_item, 
                 NEWORDER_TRANSACTION_DATA  *no_trans_data);

extern int
payment_transaction(DB_ENV *db_envp,
                    DB *dbp_warehouse, 
                    DB *dbp_district,
                    DB *dbp_customer, 
                    DB *dbp_customer_sec,
                    DB *dbp_history,
                    PAYMENT_TRANSACTION_DATA *p_trans_data);


extern int
orderstatus_transaction(DB_ENV *db_envp, 
                        DB     *dbp_customer, 
                        DB     *dbp_customer_sec,
                        DB     *dbp_order, 
                        DB     *dbp_order_sec,
                        DB     *dbp_orderline,
                        ORDERSTATUS_TRANSACTION_DATA  *os_trans_data);

extern int
delivery_transaction(DB_ENV *db_envp, 
                     DB  *dbp_neworder, 
                     DB  *dbp_order,
                     DB  *dbp_orderline,
                     DB  *dbp_customer,
                     DELIVERY_TRANSACTION_DATA *d_trans_data);

extern int
stocklevel_transaction(DB_ENV *db_envp, 
                       DB     *dbp_district, 
                       DB     *dbp_orderline, 
                       DB     *dbp_stock,
                       STOCKLEVEL_TRANSACTION_DATA *sl_trans_data);
/////////////////////////////////////////////////////////////////////////

static pthread_t checkpoint_thread;
int thread_started;

/////////////////////////////////////////////////////////////////////////
/*
 * Initialize the environment, open and set up the databases.
 */
int
prepare_for_xactions(char *home_dir, int start_thread)
{
    int err;
    
    /* Initialize the environment */
    if(init_environment(&db_envp, home_dir, DB_INIT_TXN | DB_INIT_LOCK 
	   | DB_INIT_LOG | DB_THREAD ))
    {
	return -1;
    }


    /* Set the nosync flags 
    if((err = db_envp->set_flags(db_envp, DB_TXN_NOSYNC, 1)))
    {
	return err;
    }*/

    /* Open ITEM database */
    if(create_db(db_envp, &dbp_item, DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_item, ITEM_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Open WAREHOUSE database */
    if(create_db(db_envp, &dbp_warehouse,DB_WH_PAGE_SIZE,  0) || open_db(dbp_warehouse, WAREHOUSE_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }
    
    /* Open CUSTOMER database */
    if(create_db(db_envp, &dbp_customer,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_customer, CUSTOMER_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Open the index secondary to CUSTOMER and associate it with the primary */
    if(create_db(db_envp, &dbp_customer_sec,DB_COMMON_PAGE_SIZE, DB_DUP | DB_DUPSORT))
	return -1;
    
    if((err = dbp_customer_sec->set_bt_compare
	(dbp_customer_sec, customer_secondary_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }
    
    if( open_db(dbp_customer_sec, CUSTOMER_SECONDARY_NAME, DB_THREAD))
	return -1;
    
    if ((err = dbp_customer->associate(dbp_customer, 0, dbp_customer_sec, get_customer_sec_key, 0)) != 0)
    {
	db_error("DB->associate failed: %s\n", err);
	return -1;
    }

    /* Open ORDER database */
    if(create_db(db_envp, &dbp_order,DB_COMMON_PAGE_SIZE, 0))
	return -1;
    
    if((err = dbp_order->set_bt_compare
	(dbp_order, order_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if(open_db(dbp_order, ORDER_INDEX_NAME, DB_THREAD))
   	return -1;
    

    /* Open the index secondary to ORDER and associate it with the primary */
    if(create_db(db_envp, &dbp_order_sec,DB_COMMON_PAGE_SIZE, DB_DUP | DB_DUPSORT) || 
       open_db(dbp_order_sec,  ORDER_SECONDARY_NAME, 0)) 
	return -1;
    
    if ((err = dbp_order->associate(dbp_order, 0, dbp_order_sec, get_order_sec_key, 0)) != 0)
    {
	db_error("DB->associate failed: %s\n", err);
	return -1;
    }

    /* Open NEWORDER database */
    if(create_db(db_envp, &dbp_neworder,DB_COMMON_PAGE_SIZE, 0))
    {
	return -1;
    }

    if((err = dbp_neworder->set_bt_compare(dbp_neworder, neworder_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if( open_db(dbp_neworder, NEWORDER_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Open STOCK database */
    if(create_db(db_envp, &dbp_stock,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_stock, STOCK_INDEX_NAME, 0))
    {
	return -1;
    }

    /* Open DISTRICT database and set the custom comparison function */
    if(create_db(db_envp, &dbp_district,DB_DS_PAGE_SIZE, 0))
    {
	return -1;
    }    

    if((err = dbp_district->set_bt_compare(dbp_district, district_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if( open_db(dbp_district, DISTRICT_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Open ORDERLINE database and set the custom comparison function */
    if(create_db(db_envp, &dbp_orderline,DB_COMMON_PAGE_SIZE, 0))
    {
	return -1;
    }
	
    if((err = dbp_orderline->set_bt_compare(dbp_orderline, orderline_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if( open_db(dbp_orderline, ORDERLINE_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Open HISTORY database */
    if(create_db(db_envp, &dbp_history,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_history, HISTORY_INDEX_NAME, DB_THREAD))
    {
	return -1;
    }

    /* Start the checkpoint thread */
    if(start_thread)
    {
	if(start_checkpoint_thread(db_envp, &checkpoint_thread))
	{
	    error("start_checkpoint_thread");
	    return -1;	
	}
	thread_started = 1;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////
/*
 * Close the databases 
 */
void
cleanup(void)
{

    if(dbp_item)
	dbp_item->close(dbp_item, 0);

    if(dbp_warehouse)
	dbp_warehouse->close(dbp_warehouse, 0);

    if(dbp_customer)
	dbp_customer->close(dbp_customer, 0);

    if(dbp_customer_sec)
	dbp_customer_sec->close(dbp_customer_sec, 0);

    if(dbp_district)
	dbp_district->close(dbp_district, 0);

    if(dbp_order)
	dbp_order->close(dbp_order, 0);

    if(dbp_order_sec)
	dbp_order_sec->close(dbp_order_sec, 0);

    if(dbp_neworder)
	dbp_neworder->close(dbp_neworder, 0);

    if(dbp_stock)
	dbp_stock->close(dbp_stock, 0);

    if(dbp_orderline)
	dbp_orderline->close(dbp_orderline, 0);

    if(dbp_history)
	dbp_history->close(dbp_history, 0);
    
    if(thread_started)
    {
	/* Tell the checkpoint thread to exit */
	if(pthread_mutex_lock(&checkpoint_mutex))
	{
	    error("pthread_mutex_lock");
	    goto error;
	}
	checkpoint_exit_flag = 1;
	pthread_mutex_unlock(&checkpoint_mutex);
	pthread_join(checkpoint_thread, NULL);
    }
    
    return;

 error:
    fprintf(stderr, "Cleanup completed with errors\n");
}

int
do_neworder(NEWORDER_TRANSACTION_DATA *notd)
{
    return neworder_transaction(db_envp, 
				dbp_warehouse, 
				dbp_district, 
				dbp_customer,
				dbp_neworder, 
				dbp_stock, 
				dbp_orderline,
				dbp_order, 
				dbp_item, notd);
}

int 
do_payment(PAYMENT_TRANSACTION_DATA *ptd)
{
    return payment_transaction(db_envp, 
			       dbp_warehouse, 
			       dbp_district,
			       dbp_customer, 
			       dbp_customer_sec, 
			       dbp_history, ptd);
}

int
do_orderstatus(ORDERSTATUS_TRANSACTION_DATA *ostd)
{
    return orderstatus_transaction(db_envp, 
				   dbp_customer, 
				   dbp_customer_sec,
				   dbp_order, 
				   dbp_order_sec,
				   dbp_orderline, ostd);

}

int 
do_delivery(DELIVERY_TRANSACTION_DATA *dtd)
{
    
    return delivery_transaction(db_envp, 
				dbp_neworder, 
				dbp_order,
				dbp_orderline,
				dbp_customer, dtd);
}

int 
do_stocklevel(STOCKLEVEL_TRANSACTION_DATA *sltd)
{
    return stocklevel_transaction(db_envp, 
				  dbp_district, 
				  dbp_orderline, 
				  dbp_stock, sltd);
}

/////////////////////////////////////////////////////////////////////////

/*
 * IMPORTANT! 
 * Byte swapping only needs to be done if the TPC server is running
 * on the architecture that's different from the database server.
 * Perhaps this should be compiled conditionally.
 */

typedef char uint8;

double swap_double(double c)
{
	uint8 tmp;
	uint8 *ptr = (uint8 *)&c;
	
	tmp = ptr[0];
	ptr[0] = ptr[7];
	ptr[7] = tmp;
	tmp = ptr[1];
	ptr[1] = ptr[6];
	ptr[6] = tmp;
	tmp = ptr[2];
	ptr[2] = ptr[5];
	ptr[5] = tmp;
	tmp = ptr[3];
	ptr[3] = ptr[4];
	ptr[4] = tmp;
		
	return c;
}
/////////////////////////////////////////////////////////////////////////

void ntoh_msg_header(struct msgh_req *msg)
{
    msg->type = ntohl(msg->type);
    msg->len = ntohl(msg->len);
    msg->client_id = ntohl(msg->client_id);
    msg->pad = ntohl(msg->pad);
}

void hton_msg_header(struct msgh_req *msg)
{
    msg->type = htonl(msg->type);
    msg->len = htonl(msg->len);
    msg->client_id = htonl(msg->client_id);
    msg->pad = htonl(msg->pad);
}

/////////////////////////////////////////////////////////////////////////

void ntoh_delivery(DELIVERY_QUEUE_RECORD *qr)
{
    qr->w_id = ntohl(qr->w_id);
    qr->o_carrier_id = ntohl(qr->o_carrier_id);
    qr->qtime = ntohl(qr->qtime);
}

/////////////////////////////////////////////////////////////////////////

void ntoh_stock(STOCKLEVEL_TRANSACTION_DATA *std)
{
    std->w_id = ntohl( std->w_id);
    std->d_id = ntohl( std->d_id);
    std->threshold = ntohl( std->threshold );
    std->low_stock = ntohl( std->low_stock);
}

void hton_stock(STOCKLEVEL_TRANSACTION_DATA *std)
{
    std->w_id = htonl( std->w_id);
    std->d_id = htonl( std->d_id);
    std->threshold = htonl( std->threshold );
    std->low_stock = htonl( std->low_stock);
}

/////////////////////////////////////////////////////////////////////////

void ntoh_payment( PAYMENT_TRANSACTION_DATA *ptd)
{
    ptd->w_id = ntohl(ptd->w_id);
    ptd->d_id = ntohl(ptd->d_id);
    ptd->c_id = ntohl(ptd->c_id);
    ptd->c_d_id = ntohl(ptd->c_d_id);
    ptd->c_w_id = ntohl(ptd->c_w_id);

    /* Now do the doubles */
    ptd->h_amount = swap_double( ptd->h_amount);
    ptd->c_credit_lim =  swap_double( ptd->c_credit_lim);
    ptd->c_balance = swap_double( ptd->c_balance);
    ptd->c_discount =  swap_double( ptd->c_discount);
}

void hton_payment(PAYMENT_TRANSACTION_DATA *ptd)
{
    ntoh_payment(ptd);
}

/////////////////////////////////////////////////////////////////////////

void ntoh_orderstatus_item(ORDERSTATUS_ITEM_INFO *oid)
{
    oid->ol_supply_w_id = ntohl(oid->ol_supply_w_id);
    oid->ol_i_id = ntohl(oid->ol_i_id); 
    oid->ol_quantity = ntohl(oid->ol_quantity);
    oid->ol_amount = swap_double(oid->ol_amount);
}

void ntoh_orderstatus(ORDERSTATUS_TRANSACTION_DATA *otd)
{
    int i;

    otd->w_id = ntohl(otd->w_id );
    otd->d_id = ntohl(otd->d_id );
    otd->c_id = ntohl(otd->c_id );
    otd->o_id = ntohl(otd->o_id );
    otd->o_carrier_id = ntohl(otd->o_carrier_id);
    otd->item_cnt = ntohl(otd->item_cnt);
    otd->c_balance = swap_double(otd->c_balance);

    for( i = 0; i<15; i++)
    {
	ntoh_orderstatus_item(&(otd->order_data[i]));
    }
}

void hton_orderstatus(ORDERSTATUS_TRANSACTION_DATA *otd)
{
    ntoh_orderstatus(otd);
}

/////////////////////////////////////////////////////////////////////////

void ntoh_neworder_item(NEWORDER_ITEM_INFO *ni)
{
    ni->ol_supply_w_id = ntohl(ni->ol_supply_w_id);
    ni->ol_i_id        = ntohl(ni->ol_i_id);
    ni->ol_quantity    = ntohl(ni->ol_quantity);
    ni->s_quantity     = ntohl(ni->s_quantity);
    ni->i_price        = swap_double(ni->i_price );
    ni->ol_amount      = swap_double(ni->ol_amount);
}

void ntoh_neworder(NEWORDER_TRANSACTION_DATA *ntd)
{
    int i;
    
    ntd->w_id      = ntohl(ntd->w_id);
    ntd->d_id      = ntohl(ntd->d_id);
    ntd->c_id      = ntohl(ntd->c_id);     
    ntd->o_id      = ntohl(ntd->o_id);
    ntd->o_ol_cnt  = ntohl(ntd->o_ol_cnt);
    
    ntd->c_discount = swap_double(ntd->c_discount);
    ntd->w_tax      = swap_double(ntd->w_tax);
    ntd->d_tax      = swap_double(ntd->d_tax);

    for( i = 0; i<15; i++)
    {
	ntoh_neworder_item(&(ntd->order_data[i]));
    }

    ntd->total      = swap_double(ntd->total);
}

void hton_neworder(NEWORDER_TRANSACTION_DATA *ntd)
{
    ntoh_neworder(ntd);
}
