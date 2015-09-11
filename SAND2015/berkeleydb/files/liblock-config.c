#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <liblock.h>
#include <liblock-fatal.h>
//#include "liblock-memcached.h"

#define NUM_LOCKS 11

//#define ONE_SERVER

const  char* liblock_lock_name;
struct core** liblock_server_cores;

static int volatile go = 0;
static int volatile current_nb_threads = 0;
static int volatile wait_nb_threads = 0;
static int volatile n_available_cores = 0;
static int* client_cores;

static void do_go(); 
static void liblock_splash();
volatile int is_rcl = 0;
 
static void do_go() {
	go = 1;
}

__attribute__ ((constructor (103))) static void liblock_init() {
	int i;
    char get_cmd[128];
    char *tmp_env;
    int num_servers;

    tmp_env = getenv("NUM_SERVERS");
    if(!tmp_env)
        tmp_env = "1";
    num_servers = atoi(tmp_env);

    liblock_server_cores = malloc(NUM_LOCKS * sizeof(struct core *));

/*
    for (i = 0; i < NUM_LOCKS; i++)
    {
        if (num_servers > 6)
            printf("!!! warning: too many server cores (> 6 not supported).\n");

        liblock_server_cores[i] = topology->nodes[0].cores[i % num_servers];
    }
*/

/*
    Fast config 1:

    liblock_server_cores[0] = topology->nodes[0].cores[2];
    liblock_server_cores[1] = topology->nodes[0].cores[2];
    liblock_server_cores[2] = topology->nodes[0].cores[2];
    liblock_server_cores[3] = topology->nodes[0].cores[2];
    liblock_server_cores[4] = topology->nodes[0].cores[2];
    liblock_server_cores[5] = topology->nodes[0].cores[2];
    liblock_server_cores[6] = topology->nodes[0].cores[2];
    liblock_server_cores[7] = topology->nodes[0].cores[1];
    liblock_server_cores[8] = topology->nodes[0].cores[2];
    liblock_server_cores[9] = topology->nodes[0].cores[2];
    liblock_server_cores[10] = topology->nodes[0].cores[2];

    Fast config 2: 

    liblock_server_cores[0] = topology->nodes[0].cores[1];
    liblock_server_cores[1] = topology->nodes[0].cores[1];
    liblock_server_cores[2] = topology->nodes[0].cores[1];
    liblock_server_cores[3] = topology->nodes[0].cores[1];
    liblock_server_cores[4] = topology->nodes[0].cores[1];
    liblock_server_cores[5] = topology->nodes[0].cores[1];
    liblock_server_cores[6] = topology->nodes[0].cores[1];
    liblock_server_cores[7] = topology->nodes[0].cores[1];
    liblock_server_cores[8] = topology->nodes[0].cores[1];
    liblock_server_cores[9] = topology->nodes[0].cores[2];
    liblock_server_cores[10] = topology->nodes[0].cores[1];

    Conclusion: conflict between locks 7 and 9!
*/

#ifdef ONE_SERVER
    liblock_server_cores[0] = topology->nodes[0].cores[0];
    liblock_server_cores[1] = topology->nodes[0].cores[0];
    liblock_server_cores[2] = topology->nodes[0].cores[0];
    liblock_server_cores[3] = topology->nodes[0].cores[0];
    liblock_server_cores[4] = topology->nodes[0].cores[0];
    liblock_server_cores[5] = topology->nodes[0].cores[0];
    liblock_server_cores[6] = topology->nodes[0].cores[0];
    liblock_server_cores[7] = topology->nodes[0].cores[0];
    liblock_server_cores[8] = topology->nodes[0].cores[0];
    liblock_server_cores[9] = topology->nodes[0].cores[0];
    liblock_server_cores[10] = topology->nodes[0].cores[0];
#else

    liblock_server_cores[0] = topology->nodes[0].cores[0];
    liblock_server_cores[1] = topology->nodes[0].cores[0];
    liblock_server_cores[2] = topology->nodes[0].cores[0];
    liblock_server_cores[3] = topology->nodes[0].cores[0];
    liblock_server_cores[4] = topology->nodes[0].cores[0];
    liblock_server_cores[5] = topology->nodes[0].cores[1];
    liblock_server_cores[6] = topology->nodes[0].cores[1];
    liblock_server_cores[7] = topology->nodes[0].cores[0];
    liblock_server_cores[8] = topology->nodes[0].cores[1];
    liblock_server_cores[9] = topology->nodes[0].cores[1];
    liblock_server_cores[10] = topology->nodes[0].cores[1];
#endif

	liblock_lock_name = getenv("LIBLOCK_LOCK_NAME");
	if(!liblock_lock_name)
		liblock_lock_name = "rcl";

	is_rcl = !strcmp(liblock_lock_name, "rcl") ||
             !strcmp(liblock_lock_name, "multircl");

	liblock_start_server_threads_by_hand = 1;
	liblock_servers_always_up = 1;

	sprintf(get_cmd, "/proc/%d/cmdline", getpid());
	FILE* f=fopen(get_cmd, "r");
	if(!f) {
		printf("!!! warning: unable to find command line\n");
	}
	char buf[1024];
	buf[0] = 0;
	if(!fgets(buf, 1024, f))
		printf("fgets\n");

	printf("**** testing %s with lock %s\n", buf,
           liblock_lock_name);

    /* Pre-bind */
    cpu_set_t    cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(topology->nodes[0].cores[2]->core_id, &cpuset);
    if(sched_setaffinity(0, sizeof(cpu_set_t), &cpuset))   
        fatal("pthread_setaffinity_np");
    /* /Pre-bind */

	if(is_rcl) {
		go = 0;

        liblock_reserve_core_for(topology->nodes[0].cores[0], liblock_lock_name);
#ifndef ONE_SERVER
        liblock_reserve_core_for(topology->nodes[0].cores[1], liblock_lock_name);
#endif
/*
        for (i = 0; i < NUM_LOCKS; i++)
	      liblock_reserve_core_for(liblock_server_cores[i], liblock_lock_name);
*/
      
        /* launch the liblock threads */
		liblock_lookup(liblock_lock_name)->run(do_go); 
        
        while(!go)
			PAUSE();
	}

	client_cores = malloc(sizeof(int)*topology->nb_cores);

	int j, k, z;
	for(i=0, z=0; i<topology->nb_nodes; i++) {
		for(j=0; j<topology->nodes[i].nb_cores; j++)
        {
            int is_server_core = 0;
            
            if (is_rcl)
            {
                for (k = 0; k < NUM_LOCKS; k++)
			        if(topology->nodes[i].cores[j] == liblock_server_cores[k])
                        is_server_core = 1;
            }

            if (!is_server_core)
				client_cores[z++] = topology->nodes[i].cores[j]->core_id;
        }
    }

    n_available_cores = z;

    printf("**** %d available cores for clients.\n", z);

	liblock_auto_bind();
}

