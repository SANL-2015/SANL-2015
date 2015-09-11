#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define NUM_WAREHOUSES 10
#define DISTRICTS_PER_W 10

#include "tpcc_trans.h"
#include "tpcc_globals.h"

/* Names of transaction types */
char *tx_names[] = {"Mixed", "Payment", "New Order", "Order Status", 
		  "Delivery", "Stock Level"};



void 
usage(const char* prog)
{
    fprintf(stderr, "usage: %s \t -c <num_clients>\n", prog);
    fprintf(stderr, "\t\t -n <transaction type>\n");
    fprintf(stderr, "\t\t -i <num_iterations>\n");
    fprintf(stderr, "\t\t -h <home directory>\n");
    exit(1);
}

int
main(int argc, char **argv)
{
    extern char *optarg;
    extern int optind, optopt;
    int num_clients = 0, iterations = 0, type = 0;
    char *home_dir = NULL, c;
    int i, pid;
    int wh = -1, ds = 1;
    struct timeval tv;
    char *iter_buffer;

    while ((c = getopt(argc, argv, "i:n:c:h:")) != -1) 
    {
        switch(c)
        {
	case 'h': /* Get the home directory for db */
	    home_dir = optarg;
	    break;
  	case 'c':
	    num_clients = atoi(optarg);
	    break;
    	case 'n':
	    type = atoi(optarg);
	    break;
	case 'i':
	    iterations = atoi(optarg);
	    break;
	default : 
	    usage(argv[0]);
	    exit(-1);
	}
    }

    if(home_dir == NULL || iterations < 0 || num_clients < 0 ||
       type < MIXED || type > STOCKLEVEL)
    {
	usage(argv[0]);
	exit(-1);
    }

    fprintf(stdout, "Clients: %d, iterations: %d, txn type: %s.\n"
	    "Database home directory: %s\n", 
	    num_clients, iterations, tx_names[type], home_dir);
    
    if(gettimeofday(&tv, 0))
    {
	perror("gettimeofday()");
	exit(1);
    }

    srandom(tv.tv_usec);
    wh = random1(1, NUM_WAREHOUSES);

    iter_buffer = (char*)malloc(sizeof(char) * (log10(iterations) + 2));

    if(iter_buffer == NULL)
    {
	perror("malloc");
	exit(1);
    }

    for(i = 0; i<num_clients; i++)
    {

	wh = ((wh+1)% NUM_WAREHOUSES)  + 1;
	ds = ((ds+1)% DISTRICTS_PER_W) + 1;


        if ((pid = fork()) < 0) 
	{
	    fprintf(stderr, "Fork failed at i=%d: %s\n",
		    i, strerror(errno));
	    exit(-1);
        }
        if (pid == 0) 
	{
	    char wb[3];
	    char db[3];
	    char tb[2];
	    
	    sprintf(wb, "%d", wh);
	    sprintf(db, "%d", ds);
	    sprintf(tb, "%d", type);
	    sprintf(iter_buffer, "%d", iterations);

	    execlp("./tpcc_xact_server", 
		   "tpcc_xact_server", 
		   "-t", "-W", wb, "-D", db,
		   "-n", tb, "-i", iter_buffer, "-h", home_dir, 0);
	    
	    
	    fprintf(stderr, "Failed to exec at i = %d: %s\n", i, 
		    strerror(errno));
	    exit(1);
	    
        }   
    }

    free(iter_buffer);

    return 0;
}
