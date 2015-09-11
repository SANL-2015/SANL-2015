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
/* Ported to Linux x86_64 by Justin Funston */

#include <sys/types.h>
#include <errno.h>
#include <time.h>
#include <sys/mman.h>
#include <signal.h>
/*#include <sys/procset.h>
#include <sys/processor.h>
#include <sys/priocntl.h>*/
#include <pthread.h>
#include <poll.h>
#include <assert.h>
/*#include <sys/lwp.h>*/

#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_trans.h"
#include "db_checkpoint.h"
#include "db_trickle.h"
#if defined (__sparc)
#include "magic-instruction.h"
#include "inline.h"
#endif
/** +EDIT */
#ifdef BENCHMARK
#include "liblock.h"
#endif
/** -EDIT */

#define PRINTALOT 0
#define PRINT_TX_STATUS 0
#define DELIVERY_NONRANDOM 0

#define SYNC_MPOOL 0

#define NS_IN_MS  1000000
#define US_IN_MS  1000
#define MS_IN_SEC 1000

/* Names of transaction types */
char *tx_names[] = {"Mixed", "Payment", "New Order", "Order Status", 
		  "Delivery", "Stock Level"};


typedef struct
{
    txn_type_t txn_type;
    /*processorid_t cpu_id;*/
    int iterations;
    int report_perf_sim;
    pthread_t tid;
} thread_data_t;

/** +EDIT */
//#define MAX_THREADS 16
#define MAX_THREADS 1024
/** -EDIT */

static thread_data_t bench_threads[MAX_THREADS];
static int num_bench_threads;

/* Global flags */
static int option_debug;
static int checkpoint;
static int do_trickle;
static int do_scan;
static int private;
static int no_locks;

/* Berkeley DB environment */
DB_ENV *db_envp;   

/* Type of transaction we are running */
int my_type;

/* Boolean indicating whether we are trying to achieve a 
 * a certain transaction rate */
int rate_limit;
double tx_target_rate;  /* In transactions per second */

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

extern int do_autocommit;

void     scan_all_db(void);

#if defined (__sparc) 
uint64_t exec_magic_200();
uint64_t do_quit();
uint64_t report_data_to_simulator(uint64_t data);

/* When this server runs in the "benchmark" mode on the Simics simulator
 * we use this function to synchronize all concurrent threads
 */
static uint64_t
others_arrived()
{
    return exec_magic_200();
}
#endif

static void loop_same_txn(void *param);

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
static pthread_t trickle_thread;
/////////////////////////////////////////////////////////////////////////
/*static void bind_break_and_wait(processorid_t cpu_id);
static void maybe_bind_me(processorid_t cpu_id);*/
/*static void report_my_lwpusage(char *thread_name);*/

#define PSIZE 4096