void liblock_auto_bind() {
	if(!self.running_core) {

		static int cur_core = 0;

		struct core *core;

		do {
			int n = __sync_fetch_and_add(&cur_core, 1) % n_available_cores;
			core = &topology->cores[client_cores[n]];
		} while(core->server_type);

		self.running_core = core;


		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		CPU_SET(core->core_id, &cpuset);


/*        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        
        int i, j;

      	for(i=0; i<topology->nb_nodes; i++)
        {
		    for(j=0; j<topology->nodes[i].nb_cores; j++)
            {
#ifndef ONE_SERVER

                if (is_rcl && (i == 0 && (j == 0 || j == 1))) // Server cores
#else
                if (is_rcl && (i == 0 && j == 0)) // Server cores
#endif
                    CPU_CLR(topology->nodes[i].cores[j]->core_id, &cpuset);
                else
                    CPU_SET(topology->nodes[i].cores[j]->core_id, &cpuset);
            }
        }

*/
		if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset))
            fatal("pthread_setaffinity_np");

		__sync_fetch_and_add(&current_nb_threads, 1);

		printf("autobind thread %d to core %d/%d (%d threads out of %d)\n",
               self.id, sched_getcpu(), self.running_core->core_id,
               current_nb_threads, wait_nb_threads);
	}
}

