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

/*
 * Load TPCC tables. Ported to Berkeley DB from the sample program
 * included with TPCC spec.
 */
/* Ported to Linux x86_64 by Justin Funston */

#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <assert.h>

#include <db.h>
#include "tpcc_schema.h"
#include "tpcc_globals.h"






#define MAXITEMS      100000
#define CUST_PER_DIST 3000
#define DIST_PER_WARE 10
#define ORD_PER_DIST  3000

/* Functions */
void         InitPermutation();
int          GetPermutation();
void         LoadItems();
void         LoadWare();
void         LoadCust();
void         LoadOrd();
void         LoadNewOrd();
int          Stock();
int          District();
int          Customer();
int          Orders();
void         New_Orders();
void         MakeAddress();
void         Error();
void         Lastname();


/* Global Variables */
int         i;
int         option_debug = 0;  /* 1 if generating debug output */
char       *home_dir = NULL;   /* home directory of db */ 
long        count_ware;        /* number of warehouses */
char       *timestamp;         /* timestamp for date fields */

extern int do_autocommit;

   DB_ENV         *db_env;            /* Berkeley DB environment */
static DB         *dbp_stock;
static DB         *dbp_district;
static DB         *dbp_customer;
static DB         *dbp_history;
static DB         *dbp_order;
static DB         *dbp_neworder;
static DB         *dbp_orderline;

char *database_names[] = { 
    ITEM_INDEX_NAME,
    WAREHOUSE_INDEX_NAME,
    CUSTOMER_INDEX_NAME,
    CUSTOMER_SECONDARY_NAME,
    ORDER_INDEX_NAME,
    ORDER_SECONDARY_NAME,
    NEWORDER_INDEX_NAME,
    STOCK_INDEX_NAME,
    DISRICT_INDEX_NAME,
    ORDERLINE_INDEX_NAME,
    HISTORY_INDEX_NAME,
    NULL
};

static int databases_exist(void);
static int MakeAlphaString(int min, int max, char *str);
static int MakeNumberString(int min, int max, char *str);

/////////////////////////////////////////////////////////////////////////
/*
 * main()
 * ARGUMENTS:     Warehouses n [Debug] [Help]
 */
int
main(int argc, char **argv )
{
    char        arg[2];
    time_t t_clock;
    do_autocommit = 0;

    count_ware=0;

    for (i=1; i<argc; i++)
    {
       
	strncpy(arg, argv[i], 2);
	if(arg[0] != '-')
	{
	    i++;
	    continue;
	}

	switch (arg[1]) {
	case 'w': /* Warehouses */
	    if (count_ware)
	    {
		printf("Error - Warehouses specified more than once.\n");
		exit(-1);
	    }
	    if (argc-1>i)
	    {
		i++;
		count_ware=atoi(argv[i]);
		if (count_ware<=0)
		{
		    printf("Invalid Warehouse Count.\n");
		    exit(-1);
		}
	    }
	    else
	    {
		printf("Error - Warehouse count must follow");
		printf(" Warehouse keyword\n");
		exit(-1);
	    }
	    break;

/******* Generic Args *********************/
	case 'd': /* Debug Option */
	    if (option_debug)
	    {
		printf("Error - Debug option specified more than once\n");
		exit(-1);
	    }
	    option_debug=1;
	    break;
	    
	case 'h': /* Get the home directory for db */
	    if(argc - 1 > i)
	    {
		i++;
		home_dir = argv[i];
	    }
	    else
	    {
		printf("Error: home directory name must follow -H keyword\n");
		exit(-1);
	    }
	    break;

	default : printf("Error - Unknown Argument (%s)\n",arg);
	    printf("Usage: %s -w <number of warehouses> -h <home dir> ", argv[0]);
	    printf("-d [Debug]\n");
	    exit(-1);
	}
    }

    if (!(count_ware)) {
	printf("Not enough arguments.\n");
	printf("Usage: %s -w <number of warehouses> -h <home dir> ", argv[0]);
	printf("-d [Debug]\n");
	exit(-1);
    } 
    if(home_dir == NULL)
    {
	printf("Not enough arguments.\n");
	printf("Usage: %s -w <number of warehouses> -h <home dir> ", argv[0]);
	printf("-d [Debug]\n");
	exit(-1);
    }

    srandom( time( 0 ) );

    /* Create the db environment */
    if(init_environment(&db_env, home_dir, DB_PRIVATE /* non-default flags */))
    {
	exit(1);
    }

    /* Make sure that databases do not exist. */
    if(databases_exist())
    {
	printf("Please remove all old databases before");
	printf(" loading the new ones.\n");
	exit(1);
    }

    /* Initialize timestamp (for date columns) */
    if((int)time(&t_clock) == -1)
    {
	error("time");
	exit(1);
    }
    timestamp = ctime(&t_clock);
    assert(timestamp != NULL);

    printf( "TPCC Data Load Started...\n" );
    LoadItems(); 
    LoadWare();
    LoadCust();
    LoadOrd();


    printf( "\n...DATA LOADING COMPLETED SUCCESSFULLY.\n" );
    return(0);
}


