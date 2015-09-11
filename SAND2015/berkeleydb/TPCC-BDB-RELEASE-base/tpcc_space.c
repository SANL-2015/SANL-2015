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
   tpcc_space.c

   To determine space requirements for TPCC tables, given # warehouses(Scale)
   and table name as a command line inputs. Output is in kilobytes.
  
*/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "tpcc_schema.h"
#include "tpcc_globals.h"


#define MAXSCALE 1000

static char USAGE[] = {
"USAGE: %s -n scale -t tablename [-t tablename ....]  \n\
	   scale: number of warehouses in the database \n\
	   tablename: One of - warehouse, district, customer, history, \n\
			       order, neworder, orderline, item, stock \n"
	};



char *tables[] = { 
    "warehouse", "district", "customer",
    "history", "orders", "neworder", 
    "orderline", "item", "stock"
};

void
usage(char *prognm)
{

    fprintf(stderr,USAGE, prognm);
}

int
main(int argc, char **argv)
{
    char *progname;
    char *tablename;
    int  scale;
	
    int k = 4, i;                    /*   first tablename   */
    long total_space = 0;

    progname = argv[0];
    if (argc < 5) {
	usage(progname);
	exit(1);
    }

    scale = atoi(argv[2]);	
    if (scale < 1 || scale > MAXSCALE) 
    {
	fprintf(stderr, "ERROR: Scale (%d) doesnt seem right!\n",scale);
	usage(progname);
	exit(1);
    }

    while(k <= argc)
    {
	tablename = argv[k];
	
	i=0;
	while (i < 9){
	    if (strcasecmp(tablename, tables[i]) == 0) 
		break;
	    i++;
	}
	
	if (i >= 9)
	{
	    fprintf(stderr,
		    "ERROR: Tablename (%s) doesnt seem right!\n",
		    tablename);
	    usage(progname);
	    exit(1);
	}
	
	/* We are going to do a brute-force estimation of the space
	 * requirement. This has to be improved to get a more
	 * precise estimate.
	 */
	switch(i)
	{
	    long space;
	case 0:
	    /* Warehouse index */
	    space = (sizeof(WAREHOUSE_PRIMARY_KEY) + 
			  sizeof(WAREHOUSE_PRIMARY_DATA))*scale;
	    /* Round to the page size */
	    space += DB_WH_PAGE_SIZE - (space % DB_WH_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 1:
	    /* District index */
	    space = (sizeof(DISTRICT_PRIMARY_KEY) + 
		     sizeof(DISTRICT_PRIMARY_DATA))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 2:
	    /* Customer primary and secondary indices */
	    space = (sizeof(CUSTOMER_PRIMARY_KEY) + 
		     sizeof(CUSTOMER_PRIMARY_DATA) +
		     sizeof(CUSTOMER_SECONDARY_KEY))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 3:
	    /* History index */
	    space = (sizeof(HISTORY_PRIMARY_KEY))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 4:
	    /* Order primary and secondary indices */	  
	    space = (sizeof(ORDER_PRIMARY_KEY) + 
		     sizeof(ORDER_PRIMARY_DATA) +
		     sizeof(ORDER_SECONDARY_KEY))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 5:
	    /* Neworder index */
	    space = (sizeof(NEWORDER_PRIMARY_KEY))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 6:
	    /* Orderline index */
	     space = (sizeof(DISTRICT_PRIMARY_KEY) + 
		     sizeof(DISTRICT_PRIMARY_DATA))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 7:
	    /* Item index */
	    space = (sizeof(ITEM_PRIMARY_KEY) + 
		     sizeof(ITEM_PRIMARY_DATA))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	case 8:
	    /* Stock index */
	    space = (sizeof(STOCK_PRIMARY_KEY) + 
		     sizeof(STOCK_PRIMARY_DATA))*scale;
	    /* Round to the page size */
	    space += DB_COMMON_PAGE_SIZE - (space % DB_COMMON_PAGE_SIZE);
	    total_space += space/1024;
	    break;
	default:
	    fprintf(stderr, "Should not get here.\n");
	    
	}

	k += 2;
    }
    
    printf("Total space requires is %d KB\n", total_space);
}
