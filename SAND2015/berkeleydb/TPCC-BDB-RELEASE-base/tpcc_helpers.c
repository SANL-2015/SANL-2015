#include <errno.h>

#include <db.h>
#include "tpcc_globals.h"
#include "tpcc_schema.h"
#include "log.h"
#include "tpcc_trans.h"
#include <fcntl.h>

int do_autocommit;


/////////////////////////////////////////////////////////////////////////
long bytes_read, bytes_written;


void
print_stats(void)
{
    fprintf(stderr, "BYTES READ: %lu\n", bytes_read);
    fprintf(stderr, "BYTES WRITTEN: %lu\n", bytes_written);
}


void
print_memp_stat(DB_MPOOL_STAT *mpool_stat)
{
    printf("\nMemory pool statistics:\n");
    printf("=======================\n");
    printf("st_gbytes      = %d\n", mpool_stat->st_gbytes);
    printf("st_bytes       = %d\n", mpool_stat->st_bytes);
    printf("st_regsize     = %d\n", mpool_stat->st_regsize);
    printf("st_map         = %d\n", mpool_stat->st_map);
    printf("st_page_create = %d\n", mpool_stat->st_page_create);
    printf("st_pages       = %d\n", mpool_stat->st_pages);
    printf("st_alloc       = %d\n", mpool_stat->st_alloc);
    printf("st_page_in     = %d\n", mpool_stat->st_page_in);
    printf("st_page_out    = %d\n", mpool_stat->st_page_out);
    printf("st_cache_hit   = %d\n", mpool_stat->st_cache_hit);
    printf("st_cache_miss  = %d\n", mpool_stat->st_cache_miss);
    printf("=======================\n\n");
}

void
print_mutex_stat(DB_MUTEX_STAT *mutex_stat)
{
    printf("\nMutex region statistics:\n");
    printf("=======================\n");
    printf("st_region_wait   = %d\n", mutex_stat->st_region_wait);
    printf("st_region_nowait = %d\n", mutex_stat->st_region_nowait);
    printf("=======================\n");

}

/////////////////////////////////////////////////////////////////////////
char *prefix = "pp";
void
errcall(const DB_ENV *env, const char* prefix, const char *message)
{
    write_log( "%s\n", message);
}


/////////////////////////////////////////////////////////////////////////
/*
 * Open simple Berkeley DB environment. We don't care about
 * durability here, and we are doing single-threaded accees.
 * So don't do locking, logging and transactions.
 */
int
init_environment(DB_ENV **envp, char *home_dir, int add_flags)
{
    int err = 0;
    long lg_max;


    if((err = db_env_create(envp, 0)))
    {
	db_error("db_env_create", err);
	return err;
    }

    (*envp)->set_errcall(*envp, errcall);

    /*If we are doing locking, then do deadlock detection */
    if(add_flags & DB_INIT_LOCK)
    {
        /* Do deadlock detection internally. */
	if ((err = (*envp)->set_lk_detect(*envp, DB_LOCK_DEFAULT)) != 0) 
	{
	    db_error("set_lk_detect: DB_LOCK_DEFAULT", err);
	    return err;
	}
    }

    if((err = (*envp)->set_cachesize(*envp, 0, DB_CACHE_SIZE, 0)))
    {
	db_error("DB_ENV->set_cachesize", err);
	return err;
    }	
    
    if((err = (*envp)->set_data_dir(*envp, home_dir)))
    {
	db_error("DB_ENV->set_data_dir", err);
	return err;
    }


    lg_max = 200*1024*1024*4;
    if((err = (*envp)->set_lg_max(*envp, lg_max)))
    {
	db_error("DB_ENV->set_lg_max", err);
	return err;
    }

    /* Set the log buffer to X MB, so that the log is not 
     * flushed very often 
     */
    if((err = (*envp)->set_lg_bsize(*envp, 512*1024)))
    {
	db_error("DB_ENV->set_lg_bsize", err);
	return err;
    }

    /* Set the shared memory key */
    if(add_flags & DB_SYSTEM_MEM)
    {
	if(( err = (*envp)->set_shm_key(*envp, 12)))
	{
	    db_error("DB_ENV->set_shm_key", err);
	    return err;
	}
    }

    /* Set the size of the lock region */
    if((err = (*envp)->set_lk_max_locks(*envp, 10000)))
    {
	db_error("DB_ENV->set_lk_max_locks", err);
	return err;
    }

    if((err = (*envp)->set_lk_max_lockers(*envp, 10000)))
    {
	db_error("DB_ENV->set_lk_max_locks", err);
	return err;
    }

    if((err = (*envp)->set_lk_max_objects(*envp, 10000)))
    {
	db_error("DB_ENV->set_lk_max_locks", err);
	return err;
    }
    

    if((err = (*envp)->open(*envp, home_dir, DB_INIT_MPOOL | add_flags | DB_CREATE, 0)))
    {
	db_error("DB_ENV->open", err);
	return err;
    }	
          
    return 0;
}

