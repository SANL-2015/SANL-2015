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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <db.h>

/* Some global settings for Berkeley DB */
#define DB_CACHE_SIZE    600*1024*1024 
#define DB_COMMON_PAGE_SIZE     8*1024

/* 
 * Play with page sizes to see how this affects lock contention.
 */
#define DB_WH_PAGE_SIZE      DB_COMMON_PAGE_SIZE     
#define DB_DS_PAGE_SIZE      DB_COMMON_PAGE_SIZE    
#define DB_OL_PAGE_SIZE      DB_COMMON_PAGE_SIZE  

/* Names of databases that we will need */
#define ITEM_INDEX_NAME               "item.db"
#define WAREHOUSE_INDEX_NAME          "warehouse.db"
#define DISTRICT_INDEX_NAME           "district.db"
#define CUSTOMER_INDEX_NAME           "customer.db"
#define CUSTOMER_SECONDARY_NAME       "customer_sec.db"
#define ORDER_INDEX_NAME              "order.db"
#define ORDER_SECONDARY_NAME          "order_sec.db"
#define NEWORDER_INDEX_NAME           "neworder.db"
#define STOCK_INDEX_NAME              "stock.db"
#define DISRICT_INDEX_NAME            "district.db"
#define ORDERLINE_INDEX_NAME          "orderline.db"
#define HISTORY_INDEX_NAME            "history.db"

/////////////////////////////////////////////////////////////////////////
/* Are we using transactions? */
#define TRANSACTIONS 0

/////////////////////////////////////////////////////////////////////////

/* TPCC definitions */
#define DISTRICTS_PER_WAREHOUSE 10

/////////////////////////////////////////////////////////////////////////

/* Some helper db functions */
int          init_environment(DB_ENV **envp, char *home_dir, int flags);
int          create_db(DB_ENV *envp, DB **dbp, int psize, int flags);
int          open_db(DB *dbp, char *name, int flags);
void         db_error(char *func_name, int err);
void         error(char *func_name);
void         close_environment(DB_ENV *db_envp);

/* Useful db related functions */
void         print_memp_stat(DB_MPOOL_STAT *mp_stat);
void         print_mutex_stat(DB_MUTEX_STAT *mutex_stat);
int          scan_db(DB *dbp); /* Sequentially scan the db into memp */

/* Custom comparison functions */
int          district_comparison_func(DB *dbp, const DBT *a, const DBT *b);
int          orderline_comparison_func(DB *dbp, const DBT *a, const DBT *b);
int          neworder_comparison_func(DB *dbp, const DBT *a, const DBT *b);
int          order_comparison_func(DB *dbp, const DBT *a, const DBT *b);
int          customer_secondary_comparison_func(DB *dbp, const DBT *a, 
						const DBT *b);


/* Functions to calculate keys for secondary indices */
int          get_customer_sec_key(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);
int          get_order_sec_key(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey);

/* Random numbers */
int          no_rand;
int          NURand(int A, int x, int y);
int          random1(int x, int y);
void         seed(long val);

/* Statistics reporting */
void         print_stats(void);