int
lock_cache(void *ptr, int size)
{
    int retval = 0;

  
    /* 
     * The memory that we mlock has to be page-aligned: both
     * the address and size. Check that this is the case
     */
    if((long long)ptr % PSIZE != 0 || size % PSIZE != 0)
    {
	fprintf(stderr, "dbaio register cache: unaligned address or size\n");
	return -1;
    }

    /* Mlock the memory. */
    fprintf(stderr, "Locking cache: ptr is %x, size is %d", 
	    ptr, size);
    retval =  mlock(ptr, size);
    if(retval)
    {
	return retval;
    }
    
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////
/* If we are doing a checkpoint, we must hang around until told to
 * exit by SIGHUP
 */
int must_exit = 0;

void
sighup_handle(int signal)
{
    must_exit = 1;
}

/////////////////////////////////////////////////////////////////////////
/*
 * Initialize the environment, open and set up the databases.
 */
static int
prepare_for_xactions(char *home_dir, int recover)
{
    int err;
    int flags = 0;
    int autocomm_flag = 0; 
    int db_thread_flag = 0; 

    /* This initializes the log file */
    /*write_log("Begin\n");*/
    
#if TRANSACTIONS
    autocomm_flag = DB_AUTO_COMMIT;
    db_thread_flag = DB_THREAD;
#endif
    
    if(private)
	flags = DB_PRIVATE;
    
    if(flags & DB_PRIVATE)
    {
	printf("Will initialize a privately accessed DB environment.\n");
    }

    if(recover)
    {
	flags |= DB_RECOVER;
    }


    /* Initialize the environment */
#if TRANSACTIONS
    if(init_environment(&db_envp, home_dir, DB_INIT_TXN | DB_INIT_LOCK 
	   | DB_INIT_LOG | DB_THREAD | flags))
    {
	return -1;
    }
#else
    if(no_locks)
    {
	if(init_environment(&db_envp, home_dir, 
			    DB_INIT_CDB | DB_THREAD | flags))
	    return -1;
    }
    else
    {
	if(init_environment(&db_envp, home_dir, 
			    DB_INIT_LOCK| DB_THREAD | flags))
	    return -1;
    }
#endif
    
    
    /* Open ITEM database */
    if(create_db(db_envp, &dbp_item, DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_item, ITEM_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }

    /* Open WAREHOUSE database */
    if(create_db(db_envp, &dbp_warehouse,DB_WH_PAGE_SIZE, 0) || open_db(dbp_warehouse, WAREHOUSE_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }
    
    /* Open CUSTOMER database */
    if(create_db(db_envp, &dbp_customer,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_customer, CUSTOMER_INDEX_NAME, db_thread_flag))
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
    
    if( open_db(dbp_customer_sec, CUSTOMER_SECONDARY_NAME, db_thread_flag))
	return -1;
    
    if ((err = dbp_customer->associate(dbp_customer, 0, dbp_customer_sec, get_customer_sec_key, autocomm_flag)) != 0)
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

    if(open_db(dbp_order, ORDER_INDEX_NAME, db_thread_flag))
   	return -1;
    

    /* Open the index secondary to ORDER and associate it with the primary */
    if(create_db(db_envp, &dbp_order_sec,DB_COMMON_PAGE_SIZE, DB_DUP | DB_DUPSORT) || 
       open_db(dbp_order_sec,  ORDER_SECONDARY_NAME, db_thread_flag)) 
	return -1;
    
    if ((err = dbp_order->associate(dbp_order, 0, dbp_order_sec, get_order_sec_key, autocomm_flag)) != 0)
    {
	db_error("DB->associate failed: %s\n", err);
	return -1;
    }

    /* Open NEWORDER database */
    if(create_db(db_envp, &dbp_neworder, DB_COMMON_PAGE_SIZE,0))
    {
	return -1;
    }

    if((err = dbp_neworder->set_bt_compare(dbp_neworder, neworder_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if( open_db(dbp_neworder, NEWORDER_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }

    /* Open STOCK database */
    if(create_db(db_envp, &dbp_stock,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_stock, STOCK_INDEX_NAME, db_thread_flag))
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

    if( open_db(dbp_district, DISTRICT_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }

    /* Open ORDERLINE database and set the custom comparison function */
    if(create_db(db_envp, &dbp_orderline,DB_OL_PAGE_SIZE, 0))
    {
	return -1;
    }
	
    if((err = dbp_orderline->set_bt_compare(dbp_orderline, orderline_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	return -1;
    }

    if( open_db(dbp_orderline, ORDERLINE_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }

    /* Open HISTORY database */
    if(create_db(db_envp, &dbp_history,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_history, HISTORY_INDEX_NAME, db_thread_flag))
    {
	return -1;
    }

    if(do_scan)
	scan_all_db();

    /* Start the checkpoint thread */
    if(checkpoint)
    {
	printf("Will start the checkpoint thread ...\n");
	if( signal(SIGHUP, sighup_handle) == SIG_ERR)
	{
	    error("signal");
	    return -1;
	}
	if(start_checkpoint_thread(db_envp, &checkpoint_thread))
	{
	    error("start_checkpoint_thread");
	    return -1;	
	}
    }


    /* Start the trickle thread */
    if(do_trickle)
    {
	printf("Will start the trickle thread ...\n");
	if( signal(SIGHUP, sighup_handle) == SIG_ERR)
	{
	    error("signal");
	    return -1;
	}
	if(start_trickle_thread(db_envp, &trickle_thread))
	{
	    error("start_trickle_thread");
	    return -1;	
	}
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////
/*
 * Sequentially scan all databases to read them into the memory pool.
 */
void
scan_all_db(void)
{
    scan_db(dbp_item);
    scan_db(dbp_warehouse);
    scan_db(dbp_customer);
    scan_db(dbp_customer_sec);
    scan_db(dbp_district);
    scan_db(dbp_order);
    scan_db(dbp_order_sec);
    scan_db(dbp_neworder);
    scan_db(dbp_stock);
    scan_db(dbp_orderline);
    scan_db(dbp_history);
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
    
    /* Tell the checkpoint thread to exit */
    if(checkpoint)
    {	
	/* Hang around until we are told to exit */
	while(!must_exit)
	{
	    poll(0, 0, 5000);
	}

	if(pthread_mutex_lock(&checkpoint_mutex))
	{
	    error("pthread_mutex_lock");
	    goto error;
	}

	checkpoint_exit_flag = 1;
	pthread_mutex_unlock(&checkpoint_mutex);
	pthread_join(checkpoint_thread, NULL);
    }

      /* Tell the checkpoint thread to exit */
    if(do_trickle)
    {	
	/* Hang around until we are told to exit */
	while(!must_exit)
	{
	    poll(0, 0, 5000);
	}

	if(pthread_mutex_lock(&trickle_mutex))
	{
	    error("pthread_mutex_lock");
	    goto error;
	}

	trickle_exit_flag = 1;
	pthread_mutex_unlock(&trickle_mutex);
	pthread_join(trickle_thread, NULL);
    }

    close_environment(db_envp);
    return;

 error:
    fprintf(stderr, "Cleanup completed with errors\n");
}

/////////////////////////////////////////////////////////////////////////
double
get_delta_ms(struct timeval *before, struct timeval *after)
{
    double retval = 0;
    long diff_sec = 0, diff_usec = 0;

    if(before == NULL || after == NULL)
    {
	return -1000000;
    }

    diff_sec = after->tv_sec - before->tv_sec;
    
    if( (diff_usec = after->tv_usec - before->tv_usec) < 0)
    {
	if(diff_sec <= 0)
	{
	    fprintf(stderr, "Invalid input\n");
	}
	diff_sec--;
	diff_usec += 1000000;
    }
    
    retval = (double)diff_sec * (double)1000 + (double)diff_usec/(double)1000;
    return retval;
}

/////////////////////////////////////////////////////////////////////////
/*
 * Various routines to test TPC transactions locally
 */
/////////////////////////////////////////////////////////////////////////
int MAX_WID = 1;
int W_ID = -1;
int D_ID = -1;

void 
Lastname(int num, char *name)
{
    int i;
    static char *n[] = 
    {"BAR", "OUGHT", "ABLE", "PRI", "PRES", 
     "ESE", "ANTI", "CALLY", "ATION", "EING"};

    strcpy(name,n[num/100]);
    strcat(name,n[(num/10)%10]);
    strcat(name,n[num%10]);
    
    i = strlen(name);
    for(; i<16; i++)
    {
	name[i] = ' ';
    }
    name[16] = '\0';
    return;
}

void
generate_orderstatus(ORDERSTATUS_TRANSACTION_DATA *ostd)
{
    int y;

    if(W_ID != -1)
    	ostd->w_id = W_ID;
    else
	ostd->w_id = random1(1, MAX_WID);
    
    if(D_ID != -1)
	ostd->d_id = D_ID;
    else
	ostd->d_id = random1(1, DISTRICTS_PER_WAREHOUSE);
    
    y = random1(1, 100);
    
    if(y <= 60)
    	Lastname(NURand(255,0,999), ostd->c_last);
    else
    	ostd->c_id = NURand(1023, 1, 3000);
    
}

/* Prints the data returned from the ORDERSTATUS transaction */
void
print_orderstatus_data(ORDERSTATUS_TRANSACTION_DATA *ostd)
{
    int i;
    
    fprintf(stderr, "ORDERSTATUS TRANSACTION DATA:\n");
    fprintf(stderr, "\tw_id:         %d\n", ostd->w_id);
    fprintf(stderr, "\td_id:         %d\n", ostd->d_id);
    fprintf(stderr, "\tc_id:         %d\n", ostd->c_id);
    fprintf(stderr, "\to_id:         %d\n", ostd->o_id);
    fprintf(stderr, "\to_carrier_id: %d\n", ostd->o_carrier_id);
    fprintf(stderr, "\titem_cnt:     %d\n", ostd->item_cnt);
    fprintf(stderr, "\tc_balance:    %f\n", ostd->c_balance);
    fprintf(stderr, "\tc_first:      %s\n", ostd->c_first);
    fprintf(stderr, "\tc_middle:     %s\n", ostd->c_middle);
    fprintf(stderr, "\tc_last:       %s\n", ostd->c_last);
    fprintf(stderr, "\to_entry_d:    %s\n", ostd->o_entry_d);
    
    for(i = 0; i<ostd->item_cnt; i++)
    {
	fprintf(stderr, "\t   ol_s_w_d:      %d\n", 
		ostd->order_data[i].ol_supply_w_id);
	fprintf(stderr, "\t   ol_i_d:        %d\n", 
		ostd->order_data[i].ol_i_id);
	fprintf(stderr, "\t   ol_quantity:   %d\n", 
		ostd->order_data[i].ol_quantity);
	fprintf(stderr, "\t   ol_amount:     %f\n", 
		ostd->order_data[i].ol_amount);
	fprintf(stderr, "\t   ol_delivery_d: %s\n", 
		ostd->order_data[i].ol_delivery_d);
    }
}


/* Generates the ORDERSTATUS transaction data and executes the
 * transaction
 */
double
test_os()
{
    ORDERSTATUS_TRANSACTION_DATA ostd;
    int ret;
    struct timeval before, after;

    memset(&ostd, 0, sizeof(ORDERSTATUS_TRANSACTION_DATA));
    
    generate_orderstatus(&ostd);

#if PRINTALOT
    print_orderstatus_data(&ostd);
#endif
    
    if(gettimeofday(&before, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    ret = orderstatus_transaction(db_envp, 
				  dbp_customer, 
				  dbp_customer_sec,
				  dbp_order, 
				  dbp_order_sec,
				  dbp_orderline, &ostd);

    if(ret)
	printf("orderstatus transaction failed!\n");

    if(gettimeofday(&after, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    

#if PRINTALOT
    print_orderstatus_data(&ostd);
#endif

#if PRINT_TX_STATUS
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");

    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
#endif
    return get_delta_ms(&before, &after);
}


void *
test_os_thread(void *params)
{
    loop_same_txn(params);
}

/////////////////////////////////////////////////////////////////////////

void
generate_neworder(NEWORDER_TRANSACTION_DATA *notd)
{
    int rbk, i;

    if(W_ID != -1)
	notd->w_id = W_ID;
    else
	notd->w_id = random1(1, MAX_WID);

    if(D_ID != -1)
	notd->d_id = D_ID;
    else
	notd->d_id = random1(1, DISTRICTS_PER_WAREHOUSE);

    notd->c_id = NURand(1023, 1, 3000);
    notd->o_ol_cnt = random1(5, 15);
    
    rbk = random1(1, 100);

    for(i = 0; i<notd->o_ol_cnt; i++)
    {
	int x;

	notd->order_data[i].ol_i_id = NURand(8191, 1, 100000);
	
	/* Generate unused item */
	if(i == notd->o_ol_cnt-1 && rbk == 1)
	{
	    notd->order_data[i].ol_i_id = 100001;
	}
	
	x = random1(1, 100);
	if(x>1)
	    notd->order_data[i].ol_supply_w_id = notd->w_id;
	else
	{
	    /* Select warehouse other than home */
	    do
	    {
		x = random1(1, 5);
	    }while(x == notd->w_id);
	    
	    notd->order_data[i].ol_supply_w_id = x;
	}
	notd->order_data[i].ol_quantity = random1(1, 10);
    }
}

/////////////////////////////////////////////////////////////////////////

/* Prints the data returned after the neworder transaction */
void
print_neworder_data(NEWORDER_TRANSACTION_DATA *notd)
{
    int i;

    fprintf(stderr, "NEWORDER TRANSACTION DATA:\n");
    fprintf(stderr, "\tw_id:       %d\n", notd->w_id);
    fprintf(stderr, "\td_id:       %d\n", notd->d_id);
    fprintf(stderr, "\tc_id:       %d\n", notd->c_id);
    fprintf(stderr, "\to_id:       %d\n", notd->o_id);
    fprintf(stderr, "\to_ol_cnt:   %d\n", notd->o_ol_cnt);
    fprintf(stderr, "\tc_discount: %f\n", notd->c_discount);
    fprintf(stderr, "\tw_tax:      %f\n", notd->w_tax);
    fprintf(stderr, "\td_tax:      %f\n", notd->d_tax);
    fprintf(stderr, "\to_entry_d:  %s\n", notd->o_entry_d);
    fprintf(stderr, "\tc_credit:   %s\n", notd->c_credit);
    fprintf(stderr, "\tc_last:     %s\n", notd->c_last);
    
    for(i = 0; i<notd->o_ol_cnt; i++)
    {
	fprintf(stderr, "\t  ol_s_w_id:     %d\n", 
		notd->order_data[i].ol_supply_w_id);
	fprintf(stderr, "\t  ol_i_id:      %d\n", 
		notd->order_data[i].ol_i_id);
	fprintf(stderr, "\t  i_name:       %s\n", 
		notd->order_data[i].i_name);
	fprintf(stderr, "\t  ol_quantity:  %d\n", 
		notd->order_data[i].ol_quantity);
	fprintf(stderr, "\t  s_quantity:   %d\n", 
		notd->order_data[i].s_quantity);
	fprintf(stderr, "\t  brand:        %s\n", 
		notd->order_data[i].brand);
	fprintf(stderr, "\t  i_price:      %f\n", 
		notd->order_data[i].i_price);
	fprintf(stderr, "\t  amount:       %f\n", 
		notd->order_data[i].ol_amount);
    }
    
    fprintf(stderr, "\tstatus:     %s\n", notd->status);
    fprintf(stderr, "\ttotal:      %f\n", notd->total);

}

/* Test the new order transaction. */
double
test_no()
{
    static int failed = 0;
    int ret;
    NEWORDER_TRANSACTION_DATA notd;
    struct timeval before, after;
    
    memset(&notd, 0, sizeof(NEWORDER_TRANSACTION_DATA));
    
    /* Generate the neworder data */
    generate_neworder(&notd);
    
#if PRINTALOT
    print_neworder_data(&notd);    
#endif

    if(gettimeofday(&before, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    ret = neworder_transaction(db_envp, 
			       dbp_warehouse, 
			       dbp_district, 
			       dbp_customer,
			       dbp_neworder, 
			       dbp_stock, 
			       dbp_orderline,
			       dbp_order, 
			       dbp_item, &notd);

    if(ret)
    {
	failed++;
#if PRINTALOT
	fprintf(stderr, "neworder transaction failed! (total = %d)\n", failed);
#endif
    }


    if(gettimeofday(&after, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    

#if PRINTALOT
    print_neworder_data(&notd);
#endif

#if PRINT_TX_STATUS
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
#endif
    return get_delta_ms(&before, &after);
}

void *
test_no_thread(void *params)
{ 
    loop_same_txn(params);
}

/////////////////////////////////////////////////////////////////////////

/* Generates the new order, then checks it's status.
 * Then performs delivery on that order and checks the
 * status again.
 */
void
test_no_os()
{
    int ret;
    
    NEWORDER_TRANSACTION_DATA notd;
    ORDERSTATUS_TRANSACTION_DATA ostd;

    memset(&notd, 0, sizeof(NEWORDER_TRANSACTION_DATA));
    memset(&ostd, 0, sizeof(ORDERSTATUS_TRANSACTION_DATA));
    
    /* Generate the neworder data */
    generate_neworder(&notd);
    
    print_neworder_data(&notd);    

    ret = neworder_transaction(db_envp, 
			       dbp_warehouse, 
			       dbp_district, 
			       dbp_customer,
			       dbp_neworder, 
			       dbp_stock, 
			       dbp_orderline,
			       dbp_order, 
			       dbp_item, &notd);
    print_neworder_data(&notd);
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
    
    if(ret)
	return;

    /* Now check the order status of the order that has
     * just been placed */
    ostd.w_id = notd.w_id;
    ostd.d_id = notd.d_id;
    ostd.c_id = notd.c_id;

    ret = orderstatus_transaction(db_envp, 
				  dbp_customer, 
				  dbp_customer_sec,
				  dbp_order, 
				  dbp_order_sec,
				  dbp_orderline, &ostd);
    print_orderstatus_data(&ostd);
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");    

}

void *
test_no_os_thread(void *arg)
{
    int i;
    int times = (long long)arg;


    for(i = 0; i<times; i++)
    {
	test_no_os();
    }
    fprintf(stderr, "test_no_os_thread is DONE\n", times);

}

/////////////////////////////////////////////////////////////////////////
void
generate_stocklevel(STOCKLEVEL_TRANSACTION_DATA *sltd)
{
    if(W_ID != -1)
	sltd->w_id = W_ID;
    else
	sltd->w_id = random1(1, MAX_WID);

    if(D_ID != -1)
	sltd->d_id = D_ID;
    else
	sltd->d_id = random1(1, DISTRICTS_PER_WAREHOUSE);

    sltd->threshold = random1(10, 20);
}

/* Prints the data returned from the stocklevel transaction */
void
print_stocklevel_data(STOCKLEVEL_TRANSACTION_DATA *sltd)
{
    fprintf(stderr, "STOCKLEVEL DATA:\n");
    fprintf(stderr, "\tw_id:      %d\n", sltd->w_id);
    fprintf(stderr, "\td_id:      %d\n", sltd->d_id);
    fprintf(stderr, "\tthreshold: %d\n", sltd->threshold);
    fprintf(stderr, "\tlow_stock: %d\n", sltd->low_stock);

}


/* Tests the stock level transaction */
double
test_sl()
{
    static int failed = 0;
    int ret;
    struct timeval before, after;
    STOCKLEVEL_TRANSACTION_DATA sltd;

    memset(&sltd, 0, sizeof(STOCKLEVEL_TRANSACTION_DATA));
    
    generate_stocklevel(&sltd);

    if(gettimeofday(&before, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    ret = stocklevel_transaction(db_envp, 
				 dbp_district, 
				 dbp_orderline, 
				 dbp_stock, &sltd);
    if(ret)
    {
	failed++;	
#if PRINTALOT
	printf("stocklevel transaction failed! (total = %d)\n", failed);
#endif
    }

    if(gettimeofday(&after, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    
    
#if PRINTALOT
    print_stocklevel_data(&sltd);
#endif

#if PRINT_TX_STATUS
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
#endif
    return get_delta_ms(&before, &after);
}

void *
test_sl_thread(void *params)
{
    loop_same_txn(params);
}

/////////////////////////////////////////////////////////////////////////
void
generate_payment(PAYMENT_TRANSACTION_DATA *ptd)
{
    int x;

    if(W_ID != -1)
	ptd->w_id = W_ID;
    else
	ptd->w_id = random1(1, MAX_WID);

    if(D_ID != -1)
	ptd->d_id = D_ID;
    else
	ptd->d_id = random1(1, DISTRICTS_PER_WAREHOUSE);
    
    x = random1(1, 100);
    
    ptd->c_d_id = ptd->d_id;
    ptd->c_w_id = ptd->w_id;

    if(x <= 85)
    {
	ptd->c_d_id = ptd->d_id;
	ptd->c_w_id = ptd->w_id;
    }
    else
    {
	ptd->c_d_id = random1 (1, 10);
	do
	{
	    ptd->c_w_id = random1(1, 5);
	}while(ptd->w_id == ptd->c_w_id);
    }

    
    x = random1(1, 100);
    if(x <= 60)
    	Lastname(NURand(255,0,999), ptd->c_last);
    else
    	ptd->c_id = NURand(1023, 1, 3000);

    ptd->h_amount = random1 (100, 50000);
    
}

/* Print the data returned from the PAYMENT transaction */
void
print_payment_data(PAYMENT_TRANSACTION_DATA *ptd)
{
    fprintf(stderr, "PAYMENT DATA:\n");
    fprintf(stderr, "\tw_id:      %d\n", ptd->w_id);    
    fprintf(stderr, "\tc_id:      %d\n", ptd->d_id);    
    fprintf(stderr, "\tc_id:      %d\n", ptd->c_id);    
    fprintf(stderr, "\tc_w_id:    %d\n", ptd->c_w_id);    
    fprintf(stderr, "\tc_d_id:    %d\n", ptd->c_d_id);    
    
    fprintf(stderr, "\th_amount:  %f\n", ptd->h_amount);    
    fprintf(stderr, "\tc_cr_lim:  %f\n", ptd->c_credit_lim);    
    fprintf(stderr, "\tc_balance: %f\n", ptd->c_balance);    
    fprintf(stderr, "\tc_discount:%f\n", ptd->c_discount);    

    fprintf(stderr, "\th_date    :%s\n", ptd->h_date);    
    fprintf(stderr, "\tw_street_1:%s\n", ptd->w_street_1);    
    fprintf(stderr, "\tw_street_2:%s\n", ptd->w_street_2);    
    fprintf(stderr, "\tw_city    :%s\n", ptd->w_city);    
    fprintf(stderr, "\tw_state   :%s\n", ptd->w_state);    
    fprintf(stderr, "\tw_zip     :%s\n", ptd->w_zip);    

    fprintf(stderr, "\td_street_1:%s\n", ptd->d_street_1);    
    fprintf(stderr, "\td_street_2:%s\n", ptd->d_street_2);    
    fprintf(stderr, "\td_city    :%s\n", ptd->d_city);    
    fprintf(stderr, "\td_state   :%s\n", ptd->d_state);    
    fprintf(stderr, "\td_zip     :%s\n", ptd->d_zip);    

    fprintf(stderr, "\tc_first   :%s\n", ptd->c_first);    
    fprintf(stderr, "\tc_middle  :%s\n", ptd->c_middle);    
    fprintf(stderr, "\tc_last    :%s\n", ptd->c_last);    
    fprintf(stderr, "\tc_street_1:%s\n", ptd->c_street_1);    
    fprintf(stderr, "\tc_street_2:%s\n", ptd->c_street_2);    
    fprintf(stderr, "\tc_city    :%s\n", ptd->c_city);    
    fprintf(stderr, "\tc_state   :%s\n", ptd->c_state);    
    fprintf(stderr, "\tc_zip     :%s\n", ptd->c_zip);    
    
    fprintf(stderr, "\tc_phone   :%s\n", ptd->c_phone);    
    fprintf(stderr, "\tc_since   :%s\n", ptd->c_since);    
    fprintf(stderr, "\tc_credit  :%s\n", ptd->c_credit);    
    
    fprintf(stderr, "\tc_data_1  :%s\n", ptd->c_data_1);    
    fprintf(stderr, "\tc_data_2  :%s\n", ptd->c_data_2);    
    fprintf(stderr, "\tc_data_3  :%s\n", ptd->c_data_3);    
    fprintf(stderr, "\tc_data_4  :%s\n", ptd->c_data_4);    

}


/* Tests the payment transaction */
double
test_p()
{
    static int failed = 0;
    PAYMENT_TRANSACTION_DATA ptd;
    int ret;
    struct timeval before, after;

    memset(&ptd, 0, sizeof(PAYMENT_TRANSACTION_DATA));
    
    generate_payment(&ptd);

#if PRINTALOT
    print_payment_data(&ptd);
#endif 
    
    if(gettimeofday(&before, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    ret = payment_transaction(db_envp, 
			      dbp_warehouse, 
			      dbp_district,
			      dbp_customer, 
			      dbp_customer_sec, 
			      dbp_history, &ptd);
    
    if(ret)
    {
	failed++;
#if PRINTALOT
	printf("payment transaction failed! (total = %d)\n", failed);
	print_payment_data(&ptd);
#endif
    }
    if(gettimeofday(&after, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
   

#if PRINTALOT
    print_payment_data(&ptd);
#endif

#if PRINT_TX_STATUS
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
#endif
    return get_delta_ms(&before, &after);
}

void *
test_p_thread(void *params)
{
    loop_same_txn(params);
}

/////////////////////////////////////////////////////////////////////////
void
generate_delivery(DELIVERY_TRANSACTION_DATA *dtd)
{
    static int rand_reported = 0;
#if DELIVERY_NONRANDOM
    /* XXX - Hack. For debugging inconsistent performance */
    dtd->w_id = 3;
    dtd->o_carrier_id = 3;
#else
    if(W_ID != -1)
	dtd->w_id = W_ID;
    else
	dtd->w_id = random1(1, MAX_WID);

    dtd->o_carrier_id = random1(1, 5);
    if(!rand_reported)
    {
	printf("Delivery using RANDOM transaction inputs\n");
	rand_reported = 1;
    }
#endif

}

/* Print the data returned from DELIVERY transaction */
void
print_delivery_data( DELIVERY_TRANSACTION_DATA *dtd)
{
    fprintf(stderr, "DELIVERY DATA:\n");
    fprintf(stderr, "\tw_id:           %d\n", dtd->w_id);     
    fprintf(stderr, "\to_carrier_id:   %d\n", dtd->o_carrier_id);     
    fprintf(stderr, "\tstatus:         %s\n", dtd->status);     
    
}

/* Delivers and order and checks its status */
double
test_d()
{
    DELIVERY_TRANSACTION_DATA dtd;
    int ret;
    struct timeval before, after;

    memset(&dtd, 0, sizeof(DELIVERY_TRANSACTION_DATA));

    generate_delivery(&dtd);

#if PRINTALOT
    print_delivery_data(&dtd);
#endif
    
    if(gettimeofday(&before, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }

    ret = delivery_transaction(db_envp, 
			       dbp_neworder, 
			       dbp_order,
			       dbp_orderline,
			       dbp_customer, &dtd);

    if(ret)
    {
#if PRINTALOT
	fprintf(stderr, "delivery transaction failed: %s\n", 
		strerror(ret));
#endif
    }

    if(gettimeofday(&after, 0))
    {
	fprintf(stderr, "gettimeofday() failed: %s\n", strerror(errno));
    }
    
    

#if PRINTALOT
    print_delivery_data(&dtd);
#endif

#if PRINT_TX_STATUS
    if(ret)
	fprintf(stderr, "TRANSACTION FAILED\n");
    else
	fprintf(stderr, "TRANSACTION SUCCEEDED\n");
#endif
    return get_delta_ms(&before, &after);
}

void *
test_d_thread(void *params)
{
    loop_same_txn(params);
}

/////////////////////////////////////////////////////////////////////////

/* Sustain the target transaction rate. Sleep if necessary. 
 * If the system is not keeping up, signal error and exit.
 * tx_total is the number of transactions that have been executed so far
 */
int
sustain_rate()
{
    static int time_measured, tx_total;
    static struct timeval tv_start;
    struct timeval tv_current;
    double seconds_elapsed;
    double tx_rate;

    /* Record the time when this function is first called */
    if(!time_measured)
    {
	if(gettimeofday(&tv_start, 0))
	    return -1;
	time_measured = 1;
    }

    /* This is called after every transaction, so we
     * know how many transactions have been executed so far.
     */
    tx_total++;

    while(1)
    {
	/* Check whether we are within the target transaction rate. 
	 */
	if(gettimeofday(&tv_current, 0))
	    return -1;

	/* If we are over the target rate, go to sleep */
	seconds_elapsed = get_delta_ms(&tv_start, &tv_current)/1000;
	tx_rate = ((double)tx_total)/seconds_elapsed;
	
	if(tx_rate / tx_target_rate > 1)
	{
	    int ret;

	    /* Sleep takes at least 20 ms on FreeBSD */
	    ret = poll(0, 0, 1);
	    printf(".");
	    if(ret)
	    {
		perror("poll\n");
	    }
	}
	else
	{
	    return 0;
	}
    }
}

/////////////////////////////////////////////////////////////////////////
/*
 * Execute a mix of transactions. Times argument is the total number of 
 * transactions that need to be done. 
 */
#define P_MIX  45
#define NO_MIX 43
#define OS_MIX 4
#define D_MIX  4
#define SL_MIX 4

#define WARMUP   100
#define COOLDOWN 100

typedef struct
{
    double tx_done;
    double average; 
}tx_stat_t;

void *
test_mix(void *params)
{
    thread_data_t *tt = (thread_data_t *)params;
    int x, i, pid, tx_total;
    tx_stat_t *ts;    
    struct timeval tv;
    long start_sec, end_sec;
    int collect = 0;
    int times = tt->iterations;

    /*maybe_bind_me(tt->cpu_id);*/

    /* Initialize the structure where we keep statistics */
    ts = (tx_stat_t*)malloc(sizeof(tx_stat_t) * (STOCKLEVEL + 1));
    if(ts == NULL)
    {
	perror("malloc");
	return (void *)-1;
    }
    memset(ts, 0, sizeof(tx_stat_t) * (STOCKLEVEL + 1));

#if defined (__sparc) 
    /* Tell Simics when a detailed simulation should begin */
    MAGIC_BREAKPOINT;
    fprintf(stderr, "Will wait for others to arrive\n");
    while(!others_arrived())
	;
#endif

    for(i = 0; i<times; i++)
    {
	double time;
	/* Pick a transaction */
	x = random1(1, 100);
    
	/* Start collecting statistics after a 1000 transactions */
	if(i == WARMUP)
	{
	    collect = 1;
	    /* Record the start time */
	    if(gettimeofday(&tv, 0))
	    {
		perror("gettimeofday()");
		return (void *)-1;
	    }
	    start_sec = tv.tv_sec;
	}

	/* Stop collecting 1000 tx before we are done */
	if(i == (times-COOLDOWN))
	{
	    collect = 0;
	    /* Record the end time */
	    if(gettimeofday(&tv, 0))
	    {
		perror("gettimeofday()");
		return (void *)-1;
	    }
	    end_sec = tv.tv_sec;
	    tx_total = times - (WARMUP + COOLDOWN);
	}


	if(x <= P_MIX)
	{
	    tx_stat_t *tstat = &ts[PAYMENT];

	    time = test_p();
	    if(collect)
	    {
		tstat->tx_done++;
		tstat->average = (tstat->average * 
			     (tstat->tx_done-1) + time)
		    /tstat->tx_done;
	    }
	    
	}
	else if(x <= P_MIX + NO_MIX )
	{
	    tx_stat_t *tstat = &ts[NEWORDER];

	    time = test_no();
	    if(collect)
	    {
		tstat->tx_done++;
		tstat->average = (tstat->average * 
			     (tstat->tx_done-1) + time)
		    /tstat->tx_done;
	    }

	}
	else if(x <= P_MIX + NO_MIX + OS_MIX)
	{
	    tx_stat_t *tstat = &ts[ORDERSTATUS];

	    time = test_os();
	    if(collect)
	    {
		tstat->tx_done++;
		tstat->average = (tstat->average * 
			     (tstat->tx_done-1) + time)
		    /tstat->tx_done;
	    }

	}
	else if(x <= P_MIX + NO_MIX + OS_MIX + D_MIX)
	{
	    tx_stat_t *tstat = &ts[DELIVERY];

	    time = test_d();
	    if(collect)
	    {
		tstat->tx_done++;
		tstat->average = (tstat->average * 
			     (tstat->tx_done-1) + time)
		    /tstat->tx_done;
	    }

	}
	else
	{
	    tx_stat_t *tstat = &ts[STOCKLEVEL];
	    
	    time = test_sl();
	    if(collect)
	    {
		tstat->tx_done++;
		tstat->average = (tstat->average * 
			     (tstat->tx_done-1) + time)
		    /tstat->tx_done;
	    }
	    
	}
	
	/* We have to generate transactions at a specific rate */
	if( rate_limit )
	{
	    if(sustain_rate())
	    {
		perror("sustain_rate()");
		return (void *)-1;
	    }
	}
    }
    
#if defined (__sparc) 
    /* Tell Simics when a detailed simulation should end */
    MAGIC_BREAKPOINT;
#endif

    pid = getpid();

    /*report_my_lwpusage("mixed");*/

    /* Report statistics */
    fprintf(stdout, "[%d]:TRANSACTIONS COMPLETED: %d\n", pid, tx_total);  
    fprintf(stdout, "[%d]:THROUGHPUT:             %f\n", pid, 
	    ((float)tx_total)/(end_sec-start_sec));  
    for(i = 1; i<=STOCKLEVEL; i++)
    {
	fprintf(stdout, "[%d]:%15s: %5.2f ms per tx of %.0f tx.\n", pid, 
		tx_names[i], ts[i].average, ts[i].tx_done);
    }  
    free(ts);
		
}

/////////////////////////////////////////////////////////////////////////



void
test_transactions(txn_type_t test_type, int iterations)
{
    struct timespec begin, end;
    int i, ret, to_print;
    void *(*start_routine)(void*) = NULL;
    DB_MPOOL_STAT *mpool_stat;
    DB_MUTEX_STAT *mutex_stat;
    DB_LOCK_STAT  *lock_stat;

    db_envp->memp_stat(db_envp, &mpool_stat, 0, DB_STAT_CLEAR);
    db_envp->mutex_stat(db_envp, &mutex_stat, DB_STAT_CLEAR);
    db_envp->lock_stat(db_envp, &lock_stat, DB_STAT_CLEAR);
    
    // printf("DB_NOLOCKING IS SET!!!!\n");
    // db_envp->set_flags(db_envp, DB_NOLOCKING, 1);
#if DELIVERY_NONRANDOM
    printf("Delivery transactions are NOT using random inputs\n");
#endif

    if(no_locks)
	printf("Running without read/write locks\n");
    if(no_rand)
	printf("Not using randomly generated inputs\n");
    
    for(i = 0; i < num_bench_threads; i++)
    {
	int ret;
	pthread_attr_t attr;
        pthread_attr_init(&attr);

        pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM); 
	

	switch(bench_threads[i].txn_type)
	{
	case MIXED: /* do a mix of transactions */
	    start_routine = test_mix;
	    break;
	case PAYMENT:
	    start_routine = test_p_thread;
	    break;
	case NEWORDER:
	    start_routine = test_no_thread;
	    break;
	case ORDERSTATUS:
	    start_routine = test_os_thread;
	    break;
	case DELIVERY:
	    start_routine = test_d_thread;
	    break;
	case STOCKLEVEL:
	    start_routine = test_sl_thread;
	    break;
	default:
	    break;
	}
	if(start_routine == NULL)
	    return;
	
/** +EDIT */
#ifndef BENCHMARK
	ret = pthread_create(&(bench_threads[i].tid), &attr, 
			     start_routine, &(bench_threads[i]));
#else
	ret = liblock_thread_create(&(bench_threads[i].tid), &attr, 
			     start_routine, &(bench_threads[i]));
#endif
/** -EDIT */
	if(ret)
	    perror("pthread_create");
    }

    clock_gettime(CLOCK_REALTIME, &begin);

    for(i = 0; i < num_bench_threads; i++)
    {
	ret = pthread_join(bench_threads[i].tid, (void**)0);
        if (ret)
        {
            perror("pthread_join");
            return;
        }
    }


    clock_gettime(CLOCK_REALTIME, &end);

    printf("All done! The test took %lld ms\n", ((end.tv_sec-begin.tv_sec)*1000) + ((end.tv_nsec-begin.tv_nsec)/1000000));
/** +EDIT */
/*
    // +EDIT (added newline)
    printf("Enter 1 to print db stat, -1 to exit: \n");
    // -EDIT
    scanf("%d", &to_print); 
    if(to_print == 1)
    {
	db_envp->memp_stat(db_envp, &mpool_stat, 0, 0);
	print_memp_stat(mpool_stat);

	printf("-----------------------------------\n");
	printf("\nMUTEX STAT:\n");
	printf("-----------------------------------\n");
	db_envp->mutex_stat_print(db_envp, 0);
	
	printf("-----------------------------------\n");
	printf("\nLOCK STAT:\n");
	printf("-----------------------------------\n");
	db_envp->lock_stat_print(db_envp, 0);
    }
*/
    exit(0);
/** -EDIT */
}

/////////////////////////////////////////////////////////////////////////
void
usage(char *argv0)
{
    fprintf(stderr, "usage: %s -h <home_dir> -t -d\n", argv0);
    fprintf(stderr, "Required parameters: -h [database home directory]\n");
    fprintf(stderr, "Other parameters:    -b run the benchmark\n");
    fprintf(stderr, "                     -d generate debug output\n");
    fprintf(stderr, "                     -i prompt to specify test "
	    "transaction type \n");
    fprintf(stderr, "                     -n [number of txn to perform]\n");
    fprintf(stderr, "                     -t [transaction type for the "
	    "benchmark] create new thread \n");
    fprintf(stderr, "	                      (0-MIXED, 1-PAYMENT,\n");
    fprintf(stderr, "                         2-NEW ORDER, 3-ORDER STATUS,\n");
    fprintf(stderr, "                         4-DELIVERY, ""5-STOCK LEVEL)\n");
    fprintf(stderr, "                     -r  Run db recovery and exit\n");

    fprintf(stderr, "                     -B [CPU id] bind to a CPU \n");
    fprintf(stderr, "                     -C do not use random txn inputs\n");
    fprintf(stderr, "                     -L run without locks - "
	    "ok if read only transactions\n");
    fprintf(stderr, "                     -W [max warehouse number]\n");
    fprintf(stderr, "                     -P Run with DB_PRIVATE \n");
    fprintf(stderr, "                     -R [rate] Attain this txn rate\n");
    fprintf(stderr, "                     -S Scan all files into the db "
	    "cache before running the benchmarks\n");
    fprintf(stderr, "                     -T Start the trickle thread and "
	    "exit\n");

}

#define DEFAULT_ITER 10000

/////////////////////////////////////////////////////////////////////////
int 
main(int argc, char **argv)
{

    extern char *optarg;
    extern int optind, optopt;
    do_autocommit = 1;
    char *home_dir = 0;
    char c;
    int  i, test = 0, recover = 0, tx_rate, iterations = DEFAULT_ITER, 
	interactive = 0, cpu_id = -1;
    txn_type_t txn_type = MIXED;
    int cur_bench_thread = 0;

    printf("%s: pid %d\n", argv[0], getpid());

    while ((c = getopt(argc, argv, "bdh:in:t:B:CLW:D:R:PST")) != -1) 
    {
        switch(c)
        {
	case 'b': /* Run the benchmark test */
	    test = 1;
	    break;
	case 'd': /* Debug option */
	    option_debug=1;
	    break;    
	case 'h': /* Get the home directory for db */
	    home_dir = optarg;
	    fprintf(stdout, "Home directory is %s\n", home_dir);
	    break;
	case 'i':
	    interactive = 1;
	    test = 1; /* interactive implies that we want to test */
	    break;
	case 'n':
	    iterations = atoi(optarg);
	    bench_threads[cur_bench_thread].iterations = iterations;
	    break;
	case 'r':
	    recover = 1;
	    break;
	case 't':
	    if(num_bench_threads == MAX_THREADS)
	    {
		printf("Exceeded the maximum allowed threads (%d).\n",
		       MAX_THREADS);
		return -1;
	    }
	    cur_bench_thread = num_bench_threads;
	    num_bench_threads++;
	    txn_type = atoi(optarg);
	    bench_threads[cur_bench_thread].txn_type = txn_type;
	    break;
	case 'B':
	    cpu_id = atoi(optarg);
	    /*bench_threads[cur_bench_thread].cpu_id = cpu_id;*/
	    break;
	case 'C':
	    no_rand = 1;
	    break;
	case 'L':
	    no_locks = 1;
	    break;
	case 'W':
	    MAX_WID = atoi(optarg);
	    break;
	case 'D':
	    D_ID = atoi(optarg);
	    break;
	case 'P':
	    private = 1;
	    break;
	case 'R':
	    tx_target_rate = atoi(optarg);
	    printf("Target rate is %d\n", tx_target_rate);
	    rate_limit = 1;
	    if(tx_target_rate <= 0)
		rate_limit = 0;
	    printf("Target rate is %d\n", tx_target_rate);
	    break;
	case 'S':
	    do_scan = 1;
	    break;
	case 'T':
	    /* Just start the trickle thread */
	    do_trickle = 1;
	    break;
	default : 
	    usage(argv[0]);
	    exit(-1);
	}
    }
    
    if(home_dir == NULL)
    {
	usage(argv[0]);
	exit(1);
    }
    
    /*maybe_bind_me(cpu_id);*/

    if(prepare_for_xactions(home_dir, recover))
    {
	fprintf(stderr, "Failed to initialize the indices. Exiting\n");
	goto done;
    }
    
    if(recover)
	goto done;
    
    if(interactive)
    {
	int txn_val, cpu_id, i, choice_val, this_iter, report_perf;
	do
	{
	    if(num_bench_threads == MAX_THREADS)
	    {
		printf("Exceeded the maximum allowed threads (%d).\n",
		       MAX_THREADS);
		break;
	    }
		    
	    printf("Please enter the type of test transaction:\n"
		   "\t(0-MIXED, 1-PAYMENT,\n"
		   "\t 2-NEW ORDER, 3-ORDER STATUS,\n"
		   "\t 4-DELIVERY, ""5-STOCK LEVEL)\n"
		   "If you are done, enter -1\n"
		   "Enter your choice: ");
	    scanf("%d", &choice_val);
	    if(choice_val == -1)
		break;
	    txn_type = choice_val;
	    
	    printf("Enter the bound CPU id (-1 if none): ");
	    scanf("%d", &choice_val);
	    if(choice_val == -1)
		cpu_id = -1;
	    else
		cpu_id = choice_val;

	    if(txn_type < MIXED || txn_type > STOCKLEVEL)
	    {
		txn_type = MIXED;
		fprintf(stderr, "warning: transaction type set to MIXED\n");
	    }

	    printf("Enter the number of iterations (-1 to use global or "
		   "default): ");
	    scanf("%d", &choice_val);
	    if(choice_val == -1)
		this_iter = iterations;
	    else
		this_iter = choice_val;

	    printf("Enter 1 to report performance to simulator, 0 "
		   "otherwise: ");
	    scanf("%d", &choice_val);
	    if(choice_val == 1)
		report_perf = 1;
	    else
		report_perf = 0;

	    bench_threads[num_bench_threads].txn_type = txn_type;
	    /*bench_threads[num_bench_threads].cpu_id = cpu_id;*/
	    bench_threads[num_bench_threads].iterations = this_iter;
	    bench_threads[num_bench_threads].report_perf_sim = report_perf;
	    num_bench_threads++;
	} while(1);
	
	printf("Will run these benchmark threads:\n",
	       iterations);
	for(i = 0; i <  num_bench_threads; i++)
	{
	    printf("Thread %d: txn_type %s, %d iterations\n",
		   i, get_txn_type_string(bench_threads[i].txn_type), 
		   bench_threads[i].iterations);
	}

	printf("Using MAX_WID of %d\n", MAX_WID);
    }

   
    if(test)
	test_transactions(txn_type, iterations);

    
 done:
    cleanup();
    
}

/////////////////////////////////////////////////////////////////////////
/*
 * Functions for interaction with the simulator and for
 * binding to CPU
 */
/*
static void
maybe_bind_me(processorid_t cpu_id)
{
 
    if(cpu_id >= 0)
    {
	if(processor_bind(P_LWPID, P_MYID, cpu_id, NULL) != 0)
	{
	    perror("processor_bind");
	}
	else
	{
	    printf("Bound to processor %d\n", cpu_id);
	}
    }
}
*/

/*
 * A thread synchronization function using CAS. We use it
 * in addition to the simulator-drive synchronization function.
 */

static void
wait_for_threads_to_arrive(void)
{
    static volatile int threads_alive;
    int cas_success, read_threads_alive;
    int id = pthread_self();

#if defined(__sparc)
    do {
        read_threads_alive = threads_alive;
        cas_success =
            CAS(&threads_alive, read_threads_alive, 
                read_threads_alive + 1);
    } while (!cas_success);
#endif
    printf("Thread %d: is alive...\n", id);

    /* Wait for all threads to show up.
     */
    if (threads_alive < num_bench_threads) 
    {
        //printf("thread %d: waiting...\n", id);
        while (threads_alive < num_bench_threads) ;
    }
    assert(threads_alive == num_bench_threads);
    //    printf("thread %d: %d threads are now alive.\n", id, threads_alive);
}
/*
static void
bind_break_and_wait(processorid_t cpu_id)
{
    maybe_bind_me(cpu_id);

#if defined(__sparc)
    MAGIC_BREAKPOINT;
    fprintf(stderr, "Will wait for others to arrive\n");
    
    wait_for_threads_to_arrive();
    while(!others_arrived())
	;
#endif
}
*/

/////////////////////////////////////////////////////////////////////////
/*
 * Read and report cpu usage statistics for this lwp using 
 * the /proc file system
 */
/*
#include <sys/lwp.h>
#include <sys/procfs.h>
#include <fcntl.h>

extern struct timespec total_ns_in_mutex_get[32];

static void
report_my_lwpusage(char *thread_name)
{
#define BUFSIZE 100
    char proc_name[BUFSIZE];
    prusage_t prusage;
    int fd, ret;

    snprintf(proc_name, BUFSIZE, "/proc/%d/lwp/%d/lwpusage", 
	     getpid(), _lwp_self());
    
    fd = open(proc_name, O_RDONLY);
    if(fd == -1)
    {
	perror("open");
	return;
    }

    do
    {
	ret = read(fd, &prusage, sizeof(prusage));
    }
    while (ret < 0 && errno == EINTR);
    close(fd);

    printf("\n=======================================\n");
    printf("CPU usage for LWP %d (%s):\n", prusage.pr_lwpid, 
	   thread_name);
    printf("-----------------------------------------\n");
    printf("pr_rtime   (total lwp real (elapsed) time)\t %ld s, %ld ns\n", 
	   prusage.pr_rtime.tv_sec, prusage.pr_rtime.tv_nsec);
    printf("pr_utime   (user level CPU time)          \t %ld s, %ld ns\n", 
	   prusage.pr_utime.tv_sec, prusage.pr_utime.tv_nsec);
    printf("pr_stime   (system call CPU time)         \t %ld s, %ld ns\n", 
	   prusage.pr_stime.tv_sec, prusage.pr_stime.tv_nsec);
    printf("pr_ttime   (other system trap CPU time)   \t %ld s, %ld ns\n", 
	   prusage.pr_ttime.tv_sec, prusage.pr_ttime.tv_nsec);
    printf("pr_tftime  (text page fault sleep time)   \t %ld s, %ld ns\n", 
	   prusage.pr_tftime.tv_sec, prusage.pr_tftime.tv_nsec);
    printf("pr_dftime  (data page fault sleep time)   \t %ld s, %ld ns\n", 
	   prusage.pr_dftime.tv_sec, prusage.pr_dftime.tv_nsec);
    printf("pr_kftime  (kernel page fault sleep time) \t %ld s, %ld ns\n", 
	   prusage.pr_kftime.tv_sec, prusage.pr_kftime.tv_nsec);
    printf("pr_ltime   (user lock wait sleep time)    \t %ld s, %ld ns\n", 
	   prusage.pr_ltime.tv_sec, prusage.pr_ltime.tv_nsec);
    printf("pr_slptime (all other sleep time)         \t %ld s, %ld ns\n", 
	   prusage.pr_slptime.tv_sec, prusage.pr_slptime.tv_nsec);
    printf("pr_wtime   (wait-cpu (latency) time)      \t %ld s, %ld ns\n", 
	   prusage.pr_wtime.tv_sec, prusage.pr_wtime.tv_nsec);

    printf("Total time spent in mutex_get routine: %" PRId64 "ms\n", 
	   total_ns_in_mutex_get[pthread_self()]/NS_IN_MS);
}
*/
/////////////////////////////////////////////////////////////////////////

/* 
 * This function loops transactions of the same kind
 * and reports performance at the end.
 */

static void
loop_same_txn(void *param)
{
    thread_data_t *tt = (thread_data_t *)param;
    int i, times = tt->iterations;
    double resettable_cum_txn_lat_us = 0;
    double resettable_cum_txn = 0;
    struct timespec start_ns, end_ns;
    char *thread_name = tx_names[tt->txn_type];
    double (*txn_func)(void) = NULL;


    switch(tt->txn_type)
    {
    case PAYMENT:
	txn_func = test_p;
	break;
    case NEWORDER:
	txn_func = test_no;
	break;
    case ORDERSTATUS:
	txn_func = test_os;
	break;
    case DELIVERY:
	txn_func = test_d;
	break;
    case STOCKLEVEL:
	txn_func = test_sl;
	break;
    default:
	printf("Invalid txn type: %d\n", tt->txn_type);
	return;
    }

    /* This is to interact with the simulator */
    /*bind_break_and_wait(tt->cpu_id);*/

    /* Enter the scheduling class */
    /*
    if(priocntl(P_LWPID, P_MYID, PC_SETXPARMS, "CA", 0))
      fprintf(stderr, "Could not enter scheduling class\n");
    */
    printf("%s thread will run for %d iterations [%s] \n", 
	   thread_name, times, 
	   (tt->report_perf_sim==1)?"reporting performance":
	"NOT reporting performance");
    
    clock_gettime(CLOCK_REALTIME, &start_ns);

    for(i = 0; i<times; i++)
    {
//	printf("%ld thread runs %d\n", tt->tid, i);
	double time_ms = txn_func();
	resettable_cum_txn++;
	resettable_cum_txn_lat_us += (time_ms * US_IN_MS);

#if defined(__sparc)
	/* If this function returns 1 the simulator tells us
	 * to reset the statistics */
	if(tt->report_perf_sim)
	{
	    if(report_data_to_simulator((uint64_t) resettable_cum_txn))
	    {
		resettable_cum_txn_lat_us = 0;
		resettable_cum_txn = 0;
	    }
	}

	if(do_quit())
	{
	    printf("Thread is told to quit\n");
	    break;
	}
#endif

#if SYNC_MPOOL
	if(times % 100 == 0)
	{
	    int ret;
	    if(ret = db_envp->memp_trickle(db_envp, 10, 0))
	    {
		db_error("memp_trickle", ret);
	    }
	}
#endif
    }

#if defined(__sparc)
    /* Tell Simics when a detailed simulation should end */
    MAGIC_BREAKPOINT;
#endif
    clock_gettime(CLOCK_REALTIME, &end_ns);
    times = i;

    {
	double elapsed_ms = (double)(((end_ns.tv_sec - start_ns.tv_sec)*1000) + ((end_ns.tv_nsec - start_ns.tv_nsec)/NS_IN_MS));
	double avg_lat_ms = elapsed_ms / (double)times; 

	double throughput = (double)times/(elapsed_ms / MS_IN_SEC);

	/*report_my_lwpusage(thread_name);*/
    
	printf("%s thread completed %d iterations \n"
	       "\tthroughput: %.2f tps\n"
	       "\taverage txn latency: %f ms\n"
	       "\telapsed time: %.2f sec\n", thread_name,
	       times, throughput, avg_lat_ms, 
	       elapsed_ms/(double)MS_IN_SEC);
    }
}