/////////////////////////////////////////////////////////////////////////

/*
 * Create new db handle.
 */
int
create_db(DB_ENV *db_envp, DB **dbp, int PAGE_SIZE, int add_flags)
{
    int err;

    if(( err = db_create(dbp, db_envp, 0)))
    {
	db_error("db_create", err);
	return err;
    }
    if((err = (*dbp)->set_pagesize(*dbp, PAGE_SIZE)))
    {
	db_error("DB->set_pagesize", err);
	return err;
    }
    if(add_flags)
    {
	if ((err = (*dbp)->set_flags(*dbp, add_flags)) != 0)
	{
	    db_error("DB->set_flags", err);
	    return err;
	}
    }
    return 0;
}


/*
 * Open database with standard settings and additional flags
 */
int
open_db(DB *dbp, char *db_name, int flags)
{
    int err;
    
    if(do_autocommit)
    {
#if TRANSACTIONS
	flags |= DB_AUTO_COMMIT;
#endif
    }

    if((err = dbp->open(dbp, 0, db_name, NULL, DB_BTREE, DB_CREATE | flags, 0)))
    {   
	db_error("DB->open", err);
	return err;
    }
    
    return 0;
}

int
scan_db(DB *dbp)
{
    DBC *cursor;
    int ret, num_recs = 0;
    DBT key, data;

    ret = dbp->cursor(dbp, 0, &cursor, 0);
    if(ret)
    {
	db_error("DB->cursor", ret);
	return ret;
    }

    memset(&key, 0, sizeof(DBT));    
    memset(&data, 0, sizeof(DBT));
    
    while( (ret = cursor->c_get(cursor, &key, &data, DB_NEXT)) == 0)
	num_recs++;
    
    if(ret == DB_NOTFOUND)
	printf("Scan of %s completed successfully. Scanned %d records\n",
	       dbp->fname, num_recs);
    else
    {
	db_error("DBC->get", ret);
	return ret;
    }
    cursor->c_close(cursor);
    return 0;
}

/////////////////////////////////////////////////////////////////////////

/*
 * Display database error string
 */
void
db_error(char *func_name, int err)
{
    write_log( "%s failed: %s\n", func_name, db_strerror(err));
}

/*
 * Display non-database related error string.
 */
void
error(char *func_name)
{
    write_log("%s failed: %s\n", func_name, strerror(errno));
}
    
/////////////////////////////////////////////////////////////////////////
void
close_environment(DB_ENV *db_envp)
{
    db_envp->close(db_envp, 0);
#if DAFS
    dafs_disconnect();
#endif
}

/////////////////////////////////////////////////////////////////////////
/* 
 * By using custom comparison functions we get around needing
 * secondary indices. We sort keys by a subset of keys that
 * could be a secondary key. This way, we can retrieve a 
 * group of keys corresponding to that subset through a 
 * cursor. 
 * 
 * For example, in the district_comparison_func, we force
 * the keys belonging to the same warehouse to be grouped
 * together, so that later we can retrieve records for
 * all districts for a given warehouse. 
 */

int          
district_comparison_func(DB *dbp, const DBT *a, const DBT *b)
{
    DISTRICT_PRIMARY_KEY key_a, key_b;
    
    memcpy(&key_a, a->data, sizeof(DISTRICT_PRIMARY_KEY));
    memcpy(&key_b, b->data, sizeof(DISTRICT_PRIMARY_KEY));

    if(key_a.D_W_ID == key_b.D_W_ID)
    {
	return key_a.D_ID - key_b.D_ID;
    }
    else
    {
	return key_a.D_W_ID - key_b.D_W_ID;
    }

}