/////////////////////////////////////////////////////////////////////////

/*
 * Loads the Item table
 */
void 
LoadItems(void)
{
    long    i_id;
    char    i_name[25];
    char    i_data[51];
    float   i_price;
    int     idatasiz, orig[MAXITEMS], i, err, pos;
    DB *db, *db_sec;


    /* Create/open a new Berkeley DB for the Item index */
    if(create_db(db_env, &db, DB_COMMON_PAGE_SIZE, 0) || open_db(db, ITEM_INDEX_NAME, 0))
    {
	exit(1);
    }

    printf("Loading Item \n");
    
    for (i=0; i<MAXITEMS; i++) 
    {
	orig[i]=0;
    }

    for (i=0; i<MAXITEMS/10; i++) 
    {
	do
	{
	    pos = random1(0L,MAXITEMS);
	} while (orig[pos]);
	orig[pos] = 1;
    }
    
    for (i_id=1; i_id<=MAXITEMS; i_id++) 
    {
	DBT key, data;
	ITEM_PRIMARY_KEY i_key;
	ITEM_PRIMARY_DATA i_primdata;

	memset(&i_key, 0, sizeof(ITEM_PRIMARY_KEY));
	memset(&i_primdata, 0, sizeof(ITEM_PRIMARY_DATA));
    
	/* Generate Item Data */
	MakeAlphaString( 14, 24, i_name);
	i_price=((float) random1(100L,10000L))/100.0;
	idatasiz=MakeAlphaString(26,50,i_data);
	
	if (orig[i_id-1])
	{
	    pos = random1(0,idatasiz-8);

	    i_data[pos]= 'o'; 
	    i_data[pos+1] = 'r'; 
	    i_data[pos+2] = 'i'; 
	    i_data[pos+3] = 'g'; 
	    i_data[pos+4] = 'i'; 
	    i_data[pos+5] = 'n'; 
	    i_data[pos+6] = 'a'; 
	    i_data[pos+7] = 'l'; 
	}
  
	if ( option_debug )
	{
	    printf( "IID = %ld, Name= %16s, Price = %5.2f\n",
		    i_id, i_name, i_price );
	}
	
	/* Initialize the key */
	memset(&key, 0, sizeof(key));
	i_key.I_ID = i_id;
	key.data = &i_key;
	key.size = sizeof(ITEM_PRIMARY_KEY);

	/* Initialize the data */
	memset(&data, 0, sizeof(data));
	i_primdata.I_IM_ID = 0;
	i_primdata.I_PRICE = i_price;
	strncpy(i_primdata.I_NAME, i_name, 25);
	strncpy(i_primdata.I_DATA, i_data, 51);
	data.data = &i_primdata;
	data.size = sizeof(ITEM_PRIMARY_DATA);

	if((err=db->put(db, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    goto done;
	}

	if ( !(i_id % 100) ) 
	{
	    printf(".");
	    if ( !(i_id % 5000) ) printf(" %ld\n",i_id);
	}
    }
    
    printf("Item Done. \n");

 done:
    if((err = db->close(db, 0)))
    {
	db_error("DB->close", err);
    }
    return;

}


/////////////////////////////////////////////////////////////////////////

/*
 * Loads the Warehouse table
 * Loads Stock, District as Warehouses are created
 */
void 
LoadWare(void)
{
    long    w_id;
    char    w_name[11];
    char    w_street_1[21];
    char    w_street_2[21];
    char    w_city[21];
    char    w_state[3];
    char    w_zip[10];
    float   w_tax;
    float   w_ytd;

    DB     *dbp;
    char   *name = WAREHOUSE_INDEX_NAME;
    char   *stock_name = STOCK_INDEX_NAME;
    char   *district_name = DISTRICT_INDEX_NAME;

    int err;

    if(create_db(db_env, &dbp, DB_WH_PAGE_SIZE,  0) || open_db(dbp, name, 0)) 
	return;

    if(create_db(db_env, &dbp_stock, DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_stock, stock_name, 0)) 
	goto done;

    if(create_db(db_env, &dbp_district, DB_DS_PAGE_SIZE, 0)) 
	goto done;

    if((err = dbp_district->set_bt_compare(dbp_district, district_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	goto done;
    }
    
    if(open_db(dbp_district, district_name, 0)) 
    {
	goto done;
    }

    printf("Loading Warehouse \n");
    for (w_id=1L; w_id<=count_ware; w_id++) 
    { 
	DBT key, data;
	WAREHOUSE_PRIMARY_KEY w_key;
	WAREHOUSE_PRIMARY_DATA w_data;

	memset(&w_key, 0, sizeof(WAREHOUSE_PRIMARY_KEY));
	memset(&w_data, 0, sizeof(WAREHOUSE_PRIMARY_DATA));

	/* Generate Warehouse Data */
	MakeAlphaString( 6, 10, w_name);
	MakeAddress( w_street_1, w_street_2, w_city, w_state, w_zip );
	w_tax=((float)random1(10L,20L))/100.0; 
	w_ytd=3000000.00;

	w_key.W_ID = w_id;

	memcpy(&w_data.W_NAME, &w_name, 11);
	memcpy(&w_data.W_STREET_1, &w_street_1, 21);
	memcpy(&w_data.W_STREET_2, &w_street_2, 21);
	memcpy(&w_data.W_CITY, &w_city, 21);
	memcpy(&w_data.W_STATE, &w_state, 3);
	memcpy(&w_data.W_ZIP, &w_zip, 10);
	w_data.W_TAX = w_tax;
	w_data.W_YTD = w_ytd;
  
	/* Initialize the key */
	memset(&key, 0, sizeof(key));
	key.data = &w_key;
	key.size = sizeof(WAREHOUSE_PRIMARY_KEY);

	/* Initialize the data */
	memset(&data, 0, sizeof(data));
	data.data = &w_data;
	data.size = sizeof(WAREHOUSE_PRIMARY_DATA);

	if ( option_debug )
	    printf( "WID = %ld, Name= %16s, Tax = %5.2f\n",
		    w_id, w_name, w_tax );

	if((err=dbp->put(dbp, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    goto done;
	}

	/** Make Rows associated with Warehouse **/
	if(Stock(w_id) || District(w_id))
	{
	    goto done;
	}
    }


 done:
    if(dbp)
	dbp->close(dbp, 0);
    if(dbp_stock)
	dbp_stock->close(dbp_stock, 0);
    if(dbp_district)
	dbp_district->close(dbp_district, 0);
}

/////////////////////////////////////////////////////////////////////////

/*
 * Loads the Customer Table
 */
void 
LoadCust(void)
{
    long    w_id;
    long    d_id;
    char   *name = CUSTOMER_INDEX_NAME;
    char   *sec_name = CUSTOMER_SECONDARY_NAME;
    char   *hist_name = HISTORY_INDEX_NAME;
    DB     *dbp_sec;
    int     err;

    if(create_db(db_env, &dbp_customer,DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_customer, name, 0)) 
	return;

    if(create_db(db_env, &dbp_sec, DB_COMMON_PAGE_SIZE, DB_DUP | DB_DUPSORT))
	return;
    
    if((err = dbp_sec->set_bt_compare(dbp_sec, customer_secondary_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	goto done;
    }
    
    if(open_db(dbp_sec, sec_name, 0)) 
	return;

    if ((err = dbp_customer->associate(dbp_customer, 0, dbp_sec, get_customer_sec_key, 0)) != 0)
    {
	db_error("DB->associate failed: %s\n", err);
	goto done;
    }

    if(create_db(db_env, &dbp_history, DB_COMMON_PAGE_SIZE, 0) || open_db(dbp_history, hist_name, 0)) 
	return;

    for (w_id=1L; w_id<=count_ware; w_id++) 
	for (d_id=1L; d_id<=DIST_PER_WARE; d_id++) 
	{
	    if(Customer(d_id,w_id))
		goto done;
	}
   
 done:
   if(dbp_customer)
       dbp_customer->close(dbp_customer, 0);
   if(dbp_sec)
       dbp_sec->close(dbp_sec, 0);
   if(dbp_history)
       dbp_history->close(dbp_history, 0);
}

/////////////////////////////////////////////////////////////////////////

/*
 * Loads the Orders and Order_Line Tables
 */
void 
LoadOrd(void)
{
    long  w_id;
    float w_tax;
    long  d_id;
    float d_tax;

    char *order_name = ORDER_INDEX_NAME;
    char *order_sec_name = ORDER_SECONDARY_NAME;
    char *neworder_name = NEWORDER_INDEX_NAME;
    char *orderline_name = ORDERLINE_INDEX_NAME;
    
    DB   *dbp_order_sec;

    int err;

    
    /* Create primary and secondary indices for the order and 
     * make the appropriate associations.
     */
    if(create_db(db_env, &dbp_order, DB_COMMON_PAGE_SIZE, 0))
	return;

    if((err = dbp_order->set_bt_compare(dbp_order, order_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	goto done;
    }
    
    if(open_db(dbp_order, order_name, 0)) 
	return;

    if(create_db(db_env, &dbp_order_sec,  DB_COMMON_PAGE_SIZE, DB_DUP | DB_DUPSORT) || open_db(dbp_order_sec, order_sec_name, 0)) 
	goto done;
    
    if ((err = dbp_order->associate(dbp_order,0, dbp_order_sec, get_order_sec_key, 0)) != 0)
    {
	db_error("DB->associate failed: %s\n", err);
	goto done;
    }
    
    /* Create neworder index and set a custom comparison function */
    if(create_db(db_env, &dbp_neworder, DB_COMMON_PAGE_SIZE, 0)) 
	goto done;

    if((err = dbp_neworder->set_bt_compare(dbp_neworder, neworder_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	goto done;
    }
    
    if(open_db(dbp_neworder, neworder_name, 0)) 
    {
	goto done;
    }

    /* Create orderline index and set a custom comparison function */
    if(create_db(db_env, &dbp_orderline, DB_OL_PAGE_SIZE, 0)) 
	goto done;

    if((err = dbp_orderline->set_bt_compare(dbp_orderline, orderline_comparison_func)))
    {
	db_error("DB->set_bt_compare", err);
	goto done;
    }
    
    if(open_db(dbp_orderline, orderline_name, 0)) 
    {
	goto done;
    }
    
    /* Load the tables! */
    for (w_id=1L; w_id<=count_ware; w_id++)
    { 
	for (d_id=1L; d_id<=DIST_PER_WARE; d_id++) 
	{
	    if(Orders(d_id, w_id))
	    {
		goto done;
	    }
	}
    }
    
 done:
    if(dbp_order)
	dbp_order->close(dbp_order, 0);
    if(dbp_order_sec)
	dbp_order_sec->close(dbp_order_sec, 0);
    if(dbp_neworder)
	dbp_neworder->close(dbp_neworder, 0);
    if(dbp_orderline)
	dbp_orderline->close(dbp_orderline, 0);

}


/////////////////////////////////////////////////////////////////////////

/*
 * Loads the Stock table
 *
 * w_id - warehouse id 
 */
int
Stock(long w_id)
{
    long    s_i_id;
    long    s_w_id;
    long    s_quantity;
    char    s_dist_01[25];
    char    s_dist_02[25];
    char    s_dist_03[25];
    char    s_dist_04[25];
    char    s_dist_05[25];
    char    s_dist_06[25];
    char    s_dist_07[25];
    char    s_dist_08[25];
    char    s_dist_09[25];
    char    s_dist_10[25];
    char    s_data[51];
    
    int     sdatasiz;
    long    orig[MAXITEMS];
    long    pos;
    int     i, err;
    
    DBT     key, data;
    STOCK_PRIMARY_DATA s_primdata;
    STOCK_PRIMARY_KEY s_key;

    
    printf("Loading Stock Wid=%ld\n",w_id);
    s_w_id=w_id;
    
    memset(orig, 0, MAXITEMS * sizeof(long));
    
    for (i=0; i<MAXITEMS/10; i++) 
    {
      do
      {
         pos=random1(0L,MAXITEMS);
      } while (orig[pos]);
      orig[pos] = 1;
    }

    for (s_i_id=1; s_i_id<=MAXITEMS; s_i_id++) 
    {
	memset(&s_key, 0, sizeof(STOCK_PRIMARY_KEY));
	memset(&s_primdata, 0, sizeof(STOCK_PRIMARY_DATA));
      
	s_key.S_W_ID = s_w_id;
	s_key.S_I_ID = s_i_id;

	/* Generate Stock Data */
	s_quantity=random1(10L,100L);
	MakeAlphaString(24,24,s_dist_01);
	MakeAlphaString(24,24,s_dist_02);
	MakeAlphaString(24,24,s_dist_03);
	MakeAlphaString(24,24,s_dist_04);
	MakeAlphaString(24,24,s_dist_05);
	MakeAlphaString(24,24,s_dist_06);
	MakeAlphaString(24,24,s_dist_07);
	MakeAlphaString(24,24,s_dist_08);
	MakeAlphaString(24,24,s_dist_09);
	MakeAlphaString(24,24,s_dist_10);
	sdatasiz=MakeAlphaString(26,50,s_data);

	memcpy(s_primdata.S_DIST_01, &s_dist_01, 25);
	memcpy(s_primdata.S_DIST_02, &s_dist_02, 25);
	memcpy(s_primdata.S_DIST_03, &s_dist_03, 25);
	memcpy(s_primdata.S_DIST_04, &s_dist_04, 25);
	memcpy(s_primdata.S_DIST_05, &s_dist_05, 25);
	memcpy(s_primdata.S_DIST_06, &s_dist_06, 25);
	memcpy(s_primdata.S_DIST_07, &s_dist_07, 25);
	memcpy(s_primdata.S_DIST_08, &s_dist_08, 25);
	memcpy(s_primdata.S_DIST_09, &s_dist_09, 25);
	memcpy(s_primdata.S_DIST_10, &s_dist_10, 25);

	if (orig[s_i_id-1])
	{
	    pos=random1(0L,sdatasiz-8);
	    s_data[pos]='o'; 
	    s_data[pos+1]='r'; 
	    s_data[pos+2]='i'; 
	    s_data[pos+3]='g'; 
	    s_data[pos+4]='i'; 
	    s_data[pos+5]='n'; 
	    s_data[pos+6]='a'; 
	    s_data[pos+7]='l'; 
	}
  
	s_primdata.S_QUANTITY = s_quantity;

	memcpy(s_primdata.S_DATA, &s_data, 51);
	s_primdata.S_YTD = 0;
	s_primdata.S_ORDER_CNT = 0;
	s_primdata.S_REMOTE_CNT = 0;
	
	memset(&key, 0, sizeof(DBT));
	key.data = &s_key;
	key.size = sizeof(STOCK_PRIMARY_KEY);

	memset(&data, 0, sizeof(DBT));
	data.data = &s_primdata;
	data.size = sizeof(STOCK_PRIMARY_DATA);
	
	if((err=dbp_stock->put(dbp_stock, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    return -1;
	}
	
	if ( option_debug )
	    printf( "SID = %ld, WID = %ld, Quan = %ld\n",
		    s_i_id, s_w_id, s_quantity );
	if ( !(s_i_id % 100) ) 
	{
	    printf(".");
	    if ( !(s_i_id % 5000) ) printf(" %ld\n",s_i_id);
	}
    }

    printf(" Stock Done.\n");
    
    return 0;
}


/////////////////////////////////////////////////////////////////////////

/*
 * Loads the District table 
 *      w_id - warehouse id 
 */
int
District(long w_id)
{
    long    d_id;
    long    d_w_id;
    char    d_name[11];
    char    d_street_1[21];
    char    d_street_2[21];
    char    d_city[21];
    char    d_state[3];
    char    d_zip[10];
    float   d_tax;
    float   d_ytd;
    long    d_next_o_id;
    
    DBT     key, data;
    DISTRICT_PRIMARY_KEY d_key;
    DISTRICT_PRIMARY_DATA d_data;

    int err;

    printf("Loading District\n");

    
    for (d_id=1; d_id<=DIST_PER_WARE; d_id++) 
    {
	memset(&d_key, 0, sizeof(DISTRICT_PRIMARY_KEY));
	memset(&d_data, 0, sizeof(DISTRICT_PRIMARY_DATA));

	d_w_id=w_id;
	d_ytd=30000.0;
	d_next_o_id=3001L;
	
	d_key.D_W_ID = d_w_id;
	d_key.D_ID = d_id;
    
	d_data.D_YTD = d_ytd;
	d_data.D_NEXT_O_ID = d_next_o_id;
		
	/* Generate District Data */
	MakeAlphaString(6L,10L,d_name);
	MakeAddress( d_street_1, d_street_2, d_city, d_state, d_zip );
	d_tax=((float)random1(10L,20L))/100.0; 
	
	memcpy(&d_data.D_NAME, &d_name, 11);
	memcpy(&d_data.D_STREET_1, &d_street_1, 21);
	memcpy(&d_data.D_STREET_2, &d_street_2, 21);
	memcpy(&d_data.D_CITY, &d_city, 21);
	memcpy(&d_data.D_STATE, &d_state, 3);
	memcpy(&d_data.D_ZIP, &d_zip, 10);

	d_data.D_TAX = d_tax;
	
	memset(&key, 0, sizeof(DBT));
	key.data = &d_key;
	key.size = sizeof(DISTRICT_PRIMARY_KEY);

	memset(&data, 0, sizeof(DBT));
	data.data = &d_data;
	data.size = sizeof(DISTRICT_PRIMARY_DATA);


	if((err=dbp_district->put(dbp_district, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    return -1;
	}

      if ( option_debug )
	  printf( "DID = %ld, WID = %ld, Name = %10s, Tax = %5.2f\n",
		  d_id, d_w_id, d_name, d_tax );

    }
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////

/*
 * Loads Customer Table
 * Also inserts corresponding history record
 *
 * id   - customer id
 * d_id - district id
 * w_id - warehouse id
 */
int 
Customer(long d_id, long w_id )
{
    long    c_id;
    long    c_d_id;
    long    c_w_id;
    char    c_first[17];
    char    c_middle[3];
    char    c_last[17];
    char    c_street_1[21];
    char    c_street_2[21];
    char    c_city[21];
    char    c_state[3];
    char    c_zip[10];
    char    c_phone[17];
    char    c_since[12];
    char    c_credit[3];
    long    c_credit_lim;
    float   c_discount;
    float   c_balance;
    char    c_data[501];
    float   h_amount;
    char    h_data[25];
    int     err;

    
    printf("Loading Customer for DID=%ld, WID=%ld\n",d_id,w_id);

    for (c_id=1; c_id<=CUST_PER_DIST; c_id++) 
    {
	DBT     key, data;
	CUSTOMER_PRIMARY_KEY  c_key;
	CUSTOMER_PRIMARY_DATA c_primdata;
	HISTORY_PRIMARY_KEY  h_key;

	memset(&c_key, 0, sizeof(CUSTOMER_PRIMARY_KEY));
	memset(&c_primdata, 0, sizeof(CUSTOMER_PRIMARY_DATA));
	
	memset(&h_key, 0, sizeof(HISTORY_PRIMARY_KEY));

	/* Generate Customer Data */
	c_d_id=d_id;
	c_w_id=w_id;
	
	MakeAlphaString( 8, 16, c_first );
	
	c_middle[0]='O'; c_middle[1]='E'; c_middle[2] = '\0';
	
	if (c_id <= 1000)
	    Lastname(c_id-1,c_last);
	else
	    Lastname(NURand(255,0,999),c_last);
	
	MakeAddress( c_street_1, c_street_2, c_city, c_state, c_zip );
	MakeNumberString( 16, 16, c_phone );
	
	if (random1(0L,1L)) 
	    c_credit[0]='G';
	else 
	    c_credit[0]='B';
	
	c_credit[1]='C'; c_credit[2]='\0'; 
	c_credit_lim=50000;
	c_discount=((float)random1(0L,50L))/100.0; 
	c_balance= -10.0;
	MakeAlphaString(300,500,c_data);
	
	/* Prepare for putting into the database */
	c_key.C_W_ID = c_w_id;
	c_key.C_D_ID = c_d_id;
	c_key.C_ID = c_id;
	
	memcpy(&c_primdata.C_FIRST, &c_first, 17);
	memcpy(&c_primdata.C_MIDDLE, &c_middle, 3);
	memcpy(&c_primdata.C_LAST, &c_last, 17);
	memcpy(&c_primdata.C_STREET_1, &c_street_1, 21);
	memcpy(&c_primdata.C_STREET_2, &c_street_2, 21);
	memcpy(&c_primdata.C_CITY, &c_city, 21);
	memcpy(&c_primdata.C_STATE, &c_state, 3);
	memcpy(&c_primdata.C_ZIP, &c_zip, 10);
	memcpy(&c_primdata.C_PHONE, &c_phone, 17);
	memcpy(&c_primdata.C_DATA, &c_data, 501);
	memcpy(&c_primdata.C_CREDIT, &c_credit, 3);
	
	memcpy(c_primdata.C_CREDIT, &c_credit, 3);	
	c_primdata.C_DISCOUNT = c_discount;
	c_primdata.C_BALANCE = c_balance;

	memset(&key, 0, sizeof(DBT));
	key.data = &c_key;
	key.size = sizeof(CUSTOMER_PRIMARY_KEY);

	memset(&data, 0, sizeof(DBT));
	data.data = &c_primdata;
	data.size = sizeof(CUSTOMER_PRIMARY_DATA);
	
	if((err=dbp_customer->put(dbp_customer, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    return -1;
	}
  
	h_amount = 10.0;
	MakeAlphaString(12,24,h_data);

	memset(&key, 0, sizeof(DBT));
	key.data = &h_key;
	key.size = sizeof(HISTORY_PRIMARY_KEY);
	memset(&data, 0, sizeof(DBT));

	h_key.H_C_ID = c_id;
	h_key.H_C_D_ID = c_d_id;
	h_key.H_C_W_ID = c_w_id;
	h_key.H_W_ID = c_w_id;
	h_key.H_D_ID = c_d_id;
	h_key.H_AMOUNT = h_amount;
	memcpy(&h_key.H_DATE, &timestamp, 26);
	memcpy(&h_key.H_DATA, &h_data, 24);

	if((err=dbp_history->put(dbp_history, 0, &key, &data, 0)))
	{
	    db_error("DB->put", err);
	    return -1;
	}
	
	if ( option_debug )
	    printf( "CID = %ld, LST = %s, P# = %s\n",
		    c_id, c_last, c_phone );
	if ( !(c_id % 100) ) 
	{
	    printf(".");
	    if ( !(c_id % 1000) ) printf(" %ld\n",c_id);
	}
    }  
    printf("Customer Done.\n");

  return 0;

}

/////////////////////////////////////////////////////////////////////////

/*
 *  Loads the Orders table 
 *  Also loads the Order_Line table on the fly 
 *
 *  w_id - warehouse id 
 *  d_id - district id
 */
int
Orders(long d_id, long w_id)
{

    long    o_id;
    long    o_c_id;
    long    o_d_id;
    long    o_w_id;
    long    o_carrier_id;
    long    o_ol_cnt;
    long    ol;
    long    ol_i_id;
    long    ol_supply_w_id;
    long    ol_quantity;
    long    ol_amount;
    char    ol_dist_info[25];
    float   i_price;
    float   c_discount;
    
    int     err;
    DBT     key, data;
    
    ORDER_PRIMARY_KEY     o_key;
    ORDER_PRIMARY_DATA    o_data;

    ORDERLINE_PRIMARY_KEY  ol_key;
    ORDERLINE_PRIMARY_DATA ol_data;

    NEWORDER_PRIMARY_KEY   no_key;


    printf("Loading Orders for D=%ld, W= %ld\n", d_id, w_id);

    o_d_id=d_id;
    o_w_id=w_id;
    InitPermutation();           /* initialize permutation of customer numbers */


    for (o_id=1; o_id<=ORD_PER_DIST; o_id++) 
    {
	memset(&o_key, 0, sizeof(ORDER_PRIMARY_KEY));
	memset(&o_data, 0, sizeof(ORDER_PRIMARY_DATA));

	o_key.O_D_ID = o_d_id;
	o_key.O_W_ID = o_w_id;	
	o_key.O_ID = o_id;
	
	/* Generate Order Data */
	o_c_id = GetPermutation();
	o_carrier_id = random1(1L,10L); 
	o_ol_cnt=random1(5L,15L); 
  
	o_data.O_C_ID = o_c_id;
	o_data.O_ALL_LOCAL = 1;
	o_data.O_OL_CNT = o_ol_cnt;
	memcpy(&o_data.O_ENTRY_D, timestamp, 26);


	if (o_id > 2100)         /* the last 900 orders have not been delivered) */
	{
	    memset(&no_key, 0, sizeof(NEWORDER_PRIMARY_KEY));

	    o_data.O_CARRIER_ID = 0;
	    
	    memset(&key, 0, sizeof(DBT));
	    key.data = &o_key;
	    key.size = sizeof(ORDER_PRIMARY_KEY);

	    memset(&data, 0, sizeof(DBT));
	    data.data = &o_data;
	    data.size = sizeof(ORDER_PRIMARY_DATA);
	    
	    if((err=dbp_order->put(dbp_order, 0, &key, &data, 0)))
	    {
		db_error("DB->put", err);
		return -1;
	    }
	 
	    no_key.NO_O_ID = o_id;
	    no_key.NO_D_ID = o_d_id;
	    no_key.NO_W_ID = o_w_id;
	    
	    memset(&key, 0, sizeof(DBT));
	    key.data = &no_key;
	    key.size = sizeof(NEWORDER_PRIMARY_KEY);

	    memset(&data, 0, sizeof(DBT));
	    
	    if((err=dbp_neworder->put(dbp_neworder, 0, &key, &data, 0)))
	    {
		db_error("DB->put", err);
		return -1;
	    }
	}
	else
	{
	    o_data.O_CARRIER_ID = o_carrier_id;
        
	    memset(&key, 0, sizeof(DBT));
	    key.data = &o_key;
	    key.size = sizeof(ORDER_PRIMARY_KEY);

	    memset(&data, 0, sizeof(DBT));
	    data.data = &o_data;
	    data.size = sizeof(ORDER_PRIMARY_DATA);
	    
	    if((err=dbp_order->put(dbp_order, 0, &key, &data, 0)))
	    {
		db_error("DB->put", err);
		return -1;
	    }

	}

	if ( option_debug )
	    printf( "OID = %ld, CID = %ld, DID = %ld, WID = %ld\n",
		    o_id, o_c_id, o_d_id, o_w_id);
      
	for (ol=1; ol<=o_ol_cnt; ol++) 
	{
	    memset(&ol_key, 0, sizeof(ORDERLINE_PRIMARY_KEY));
	    memset(&ol_data, 0, sizeof(ORDERLINE_PRIMARY_DATA));

	    /* Generate Order Line Data */
	    ol_i_id=random1(1L,MAXITEMS); 
	    ol_supply_w_id=o_w_id; 
	    ol_quantity=5; 
	    ol_amount=0.0;

	    MakeAlphaString(24,24,ol_dist_info);

	    ol_key.OL_W_ID = o_w_id;
	    ol_key.OL_D_ID = o_d_id;
	    ol_key.OL_O_ID = o_id;
	    ol_key.OL_NUMBER = ol;

	    ol_data.OL_I_ID = ol_i_id;
	    ol_data.OL_SUPPLY_W_ID = o_w_id;
	    ol_data.OL_QUANTITY = ol_quantity;
	    memcpy(&ol_data.OL_DIST_INFO, &ol_dist_info, 25);

	    
	    if (o_id > 2100)
	    {
		ol_data.OL_AMOUNT = ol_amount;

		memset(&key, 0, sizeof(DBT));
		key.data = &ol_key;
		key.size = sizeof(ORDERLINE_PRIMARY_KEY);

		memset(&data, 0, sizeof(DBT));
		data.data = &ol_data;
		data.size = sizeof(ORDERLINE_PRIMARY_DATA);

		if((err=dbp_orderline->put(dbp_orderline, 0, &key, &data, 0)))
		{
		    db_error("DB->put", err);
		    return -1;
		}
	    }
	    else
	    {
		const int dt_size = 26;
		char datetime[dt_size];
		time_t t_clock;

		if((int)time(&t_clock) == -1)
		{
		    error("time");
		    exit(1);
		}
		ctime_r(&t_clock, (char*)datetime);


		ol_data.OL_AMOUNT =(float)(random1(10L, 10000L))/100.0; 
		memcpy(&ol_data.OL_DELIVERY_D, datetime, dt_size);
		
		memset(&key, 0, sizeof(DBT));
		key.data = &ol_key;
		key.size = sizeof(ORDERLINE_PRIMARY_KEY);

		memset(&data, 0, sizeof(DBT));
		data.data = &ol_data;
		data.size = sizeof(ORDERLINE_PRIMARY_DATA);

		if((err=dbp_orderline->put(dbp_orderline, 0, &key, &data, 0)))
		{
		    db_error("DB->put", err);
		    return -1;
		}
	    }

	    if ( option_debug )
		printf( "OL = %ld, IID = %ld, QUAN = %ld, AMT = %8.2f\n",
			ol, ol_i_id, ol_quantity, ol_data.OL_AMOUNT);

      }
	if ( !(o_id % 100) ) {
	    printf(".");
	    
	    if ( !(o_id % 1000) ) printf(" %ld\n",o_id);
	}
    }

    printf("Orders Done.\n");
    return 0;
}

/////////////////////////////////////////////////////////////////////////

/*
 * ROUTINE NAME
 *      Lastname
 * DESCRIPTION
 *      TPC-C Lastname Function.
 * ARGUMENTS 
 *      num  - non-uniform random number
 *      name - last name string
 */
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
    /* Fill the rest with spaces */
    for( ; i<16; i++)
    {
	name[i] = ' ';
    }
    name[16] = '\0';

    return;
}

/////////////////////////////////////////////////////////////////////////

/* Make a string of letter */
static int 
MakeAlphaString(int min, int max, char *str)
{
    static char character[] = 
/***  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; */
	"abcedfghijklmnopqrstuvwxyz";
    int length;
    int i;

    length = random1(min, max);

    for (i=0;  i<length;  i++)
    {
        str[i] = character[random1(0, sizeof(character)-2)];
    }
    str[length] = '\0';

    return length;
}

/////////////////////////////////////////////////////////////////////////

/* Make a string of letter */
static int 
MakeNumberString(int min, int max, char *str)
{
    static char character[] = 
/***  "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; */
	"1234567890";
    int length;
    int i;

    length = random1(min, max);

    for (i=0;  i<length;  i++)
    {
        str[i] = character[random1(0, sizeof(character)-2)];
    }
    str[length] = '\0';

    return length;
}

/////////////////////////////////////////////////////////////////////////

void
MakeAddress(char *str1, char *str2, char *city, char *state, char *zip)
{
    MakeAlphaString(10,20,str1); /* Street 1*/
    MakeAlphaString(10,20,str2); /* Street 2*/
    MakeAlphaString(10,20,city); /* City */
    MakeAlphaString(2,2,state);  /* State */
    MakeNumberString(9,9,zip);   /* Zip */
}

/////////////////////////////////////////////////////////////////////////

/*
 * Functions for generating customer ID's
 */

static short cid_array[CUST_PER_DIST+1];           /* Array element 0 not used */

void
InitPermutation()
{
    int i;
    
    for (i=0; i <= CUST_PER_DIST; i++)
	cid_array[i] = 0;
}

int
GetPermutation()
{
    int r;

    while (1) {
	r = random1(1, CUST_PER_DIST);
	if (cid_array[r])               /* This number already taken */
	    continue;
	cid_array[r] = 1;               /* mark taken */
	return(r);
    }
	
}

/////////////////////////////////////////////////////////////////////////

/*
 * Check that none of the databases we are about to create exist
 * in the home directory.
 */
static int
databases_exist(void)
{
    int ret, fd, len_hd;
    char fname[MAXPATHLEN], *db_name;

    assert(home_dir != NULL);
    
    len_hd = strlen(home_dir);
    assert(len_hd < MAXPATHLEN);
    
    strncpy(fname, home_dir, len_hd);
    if(fname[len_hd -1] != '/')
    {
	assert(len_hd < MAXPATHLEN - 1);
	strncpy(fname + len_hd, "/", 1);
	len_hd++;
    }

    for(i = 0; (db_name = database_names[i]) != NULL; i++)
    {
	int len = strlen(db_name);
	assert(len + len_hd < MAXPATHLEN);
	
	/* Attach the db name to the name of the home directory */
	strncpy(fname + len_hd, db_name, strlen(db_name));

	/* Now we have the full path. Check if the file exists */
	fd = open(fname, 0);
	if(fd >= 0)
	{
	    printf("Database %s exists\n", fname);
	    close(fd);
	    return -1;
	}
    }
    return 0;
}