int          
orderline_comparison_func(DB *dbp, const DBT *a, const DBT *b)
{
    ORDERLINE_PRIMARY_KEY key_a, key_b;
    
    memcpy(&key_a, a->data, sizeof(ORDERLINE_PRIMARY_KEY));
    memcpy(&key_b, b->data, sizeof(ORDERLINE_PRIMARY_KEY));

    if(key_a.OL_W_ID != key_b.OL_W_ID)
    {
	return key_a.OL_W_ID - key_b.OL_W_ID;
    }
    
    if(key_a.OL_D_ID != key_b.OL_D_ID)
    {
	return key_a.OL_D_ID - key_b.OL_D_ID;
    }
	
    if(key_a.OL_O_ID != key_b.OL_O_ID)
    {
	return -(key_a.OL_O_ID - key_b.OL_O_ID);
    }

    return key_a.OL_NUMBER - key_b.OL_NUMBER;
}

int          
neworder_comparison_func(DB *dbp, const DBT *a, const DBT *b)
{   
    NEWORDER_PRIMARY_KEY key_a, key_b;
    
    memcpy(&key_a, a->data, sizeof(NEWORDER_PRIMARY_KEY));
    memcpy(&key_b, b->data, sizeof(NEWORDER_PRIMARY_KEY));

    if(key_a.NO_W_ID != key_b.NO_W_ID)
    {
	return key_a.NO_W_ID - key_b.NO_W_ID;
    }
    
    if(key_a.NO_D_ID != key_b.NO_D_ID)
    {
	return key_a.NO_D_ID - key_b.NO_D_ID;
    }
    
    return  key_a.NO_O_ID - key_b.NO_O_ID;
}

int          
order_comparison_func(DB *dbp, const DBT *a, const DBT *b)
{
    ORDER_PRIMARY_KEY key_a, key_b;
    
    memcpy(&key_a, a->data, sizeof(ORDER_PRIMARY_KEY));
    memcpy(&key_b, b->data, sizeof(ORDER_PRIMARY_KEY));

    if(key_a.O_W_ID != key_b.O_W_ID)
    {
	return key_a.O_W_ID - key_b.O_W_ID;
    }
    
    if(key_a.O_D_ID != key_b.O_D_ID)
    {
	return key_a.O_D_ID - key_b.O_D_ID;
    }
	
    return -(key_a.O_ID - key_b.O_ID);
}

int
customer_secondary_comparison_func(DB *dbp, const DBT *a, const DBT *b)
{
    CUSTOMER_SECONDARY_KEY key_a, key_b;
    int ret;

    memcpy(&key_a, a->data, sizeof(CUSTOMER_SECONDARY_KEY));
    memcpy(&key_b, b->data, sizeof(CUSTOMER_SECONDARY_KEY));
    
    if(key_a.C_W_ID != key_b.C_W_ID)
    {
	return key_a.C_W_ID - key_b.C_W_ID;
    }
    if(key_a.C_D_ID != key_b.C_D_ID)
    {
	return key_a.C_D_ID - key_b.C_D_ID;
    }
    if((ret = strcmp(key_a.C_LAST, key_b.C_LAST)))
    {
	return ret;
    }
    if((ret = strcmp(key_a.C_FIRST, key_b.C_FIRST)))
    {
	return ret;
    }
    return key_a.C_ID - key_b.C_ID;
}

/////////////////////////////////////////////////////////////////////////

int          
get_customer_sec_key(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey)
{
    CUSTOMER_PRIMARY_KEY *prim_key = pkey->data;
    CUSTOMER_PRIMARY_DATA *prim_data = pdata->data;

    CUSTOMER_SECONDARY_KEY *sec_key = 
	(CUSTOMER_SECONDARY_KEY *) malloc(sizeof(CUSTOMER_SECONDARY_KEY));

    if(sec_key == NULL)
    {
	error("malloc");
	return -1;
    }

    skey->flags = DB_DBT_APPMALLOC; 
    skey->data  = sec_key;
    skey->size  = sizeof(CUSTOMER_SECONDARY_KEY);

    sec_key->C_W_ID = prim_key->C_W_ID;
    sec_key->C_D_ID = prim_key->C_D_ID;
    memcpy(sec_key->C_LAST, prim_data->C_LAST, 17);
    memcpy(sec_key->C_FIRST, prim_data->C_FIRST, 17);
    sec_key->C_ID   = prim_key->C_ID;

    return 0;
}

int          
get_order_sec_key(DB *dbp, const DBT *pkey, const DBT *pdata, DBT *skey)
{

    ORDER_PRIMARY_KEY *prim_key = pkey->data;
    ORDER_PRIMARY_DATA *prim_data = pdata->data;

    ORDER_SECONDARY_KEY *sec_key = 
	(ORDER_SECONDARY_KEY*)malloc(sizeof(ORDER_SECONDARY_KEY));

    if(sec_key == NULL)
    {
	error("malloc");
	return -1;
    }
    
    skey->flags = DB_DBT_APPMALLOC; 
    skey->data  = sec_key;
    skey->size  = sizeof(ORDER_SECONDARY_KEY);
    
    sec_key->O_W_ID = prim_key->O_W_ID;
    sec_key->O_D_ID = prim_key->O_D_ID;
    sec_key->O_C_ID = prim_data->O_C_ID;
    
    return 0;
}

/////////////////////////////////////////////////////////////////////////

/*
 * Copyright (c) 1994 by Sun Microsystems, Inc.
 *
 * 4/13/93  - Shanti S
 */

/*
 * Implements a GOOD pseudo random number generator.  This generator
 * will/should? run the complete period before repeating.
 * 
 * Copied from: 
 *	Random Numbers Generators: Good Ones Are Hard to Find.
 *	Communications of the ACM - October 1988 Volume 31 Number 10
 *
 */

#if OLD
#define	B	     16807
#define	M	2147483647	/* Largest int value */
#define	Q	    127773	/* M div B */
#define	R	      2836	/* M mod B */
#else
#define	B	     48271	/* New recommendations made by the authors */
#define	M	2147483647
#define	Q	     44488
#define	R	      3399
#endif



static int 	Seed = 1;	/* seed value for all functions */

/*
 * seed - load the Seed value used in random. 
 * Should be used before first call.
 */
void
seed(long val)
{
    if ( val > 0l )
	Seed = val;
    else if ( val < 0 )
	Seed = abs(val);
    else
	Seed = 1l;
}

/*
 * irand - returns a 32 bit integer pseudo random number 
 * with a period of 1 to 2 ^ 32 - 1.
 */
static int
irand(void)
{
    register int	s;	/* copy of seed */
    register int	test;	/* test flag */
    register int	hi;	/* tmp value for speed */
    register int	lo;	/* tmp value for speed */

    s = Seed;

    hi = s / Q;
    lo = s % Q;

    test = B * lo - R * hi;
    
    if ( test > 0 )
	Seed = test;
    else
	Seed = test + M;
    
    return( Seed );
}


/*
 * Function: drand
 * Returns a dobule pseudo random number between 0.0 and 1.0.
 */

double
drand()
{
    double tmp;

    tmp = (double)irand();
    return( tmp / M );
}


/*
 * Function :random
 * Select a random number uniformly distributed between x and y,
 * inclusively, with a mean of (x+y)/2 
 */
int
random1(int x, int y)
{
    int range, result;

    range = y - x + 1;
    if(no_rand)
	result = x + range / 2;
    else
	result = x + irand() % range;
    if ( result > y)
	result = y;
    return(result);
}


/*
 * FUnction: NURand
 * TPC_C function NURand(A, x, y) = 
 *	(((random1(0,A) | random1(x,y)) + C) % (y - x + 1)) + x
 */
int
NURand(int A, int x, int y)
{
    int nurand;
    static int C = -1;
    
    if(C == -1)
	C = A/2;

    nurand = (((random1(0, A) | random1(x, y)) + C) % ( y - x + 1)) + x;
    return(nurand);
}

/////////////////////////////////////////////////////////////////////////

char *
get_txn_type_string(txn_type_t txn_type)
{
    switch(txn_type)
    {
    case MIXED:
	return "MIXED";
    case PAYMENT:
	return "PAYMENT";
    case NEWORDER:
	return "NEWORDER";
    case ORDERSTATUS:
	return "ORDERSTATUS";
    case DELIVERY:
	return "DELIVERY";
    case STOCKLEVEL:
	return "STOCKLEVEL";
    default:
	return "Invalid";
    }
}

