/* ########################################################################## */
/* benchmark.c                                                                */
/* (C) Jean-Pierre Lozi, 2010-2011                                            */
/* (C) Gaël Thomas, 2010-2011                                                 */
/* -------------------------------------------------------------------------- */
/* This program should be compiled with the -O0 compiler flag.                */
/* ########################################################################## */

/* ########################################################################## */
/* Headers                                                                    */
/* ########################################################################## */
#include <math.h>
#include <numa.h>
#include <getopt.h>
#include <papi.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "mcs_lock.h"

#include "liblock-fatal.h"
#include "liblock.h"

/* ########################################################################## */
/* Definitions                                                                */
/* ########################################################################## */
/*
 *** DEPRECATED ***

 Use blocking locks?

 This macro is experimental. When it is defined, the benchmark uses blocking
 locks. Only certain configurations are supported, however (no sampling,
 for instance).
 */
/* #define USE_BLOCKING_LOCKS */

/* Default values for the command-line arguments. */
#define DEFAULT_NUMBER_OF_RUNS 1
#define DEFAULT_NUMBER_OF_ITERATIONS_PER_SAMPLE 10
#define DEFAULT_SERVER_CORE 0
#define DEFAULT_NUMBER_OF_ITERATIONS_PER_CLIENT 1000
#define DEFAULT_NUMBER_OF_CONTEXT_VARIABLES 0
#define DEFAULT_NUMBER_OF_SHARED_VARIABLES 0

/* Maximum number of cores, used to avoid malloc/realloc cycles. */
#define MAX_NUMBER_OF_CORES 1024
#define NUMBER_OF_CORES_PER_DIE 6

/* Maximum line size used when reading results from unix commands. */
#define MAX_LINE_SIZE 32

/* Size of cache lines. */
//#define CACHE_LINE_SIZE 64
/* PAUSE instructions for spin locks */
//#define PAUSE() __asm__ __volatile__ ("rep; nop" : : );
//#define PAUSE() __asm__ __volatile__ ("pause\n" : : );
// already defined in liblock.h
/* This macro performs the conversion between cycles and throughput. */
#define TO_THROUGHPUT(x) (g_cpu_frequency * 1000000 / (x))

/* ########################################################################## */
/* Global variables                                                           */
/* -------------------------------------------------------------------------- */
/* FIXME: some volatile keywords may not be necessary. We declare all global  */
/* variables as volatile for now.                                             */
/* ########################################################################## */
/* Environment data ========================================================= */
/* Number of cores */
volatile int g_number_of_cores;
/* CPU frequency, in MHz */
volatile float g_cpu_frequency;
/* This array maps physical to virtual core IDs. */
volatile int *g_physical_to_virtual_core_id;
/* Has the server started? */
volatile int g_server_started;

/* Execution parameters ===================================================== */
/*
 Critical sections
 */
/* Critical sections can either be null RPCs (serviced by a single server) or
 lock acquisitions. */
typedef enum _critical_sections_type {
	CST_NULL_RPCS, CST_LOCK_ACQUISITIONS, CST_LIBLOCK
} critical_sections_type_t;
volatile critical_sections_type_t g_critical_sections_type;
/* In the case of lock acquisitions, shall we use regular spinlocks or MCS? */
volatile int g_use_mcs;

/*
 Execution mode
 */
/* Three execution modes are available :
 - MULTIPLE_RUNS_AVERAGED means that the experiment is run multiple times and
 the averaged results are returned (along with the variance and standard
 deviation).
 - SINGLE_RUN_SAMPLED means that the experiment is only run once, and
 statistics are gathered during the execution.
 - SINGLE_RUN_ORDERED means that the experiment is only run once and that the
 order in which clients had their critical section executed is returned.
 */
typedef enum _execution_mode_t {
	EM_MULTIPLE_RUNS_AVERAGED, EM_SINGLE_RUN_SAMPLED, EM_SINGLE_RUN_ORDERED
} execution_mode_t;
volatile execution_mode_t g_execution_mode;
/* Number of runs over which the results are averaged (used in the
 MULTIPLE_RUNS_AVERAGED mode only). */
volatile int g_number_of_runs;
/* Number of iterations per sample (used in the SINGLE_RUN_SAMPLED only). */
volatile int g_number_of_iterations_per_sample;

/*
 Execution settings
 */
/* Core on which the server runs */
volatile int g_server_core;
/* Number of clients */
volatile int g_number_of_clients;
/* Number of iterations per client */
volatile int g_number_of_iterations_per_client;
/* Delay between RPCs, in cycles. */
volatile int g_delay;
/* ROW mode activated? */
volatile int g_read_only_wait;
/* Number of context variables */
volatile int g_number_of_context_variables;
/* Number of shared variables */
volatile int g_number_of_shared_variables;
/* shared area */
volatile uint64_t *g_shared_variables_memory_area;
/* Use one lock per client in RPC mode? */
volatile int g_use_multiple_locks;
/* Be NUMA-aware? */
volatile int g_numa_aware;
/* Skip measurements for the first critical section? */
volatile int g_skip_first_cs;
/* Number of clients done executing critical sections. */
volatile int g_number_of_finished_clients = 0;
// By zmz
/* time of waste loop */
volatile int g_wat;
//~By zmz

/*
 Measurements
 */
/* Shall we count cycles, events or failed attempts? */
typedef enum _measurement_metric {
	MM_NUMBER_OF_CYCLES, MM_NUMBER_OF_EVENTS, MM_FAILED_ATTEMPTS
} measurement_metric_t;
volatile measurement_metric_t g_measurement_metric;
/* Shall we measure events/cycles globally (around the experiment, divided by
 * the number of iterations), around the lock acquisition, or around the
 * critical section (including the lock/unlock functions)? This parameter is
 * ignored if the metric is MM_FAILED_ATTEMPTS. */
typedef enum _measurement_type {
	MT_GLOBAL, MT_LOCK_ACQUISITIONS, MT_CRITICAL_SECTIONS
} measurement_type_t;
volatile measurement_type_t g_measurement_type;
/* ID of the monitored PAPI event if measurement_type = NUMBER_OF_EVENTS */
volatile int g_monitored_event_id;
/* Shall we perform the measurements on the server or the clients? */
typedef enum _measurement_location {
	ML_SERVER, ML_CLIENTS
} measurement_location_t;
volatile measurement_location_t g_measurement_location;
/* Should the results be averaged across clients? */
volatile int g_average_client_results;
/* Shall we return the result in throughput or cycles? */
/* FIXME: doesn't make much sense. Should be split. */
typedef enum _measurement_unit {
	MU_THROUGHPUT, MU_CYCLES_PER_ITERATION, MU_TOTAL_CYCLES_MAX /* FIXME: not a unit. */
} measurement_unit_t;
volatile measurement_unit_t g_measurement_unit;
/* Order in which global and local variables are accessed. */
typedef enum _access_order {
	AO_SEQUENTIAL, AO_DEPENDENT, AO_RANDOM, AO_CUSTOM_RANDOM, AO_PERMUTATIONS
} access_order_t;
/* Randomized cache line accesses? */
volatile access_order_t g_access_order;

/*
 Output settings
 */
volatile int g_compute_standard_deviation_and_variance;
/* End output with a newline? */
volatile int g_end_output_with_a_newline;
/* Verbose output? */
volatile int g_verbose;

/* Execution variables ====================================================== */
/* Adresses of the rpc_done addresses for each thread */
volatile void ** volatile g_rpc_done_addresses;
/* Synchronization variables for the barrier between the server and the
 clients, before the main loop */
volatile int * volatile g_ready;

volatile unsigned long long g_event_count;

/* Data specific to the MULTIPLE_RUNS_AVERAGED mode */
/* Results (cycles or number of events) for each iteration */
volatile double * volatile g_iteration_result;

/* Data specific to the SINGLE_RUN_SAMPLED mode */
/* Number of iterations per sample */
volatile float * volatile g_multiple_samples_results;
/* Address of the rpc_done variable from the core whose RPC was serviced last.
 Translated into the core ID at the end of the computation. */
volatile void ** volatile g_multiple_samples_rpc_done_addrs;

/* Data specific to the SINGLE_RUN_ORDERED mode */
/* Order in which the RPCs are processed */
volatile int * volatile g_order;
/* FIXME: should be merged with g_iteration_result? */
volatile double * volatile g_latencies;

/* FIXME: rename */
/* Permutation arrays for global variables */
volatile int *g_global_permutations_array;
/* Permutation arrays for local variables */
volatile int *g_local_permutations_array;

/* MCS lock head */
mcs_lock *g_mcs_m_ptr;

/* Mutexes and conditions used by non-blocking locks ======================== */
#ifdef USE_BLOCKING_LOCKS
pthread_mutex_t mutex_rpc_done_addr_not_null;
pthread_cond_t cond_rpc_done_addr_not_null;
pthread_mutex_t mutex_rpc_done_positive;
pthread_cond_t cond_rpc_done_positive;
pthread_mutex_t mutex_rpc_done_addr_null;
pthread_cond_t cond_rpc_done_addr_null;
#endif

/* Command-line options ===================================================== */
static struct option long_options[] = {
		{ "cst_null_rpcs", no_argument, 0, 'R' }, { "cst_lock_acquisitions",
				no_argument, 0, 'L' }, { "cst_lock_acquisitions_mcs",
				no_argument, 0, 'M' }, { "cst_liblock", required_argument, 0,
				'F' },
		{ "em_multiple_runs_averaged", required_argument, 0, 'A' }, {
				"em_single_runs_sampled", required_argument, 0, 'S' }, {
				"em_single_run_ordered", no_argument, 0, 'O' }, { "server_core",
				required_argument, 0, 's' }, { "n_cores", required_argument, 0,
				'k' }, { "n_clients", required_argument, 0, 'c' }, {
				"n_iterations_per_client", required_argument, 0, 'n' }, {
				"delay", required_argument, 0, 'd' }, { "read_only_wait",
				no_argument, 0, 'r' }, { "n_context_variables",
				required_argument, 0, 'l' }, { "n_shared_variables",
				required_argument, 0, 'g' }, { "access_order",
				required_argument, 0, 'x' }, { "use_multiple_locks",
				no_argument, 0, 'o' }, { "numa_aware", no_argument, 0, 'N' }, {
				"skip_first_cs", no_argument, 0, 'w' }, { "mm_count_event",
				required_argument, 0, 'e' }, { "mm_failed_attempts",
				no_argument, 0, 'f' }, { "mt_lock_acquisitions", no_argument, 0,
				'a' }, { "mt_critical_sections", no_argument, 0, 'u' }, {
				"ml_clients", no_argument, 0, 'm' }, { "average_client_results",
				no_argument, 0, '1' }, { "mu_cycles_per_iteration", no_argument,
				0, 'y' }, { "mu_total_cycles_max", no_argument, 0, 't' },
		{ "compute_standard_deviation_and_variance", no_argument, 0, 'p' }, {
				"end_output_without_a_newline", no_argument, 0, 'i' }, {
				"verbose", no_argument, 0, 'v' },
		{ "help", no_argument, 0, 'h' }, { 0, 0, 0, 0 } };

/* ########################################################################## */
/* Type definitions                                                           */
/* ########################################################################## */
/* This structure models a cache line. It should be allocated on a 64-byte
 boundary. */
typedef char cache_line_t[CACHE_LINE_SIZE];

/*
 This structure models the data passed to threads upon their creation.
 Variables for both the 'NULL RPCs' and 'lock acquisitions' modes are passed
 in order to simplify the code.
 */
typedef struct _thread_arguments_block_t {
	/* Logical thread id (local, not provided by pthreads). */
	int id;

	/* Physical core id. */
	int client_core;

	/* 'NULL RPCs' mode synchronization variables =========================== */
	/*
	 Global synchronization variable used to limit the number of NULL RPCs
	 serviced concurrently to 1. Its value can either be:
	 - 0 if no RPC is requested.
	 - The address to a client-bound variable that shall be set to 1 upon
	 completion of the RPC otherwise.
	 */
	volatile uint64_t * volatile *null_rpc_global_sv;
	/*
	 Local synchronization variable set to a >= 0 value by the server to
	 signal the completion of a RPC.
	 */
	volatile uint64_t *null_rpc_local_sv;

	/* 'Lock acquisitions' mode synchronization variables =================== */
	/*
	 Global lock repeatedly acquired by all clients.
	 */
	volatile uint64_t *lock_acquisitions_global_sv;

	volatile uint64_t *context_variables_global_memory_area;
	volatile uint64_t *shared_variables_memory_area;
} thread_arguments_block_t;

/* ########################################################################## */
/* Prototypes                                                                 */
/* ########################################################################## */
void *client_main(void *thread_arguments_block);
void *server_main(void *thread_arguments_block);

static struct core* get_core(unsigned int physical_core);

inline void access_variables(volatile uint64_t *memory_area,
		int first_variable_number, int number_of_variables,
		int randomized_accesses, int *permutations_array);
inline int _rand(int next);
void generate_permutations_array(volatile int **permutations_array, int size);
void *alloc(int size);
void get_cpu_info();

void wrong_parameters_error(char *application_name);

pthread_barrier_t barrier;

/* ########################################################################## */
/* Critical section (liblock)                                                 */
/* ########################################################################## */
static const char* g_liblock_name = 0;
static liblock_lock_t g_liblock_lock;

__thread volatile uint64_t *t_shared_variables_memory_area;
__thread int t_number_of_context_variables;
__thread int t_number_of_shared_variables;
__thread int t_access_order;
__thread int *t_local_permutations_array;
__thread int *t_global_permutations_array;

void* cs(void *context_variables_local_memory_area) {
	/* We access context variables */
	access_variables(context_variables_local_memory_area, 0,
			g_number_of_context_variables, t_access_order,
			t_local_permutations_array);

	/* We access shared variables */
	access_variables(g_shared_variables_memory_area, 0,
			g_number_of_shared_variables, t_access_order,
			t_global_permutations_array);
	return 0;
}

void server_started() {
	g_server_started = 1;
}

/* ########################################################################## */
/* Functions                                                                  */
/* ########################################################################## */
/* Main function */
int main(int argc, char **argv) {
	int i, j;
	int command;
	int result;
	double *results;
	double *average, *variance;
	/* FIXME: use global variables instead */
	int number_of_samples = 0;

	/* We use both a server and number_of_clients clients. The clients call
	 remote procedures from the server. */
	//    pthread_t server;
	pthread_t *clients;

	/* This array contains the argument blocks passed to the clients. */
	thread_arguments_block_t *svs_memory_area, *thread_argument_blocks,
			*context_variables_global_memory_area,
			*shared_variables_memory_area;

	/* We initialize the random number generator. */
	srand(time(NULL));

	/* We get the clock speed and the core ordering. */
	get_cpu_info();

	/*
	 Command-line arguments

	 Critical sections
	 =================

	 -R
	 Perform null RPCs. This is the default behavior.

	 -L
	 Perform lock acquisitions instead of null RPCs. In this mode, there is
	 no server.

	 If both -R and -L are specified, the last one wins.

	 -M
	 Perform lock acquisitions using the MCS lock.

	 Execution mode
	 ==============

	 -A number_of_runs
	 Return the average number of cycles, with variance and standard
	 deviation, over the given number of runs. This is the default mode.

	 -S number_of_iterations_per_sample
	 Return sampled results over a single run, using the given number of
	 iterations per sample.

	 -O
	 In this mode, each client enters its critical section only once, and the
	 order in which the clients were serviced is returned. Only one run is
	 performed and the results are not averaged.

	 If several execution modes are specified, the last one wins.

	 Execution settings
	 ==================

	 -s core
	 Core on which the server thread runs if the critical section consists of
	 performing null RPCs, or core of the first client if the critical section
	 consists of lock acquisitions.

	 -c number_of_clients
	 Number of clients.

	 -n number_of_iterations_per_client
	 Number of RPCs/lock acquisitions per client.

	 -d delay
	 Number of cycles wasted (busy waiting) by clients after a RPC is
	 serviced. If 0, no cycles are wasted (apart from the execution time of a
	 comparison), otherwise the accuracy of the delay is within 50~150 cycles.

	 -r
	 Read-only wait mode : wait using comparisons, and only perform a CAS
	 after a successful comparison (test-and-test-and-set).

	 -l number_of_context_variables
	 Number of context (local) variables.

	 -g number_of_shared_variables
	 Number of shared (global) variables.

	 -x access_type
	 Change the access type
	 - access_type = dependent
	 Dependent cache line accesses.
	 - access_type = custom_random
	 Randomized cache line accesses (using glibc's rand()).
	 - access_type = random
	 Randomized cache line accesses (custom rand() fnction).
	 - access_type = permutations
	 Randomized cache line accesses with permutations.

	 -w
	 Don't count the result for the first critical section.

	 Measurements
	 ============

	 -e event_type
	 Count events of type event_type instead of measuring the elapsed time.
	 Event_type is the type of a PAPI event.

	 -a
	 Measure latencies instead of elapsed time.

	 -u
	 Measure full latencies instead of elapsed time.

	 -f
	 Count number of failed attempts to get the global lock.

	 -m
	 Count cycles (or events) on the clients instead of the server. This is
	 always the case if -L is on.

	 -1
	 Display a single result: the average of the results across clients.

	 -y
	 Returns results in cycles per iteration instead of the number of
	 iterations per second.

	 -t
	 Return results in total cycles instead of the number of iterations per
	 second. Only the maximum value is returned.

	 Output
	 ======

	 -p
	 Return the standard deviation and variance if applicable.

	 -i
	 End the results with a newline. This is left as an option, to allow for
	 more flexibility with the CSV results.
	 */

	opterr = 0;

	/* Default values */
	g_critical_sections_type = CST_NULL_RPCS;
	g_use_mcs = 0;

	g_execution_mode = EM_MULTIPLE_RUNS_AVERAGED;
	g_number_of_runs = DEFAULT_NUMBER_OF_RUNS;
	g_number_of_iterations_per_sample = DEFAULT_NUMBER_OF_ITERATIONS_PER_SAMPLE;

	g_server_started = 1;

	g_server_core = DEFAULT_SERVER_CORE;
	g_number_of_clients = -1;
	g_number_of_iterations_per_client = DEFAULT_NUMBER_OF_ITERATIONS_PER_CLIENT;
	g_delay = 0;
	g_read_only_wait = 0;

	g_number_of_context_variables = DEFAULT_NUMBER_OF_CONTEXT_VARIABLES;
	g_number_of_shared_variables = DEFAULT_NUMBER_OF_SHARED_VARIABLES;
	g_access_order = AO_SEQUENTIAL;
	g_use_multiple_locks = 0;
	g_numa_aware = 0;
	g_skip_first_cs = 0;

	g_measurement_metric = MM_NUMBER_OF_CYCLES;
	g_measurement_type = MT_GLOBAL;
	g_measurement_location = ML_SERVER;
	g_average_client_results = 0;
	g_measurement_unit = MU_THROUGHPUT;

	g_compute_standard_deviation_and_variance = 0;
	g_end_output_with_a_newline = 1;
	g_verbose = 0;

	/* FIXME: use long commands */
	while ((command = getopt_long(argc, argv,
			"RLMF:A:S:Os:c:n:d:rl:g:x:W:oNwtTe:m1yaufpivh", long_options, NULL))
			!= -1) {
		switch (command) {
		/*
		 Critical sections
		 */
		case 'R':
			/* Actually useless (default value). */
			g_critical_sections_type = CST_NULL_RPCS;
			break;
		case 'L':
			g_critical_sections_type = CST_LOCK_ACQUISITIONS;
			break;
		case 'M':
			g_critical_sections_type = CST_LOCK_ACQUISITIONS;
			g_use_mcs = 1;
			break;
		case 'F':
			g_critical_sections_type = CST_LIBLOCK;
			g_server_started = 0;
			liblock_start_server_threads_by_hand = 1;
			liblock_servers_always_up = 0;
			g_liblock_name = optarg;
			break;

			/*
			 Execution mode
			 */
		case 'A':
			g_execution_mode = EM_MULTIPLE_RUNS_AVERAGED;
			g_number_of_runs = atoi(optarg);
			break;

		case 'S':
			g_execution_mode = EM_SINGLE_RUN_SAMPLED;
			g_number_of_iterations_per_sample = atoi(optarg);
			break;

		case 'O':
			g_execution_mode = EM_SINGLE_RUN_ORDERED;
			break;

			/*
			 Execution settings
			 */
		case 's':
			g_server_core = atoi(optarg);
			break;

		case 'k':
			g_number_of_cores = atoi(optarg);
			break;

		case 'c':
			g_number_of_clients = atoi(optarg);
			break;

		case 'n':
			g_number_of_iterations_per_client = atoi(optarg);
			break;

		case 'd':
			g_delay = atoi(optarg);
			break;

		case 'r':
			g_read_only_wait = 1;
			break;

		case 'l':
			g_number_of_context_variables = atoi(optarg);
			break;

		case 'g':
			g_number_of_shared_variables = atoi(optarg);
			break;

		case 'x':
			if (!strcmp(optarg, "random"))
				g_access_order = AO_RANDOM;
			else if (!strcmp(optarg, "custom_random"))
				g_access_order = AO_CUSTOM_RANDOM;
			else if (!strcmp(optarg, "permutations"))
				g_access_order = AO_PERMUTATIONS;
			else if (!strcmp(optarg, "dependent"))
				g_access_order = AO_DEPENDENT;
			else
				fatal("invalid access order");
			break;

		case 'W':
			g_wat = atoi(optarg);
			break;

		case 'o':
			g_use_multiple_locks = 1;
			break;

		case 'N':
			g_numa_aware = 1;
			break;

		case 'w':
			g_skip_first_cs = 1;
			break;

			/*
			 Measurements
			 */
		case 'e':
			g_measurement_metric = MM_NUMBER_OF_EVENTS;
			g_monitored_event_id = strtol(optarg, NULL, 16);
			break;

		case 'f':
			g_measurement_type = MM_FAILED_ATTEMPTS;
			break;

		case 'a':
			g_measurement_type = MT_LOCK_ACQUISITIONS;
			break;

		case 'u':
			g_measurement_type = MT_CRITICAL_SECTIONS;
			break;

		case 'm':
			g_measurement_location = ML_CLIENTS;
			break;

		case '1':
			g_average_client_results = 1;
			break;

		case 'y':
			g_measurement_unit = MU_CYCLES_PER_ITERATION;
			break;

		case 't':
			g_measurement_unit = MU_TOTAL_CYCLES_MAX;
			break;

			/*
			 Output
			 */
		case 'p':
			g_compute_standard_deviation_and_variance = 1;
			break;

		case 'i':
			g_end_output_with_a_newline = 0;
			break;

		case 'v':
			g_verbose = 1;
			break;

			/*
			 Other
			 */
		case 'h':
			wrong_parameters_error(argv[0]);
		default:
			fatal("getopt");
		}
	}

	/* Default values for the number of clients */
	if (g_number_of_clients <= 0)
		g_number_of_clients = g_number_of_cores - 1;

	if (g_critical_sections_type == CST_LOCK_ACQUISITIONS) {
		/* Lock acquisitions are only compatible with the MULTIPLE_RUNS_AVERAGED
		 execution mode. */
		if (g_execution_mode != EM_MULTIPLE_RUNS_AVERAGED)
			fatal("configuration not supported");

		/* In this mode, measurements take place on the clients. */
		g_measurement_location = ML_CLIENTS;
	}

	if (g_execution_mode == EM_SINGLE_RUN_SAMPLED) {
		/* For now, the SINGLE_RUN_SAMPLED execution mode in incompatible with
		 blocking locks. */
#ifdef USE_BLOCKING_LOCKS
		fatal("configuration not supported");
#endif

		/* For now, in the SINGLE_RUN_SAMPLED mode, events can't be monitored.
		 Also, all measurements must take place on the server. */
		if (g_measurement_metric != MM_NUMBER_OF_CYCLES
				&& g_measurement_location != ML_SERVER)
			fatal("configuration not supported");

		/* Single run */
		g_number_of_runs = 1;

		if (g_number_of_iterations_per_sample
				> g_number_of_iterations_per_client * g_number_of_clients)
			fatal("too many iterations per sample");
	}

	/* In the SINGLE_RUN_ORDERED execution mode... */
	if (g_execution_mode == EM_SINGLE_RUN_ORDERED) {
		/* ...there's just one iteration per client... */
		g_number_of_iterations_per_client = 1;

		/* ...and a single run. */
		g_number_of_runs = 1;
	}

	/*
	 if (g_measurement_unit == MU_THROUGHPUT &&
	 g_measurement_metric != MM_NUMBER_OF_CYCLES &&
	 g_measurement_type != MT_GLOBAL)
	 {
	 fatal("throughput can only be measured for global number of cycles");
	 }
	 */

	/* We need the maximum priority (for performance measurements). */
	if (setpriority(PRIO_PROCESS, 0, -20) < 0)
		fatal("setpriority");

	generate_permutations_array(&g_global_permutations_array,
			g_number_of_shared_variables);

	generate_permutations_array(&g_local_permutations_array,
			g_number_of_context_variables);

	liblock_bind_thread(pthread_self(), get_core(g_server_core),
			g_liblock_name);
	liblock_define_core(get_core(g_server_core));

	if (g_critical_sections_type == CST_LIBLOCK) {
		liblock_lock_init(g_liblock_name, self.running_core, &g_liblock_lock,
				0);
	}

	// printf("binding main to %d\n",
	//        g_physical_to_virtual_core_id[g_server_core]);

	/* ====================================================================== */
	/* [v] Memory allocations                                                 */
	/* ====================================================================== */
	/* The following arrays' sizes depend on the parameters, we need to allocate
	 them dynamically. */
	clients = alloc(g_number_of_clients * sizeof(pthread_t));
	thread_argument_blocks = alloc(
			(g_number_of_clients + 1) * sizeof(thread_arguments_block_t));
	g_ready = alloc((g_number_of_clients + 1) * sizeof(int));
	g_iteration_result = alloc((g_number_of_clients + 1) * sizeof(double));
	results = alloc(
			(g_number_of_clients + 1) * (g_number_of_runs * sizeof(double)));
	average = alloc((g_number_of_clients + 1) * sizeof(double));
	variance = alloc((g_number_of_clients + 1) * sizeof(double));

	/* If we need to return the number in which RPCs are serviced... */
	if (g_execution_mode == EM_SINGLE_RUN_ORDERED) {
		/* ...we allocate the 'order' array dynamically. */
		g_order = alloc(g_number_of_clients * sizeof(double));

		// latencies = alloc(number_of_clients * sizeof(double));
	}

	/* FIXME: float? */
	g_latencies = alloc(g_number_of_clients * sizeof(double));
	for (i = 0; i < g_number_of_clients; i++)
		g_latencies[i] = -1;

	/* We always allocate the sampling-related arrays. */
	g_rpc_done_addresses = alloc((g_number_of_clients + 1) * sizeof(void *));
	number_of_samples = g_number_of_iterations_per_client * g_number_of_clients
			/ g_number_of_iterations_per_sample;

	/* FIXME: float? */
	g_multiple_samples_results = alloc(number_of_samples * sizeof(double));

	g_multiple_samples_rpc_done_addrs = alloc(
			number_of_samples * sizeof(void *));

	if (g_critical_sections_type != CST_NULL_RPCS || !g_numa_aware) {
		/* Memory area containing the synchronization variables. Each synchroni-
		 zation variable is allocated on its own pair of cache lines. */
		result = posix_memalign((void **) &svs_memory_area, 2 * CACHE_LINE_SIZE,
				(g_number_of_clients + 1) * 2 * sizeof(cache_line_t));

		if (result < 0 || svs_memory_area == NULL)
			fatal("memalign");
	} else {
		svs_memory_area = numa_alloc_local(
				(g_number_of_clients + 1) * 2 * sizeof(cache_line_t));
		// svs_memory_area = mmap(0, (g_number_of_clients + 1) * 2 *
		//                        sizeof(cache_line_t), PROT_READ | PROT_WRITE,
		//                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);

		if (svs_memory_area == NULL)
			fatal("numa_alloc_local");
		// fatal("mmap");
	}

	memset(svs_memory_area, 0,
			(g_number_of_clients + 1) * 2 * sizeof(cache_line_t));

	/* FIXME: allocate on clients? might be more realistic */
	result = posix_memalign((void **) &context_variables_global_memory_area,
			2 * CACHE_LINE_SIZE,
			(g_number_of_clients + 1) * (g_number_of_context_variables + 1) * 2
					* sizeof(cache_line_t));

	if (result < 0 || context_variables_global_memory_area == NULL)
		fatal("memalign");

	memset(context_variables_global_memory_area, 0,
			(g_number_of_clients + 1) * (g_number_of_context_variables + 1) * 2
					* sizeof(cache_line_t));

	if (g_critical_sections_type != CST_NULL_RPCS || !g_numa_aware) {
		result = posix_memalign((void **) &shared_variables_memory_area,
				2 * CACHE_LINE_SIZE/*128 1024*/,
				(g_number_of_shared_variables) * 2 * sizeof(cache_line_t));

		if (result < 0 || shared_variables_memory_area == NULL)
			fatal("memalign");
	} else {
		shared_variables_memory_area = numa_alloc_local(
				(g_number_of_shared_variables) * 2 * sizeof(cache_line_t));
		// shared_variables_memory_area = mmap(0, g_number_of_shared_variables *
		//                                     2 * sizeof(cache_line_t),
		//                                     PROT_READ | PROT_WRITE,
		//                                     MAP_ANONYMOUS | MAP_PRIVATE,
		//                                     0, 0);

		if (svs_memory_area == NULL)
			fatal("numa_alloc_local");
		// fatal("mmap");
	}

	g_shared_variables_memory_area =
			(volatile uint64_t *) shared_variables_memory_area;
	memset(shared_variables_memory_area, 0,
			(g_number_of_shared_variables) * 2 * sizeof(cache_line_t));

	/* MCS Lock */
	result = posix_memalign((void **) &g_mcs_m_ptr, 2 * CACHE_LINE_SIZE,
			2 * CACHE_LINE_SIZE);

	*g_mcs_m_ptr = 0;

	if (result < 0 || g_mcs_m_ptr == NULL)
		fatal("memalign");

	/* ====================================================================== */
	/* [^] Memory allocations                                                 */
	/* ====================================================================== */

	/* We initialize PAPI. */
	if (PAPI_is_initialized() == PAPI_NOT_INITED
			&& PAPI_library_init(PAPI_VER_CURRENT) != PAPI_VER_CURRENT)
		fatal("PAPI_library_init");

	/* If we use blocking locks, then some mutexes and conditions should be
	 initialized. */
#ifdef USE_BLOCKING_LOCKS
	pthread_mutex_init(&mutex_rpc_done_addr_not_null, NULL);
	pthread_mutex_init(&mutex_rpc_done_positive, NULL);
	pthread_mutex_init(&mutex_rpc_done_addr_null, NULL);

	if (pthread_cond_init(&cond_rpc_done_addr_not_null, NULL) < 0
			|| pthread_cond_init(&cond_rpc_done_positive, NULL) < 0
			|| pthread_cond_init(&cond_rpc_done_addr_null, NULL) < 0)
	fatal("pthread_cond_init");
#endif

	if (g_critical_sections_type == CST_NULL_RPCS)
		pthread_barrier_init(&barrier, NULL, 48);
	else
		pthread_barrier_init(&barrier, NULL, 47);

	/* We iterate to average the results. */
	for (i = 0; i < g_number_of_runs; i++) {
		/* No rpc started for now. */
		*((uint64_t **) svs_memory_area) = 0;

		/* None of the clients are ready. */
		for (j = 0; j <= g_number_of_clients; j++)
			g_ready[j] = 0;

		/* Initialisations related to sampling. */
		if (g_execution_mode == EM_SINGLE_RUN_SAMPLED) {
			for (j = 0; j < number_of_samples; j++)
				g_multiple_samples_results[j] = 0;

			for (j = 0; j < number_of_samples; j++)
				g_multiple_samples_rpc_done_addrs[j] = 0;
		}

		/* We create the client threads. */
		for (j = 0; j < g_number_of_clients; j++) {
			thread_argument_blocks[j + 1].id = j;
			thread_argument_blocks[j + 1].client_core = (g_server_core + 1
					+ (j % (g_number_of_cores - 1))) % g_number_of_cores;

			thread_argument_blocks[j + 1].null_rpc_global_sv =
					(volatile uint64_t * volatile *) (&svs_memory_area[0]);
			if (g_use_multiple_locks) {
				thread_argument_blocks[j + 1].null_rpc_local_sv =
						(volatile uint64_t *) ((uintptr_t) svs_memory_area
								+ (j + 1) * 2 * sizeof(cache_line_t));
			} else {
				thread_argument_blocks[j + 1].null_rpc_local_sv =
						(volatile uint64_t *) ((uintptr_t) context_variables_global_memory_area
								+ j * (g_number_of_context_variables + 1) * 2
										* sizeof(cache_line_t));
			}

			thread_argument_blocks[j + 1].lock_acquisitions_global_sv =
					(volatile uint64_t *) (&svs_memory_area[0]);

			thread_argument_blocks[j + 1].context_variables_global_memory_area =
					(volatile uint64_t *) ((uintptr_t) context_variables_global_memory_area);
			thread_argument_blocks[j + 1].shared_variables_memory_area =
					(volatile uint64_t *) (shared_variables_memory_area);

			/* We send i directly instead of sending a pointer to avoid having
			 to create a variable per thread containing the value. */
			liblock_thread_create_and_bind(
					get_core(thread_argument_blocks[j + 1].client_core), 0,
					&clients[j], NULL, client_main,
					&thread_argument_blocks[j + 1]);
		}

		/* If we perform null RPCs in the critical sections... */
		if (g_critical_sections_type == CST_NULL_RPCS) {
			thread_argument_blocks[0].id = 0;
			thread_argument_blocks[0].null_rpc_global_sv =
					(volatile uint64_t * volatile *) (&svs_memory_area[0]);
			thread_argument_blocks[0].null_rpc_local_sv = NULL;

			/* FIXME: global variable? */
			thread_argument_blocks[0].context_variables_global_memory_area =
					(volatile uint64_t *) (context_variables_global_memory_area);
			thread_argument_blocks[0].shared_variables_memory_area =
					(volatile uint64_t *) (shared_variables_memory_area);

			/* ...we create a server thread. */
			// liblock_thread_create(&server, NULL, server_main,
			//                       &thread_argument_blocks[0])
			server_main((void *) &thread_argument_blocks[0]);
		}

		/* Launch the liblock threads, if needed. */
		// By zmz
		if (g_critical_sections_type == CST_LIBLOCK
				&& strcmp(g_liblock_name, "saml") == 0) {
			//printf("lookup for the '%s' locking library\n", g_liblock_name);
			g_server_started = 1;
		}
		//~By zmz
		else if (g_critical_sections_type == CST_LIBLOCK)
			liblock_lookup(g_liblock_name)->run(server_started);

		/* We wait for the clients to finish their work. */
		for (j = 0; j < g_number_of_clients; j++) {
			if (pthread_join(clients[j], NULL) < 0)
				fatal("pthread_join");
		}

		/* If there is a server thread... */
		//        if (g_critical_sections_type == CST_NULL_RPCS)
		//        {
		/* ...we wait for it to finish its work. */
		//            if (pthread_join(server, NULL) < 0)
		//                fatal("pthread_join");
		//        }
		/* We save the results. */
		for (j = 0; j <= g_number_of_clients; j++) {
			/* FIXME: failed_attempts */
			if (g_measurement_unit == MU_THROUGHPUT
					&& g_measurement_type == MT_GLOBAL
					&& g_measurement_metric == MM_NUMBER_OF_CYCLES)
				g_iteration_result[j] = TO_THROUGHPUT(g_iteration_result[j]);

			results[j * g_number_of_runs + i] = g_iteration_result[j];
		}

		if (g_measurement_location == ML_SERVER
				&& g_measurement_metric == MM_NUMBER_OF_EVENTS) {
			results[0] = (double) g_event_count;
			results[0] /= (g_number_of_iterations_per_client
					* g_number_of_clients);
		}
	}

	int k;

	if (g_verbose) {
		printf("Global: ");

		/* FIXME: access_variables */
		for (k = 0; k < g_number_of_shared_variables; k++)
			printf("%llu,",
					(unsigned long long) (*((volatile uint64_t *) ((uintptr_t) shared_variables_memory_area
							+ k * 2 * sizeof(cache_line_t)))));
		printf("\n");

		printf("Local: ");

		for (j = 0; j < g_number_of_clients + 1; j++) {
			for (k = 0; k < g_number_of_context_variables; k++)
				printf("%llu,",
						(unsigned long long) (*((volatile uint64_t *) ((uintptr_t) context_variables_global_memory_area
								+ j * (g_number_of_context_variables + 1) * 2
										* sizeof(cache_line_t)
								+ (k + 1) * 2 * sizeof(cache_line_t)))));

			printf("\n");
		}

		printf("\n");
	}

#if 0
	for (k = 0; k < g_number_of_shared_variables; k++)
	printf("", (unsigned long long)(*((volatile uint64_t *)
							((uintptr_t)shared_variables_memory_area +
									k * 2 * sizeof(cache_line_t)))));

	for (k = 0; k < g_number_of_context_variables; k++)
	printf("", (unsigned long long)(*((volatile uint64_t *)
							((uintptr_t)context_variables_global_memory_area +
									(k + 1) * 2 * sizeof(cache_line_t)))));
#endif

	/* In the ordered mode, we print the order in which the RPCs were
	 serviced. */
	if (g_execution_mode == EM_SINGLE_RUN_ORDERED) {
		for (i = 0; i < g_number_of_clients; i++) {
			j = 0;

			while (g_order[j] != i)
				j++;

			printf("%d", j);
		}

		printf(",");
	}

	/* If sampling is on, we print the result of each sample followed by the ID
	 of the last core whose RPC has been serviced. */
	if (g_execution_mode == EM_SINGLE_RUN_SAMPLED) {
		for (i = 0; i < number_of_samples; i++) {
			for (j = 0; j < g_number_of_clients + 1; j++) {
				if (g_multiple_samples_rpc_done_addrs[i]
						== g_rpc_done_addresses[j])
					break;
			}

			/* We convert the result to the right unit if needed. */
			if (g_measurement_unit == MU_THROUGHPUT)
				g_multiple_samples_results[i] =
						TO_THROUGHPUT(g_multiple_samples_results[i]);

			/* We return the result. */
			printf("%f,%d\n", g_multiple_samples_results[i], j);
		}
	}

	/* If we're using MU_TOTAL_CYCLES_MAX, we have to find out the maximum
	 * of all measurements. */
	if (g_measurement_unit == MU_TOTAL_CYCLES_MAX) {
		for (i = 0; i < g_number_of_runs; i++) {
			double max = 0;

			for (j = 1; j <= g_number_of_clients; j++) {
				if (max < results[j * g_number_of_runs + i])
					max = results[j * g_number_of_runs + i];
			}

			/* FIXME: TOTAL_CYCLES_MAX is a bad name. Not a unit either.
			 Terrible hack. */
			results[i] = TO_THROUGHPUT(max /
					(g_number_of_iterations_per_client *
							g_number_of_clients));
		}
	} else if (g_measurement_location == ML_CLIENTS
			&& g_average_client_results) {
		for (i = 0; i < g_number_of_runs; i++) {
			double result = 0;
			int number_of_valid_results = 0;

			if (g_measurement_metric == MM_NUMBER_OF_EVENTS) {
				for (j = 1; j <= g_number_of_clients; j++) {
					if (results[j * g_number_of_runs + i] >= 0) {
						result += results[j * g_number_of_runs + i];
						number_of_valid_results++;
					}
				}

				results[i] = result / number_of_valid_results;
			} else {
				for (j = 1; j <= g_number_of_clients; j++) {
					result += results[j * g_number_of_runs + i];
				}

				results[i] = result / g_number_of_clients;
			}
		}
	}

	/* We compute the average and the variance (if needed). */
	for (j = 0; j <= g_number_of_clients; j++) {
		/* Initialization */
		for (i = 0; i < g_number_of_runs; i++) {
			average[j] = 0;
			variance[j] = 0;
		}

		/* We compute the average. */
		for (i = 0; i < g_number_of_runs; i++) {
			average[j] += results[j * g_number_of_runs + i];
		}

		average[j] /= g_number_of_runs;

		/* We compute the variance (only if needed). */
		if (g_compute_standard_deviation_and_variance) {
			for (i = 0; i < g_number_of_runs; i++) {
				variance[j] += (results[j * g_number_of_runs + i] - average[j])
						* (results[j * g_number_of_runs + i] - average[j]);
			}

			variance[j] /= g_number_of_runs;
		}
	}

	/* We print the result. If the measurements took place on the server... */
	if (g_measurement_location == ML_SERVER || g_average_client_results) {
		/* ...we return the average... */
		printf("%f,", average[0]);

		if (g_compute_standard_deviation_and_variance)
			/* ...and, if needed, the variance and standard deviation. */
			printf("%f,%f,", variance[0], sqrt(variance[0]));
	}
	/* Otherwise... */
	else {
		/* ...it's the same, but for each client. */
		for (j = 1; j <= g_number_of_clients; j++) {
			printf("%f,", average[j]);

			if (g_compute_standard_deviation_and_variance)
				/* ...and, if needed, the variance and standard deviation. */
				printf("%f,%f,", variance[j], sqrt(variance[j]));
		}
	}

	/* If sampling is enabled, we need to specify a value for the core ID of the
	 average. We choose -1. */
	if (g_execution_mode == EM_SINGLE_RUN_SAMPLED)
		printf("%d,", -1);

	/* Should we end the input with a newline? */
	if (g_end_output_with_a_newline)
		printf("\n");
	else
		/* If we don't, we make sure to flush the standard output. */
		fflush(NULL);

	/* ====================================================================== */
	/* [v] Cleanup (unnecessary)                                              */
	/* ====================================================================== */
	free((int *) g_physical_to_virtual_core_id);
	free(clients);
	free(thread_argument_blocks);
	free((int *) g_ready);
	free((double *) g_iteration_result);
	free(results);
	free(average);
	free(variance);
	if (g_execution_mode == EM_SINGLE_RUN_ORDERED)
		free((int *) g_order);
	if (g_execution_mode == EM_SINGLE_RUN_SAMPLED) {
		free(g_rpc_done_addresses);
		free((float *) g_multiple_samples_results);
		free((void **) g_multiple_samples_rpc_done_addrs);
	}

#ifdef USE_BLOCKING_LOCKS
	if (pthread_mutex_destroy(&mutex_rpc_done_addr_not_null) < 0
			|| pthread_mutex_destroy(&mutex_rpc_done_positive) < 0
			|| pthread_mutex_destroy(&mutex_rpc_done_addr_null) < 0)
	ERROR("pthread_mutex_destroy");

	if (pthread_cond_destroy(&cond_rpc_done_addr_not_null) < 0
			|| pthread_cond_destroy(&cond_rpc_done_positive) < 0
			|| pthread_cond_destroy(&cond_rpc_done_addr_null) < 0)
	ERROR("pthread_cond_destroy");
#endif
	/* ====================================================================== */
	/* [^] Cleanup (unnecessary)                                              */
	/* ====================================================================== */

	/* Everything went as expected. */
	return EXIT_SUCCESS;
}

/* Function executed by the clients */
void *client_main(void *thread_arguments_block_pointer) {
	int i;
	//printf("client %d is running\n", self.id);

	while (!g_server_started)
		PAUSE();

	/* This is needed because the CAS function requires an address. */
	//    const int one = 1;
	/* We avoid using global variables in (and around) the critical section
	 to limit data cache misses. */
	const int number_of_clients = g_number_of_clients;
	const int number_of_iterations_per_client =
			g_number_of_iterations_per_client;
	const int delay = g_delay;

	const int number_of_context_variables = g_number_of_context_variables;
	const int number_of_shared_variables = g_number_of_shared_variables;

	const int measurement_metric = g_measurement_metric;
	const int measurement_type = g_measurement_type;
	const int access_order = g_access_order;
	const int use_multiple_locks = g_use_multiple_locks;

	const int read_only_wait = g_read_only_wait;
	const int execution_mode = g_execution_mode;

	const int measurement_location = g_measurement_location;

	const int critical_sections_type = g_critical_sections_type;
	const int use_mcs = g_use_mcs;

	const int measurement_unit = g_measurement_unit;

	const int skip_first_cs = g_skip_first_cs;

	/* Argument block */
	thread_arguments_block_t thread_arguments_block;

	/* We copy the contents of the argument block into these variables to
	 improve the readability of the code. */
	volatile uint64_t * volatile *null_rpc_global_sv;
	volatile uint64_t *null_rpc_local_sv;
	volatile uint64_t *lock_acquisitions_global_sv;
	volatile uint64_t *context_variables_local_memory_area,
			*shared_variables_memory_area;

	int client_id, client_core;
	/* PAPI variables */
	int event_set = PAPI_NULL;
	int events[1];
	long long values[1];
	/* FIXME: rename */
	long long start_cycles = 0, end_cycles;
	long long cycles;

	/* Latency */
	long long main_lock_acquisition_beginning = 0, main_lock_acquisition_end;
	/* FIXME: names */
	double total_latency = 0;
	// double average_number_of_failed_attempts = 0;

	/* Failures */
	unsigned long long number_of_failed_attempts = 0;

	int *local_permutations_array, *global_permutations_array;

	/* FIXME */
	// int random_delay;
	mcs_lock_t *l_mcs_me_ptr;

	// Variables used for PAPI measurements.
	long long events_begin, events_end, n_events = 0;

	/* We get the client id. */
	thread_arguments_block =
			*((thread_arguments_block_t *) thread_arguments_block_pointer);

	client_id = thread_arguments_block.id;
	client_core = thread_arguments_block.client_core;
	null_rpc_local_sv = thread_arguments_block.null_rpc_local_sv;
	null_rpc_global_sv = thread_arguments_block.null_rpc_global_sv;
	lock_acquisitions_global_sv =
			thread_arguments_block.lock_acquisitions_global_sv;

	context_variables_local_memory_area =
			(volatile uint64_t *) ((uintptr_t) thread_arguments_block.context_variables_global_memory_area
					+ 2 * sizeof(cache_line_t)
							* (number_of_context_variables + 1) * client_id
					+ 2 * sizeof(cache_line_t));
	// By zmz
	*(int *) context_variables_local_memory_area = g_wat;
	//~By zmz
	shared_variables_memory_area =
			thread_arguments_block.shared_variables_memory_area;

	local_permutations_array = alloc(number_of_context_variables * sizeof(int));

	for (i = 0; i < number_of_context_variables; i++) {
		local_permutations_array[i] = g_local_permutations_array[i];
	}

	global_permutations_array = alloc(number_of_shared_variables * sizeof(int));

	for (i = 0; i < number_of_shared_variables; i++) {
		global_permutations_array[i] = g_global_permutations_array[i];
	}

	/* MCS Lock */
	if (critical_sections_type == CST_LOCK_ACQUISITIONS && use_mcs) {
		if (!g_numa_aware) {
			if (posix_memalign((void **) &l_mcs_me_ptr, 2 * CACHE_LINE_SIZE,
					2 * CACHE_LINE_SIZE))
				fatal("posix_memalign");

			if (l_mcs_me_ptr == NULL)
				fatal("posix_memalign");
		} else {
			//l_mcs_me_ptr = numa_alloc_local(2 * CACHE_LINE_SIZE);
			l_mcs_me_ptr = mmap(0, 2 * sizeof(cache_line_t),
					PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);

			if (l_mcs_me_ptr == NULL)
				//fatal("numa_alloc_local");
				fatal("mmap");
		}
	}

	/* If sampling is on... */
	if (execution_mode == EM_SINGLE_RUN_SAMPLED) {
		/* ...we register the address of our rpc_done local variable. */
		g_rpc_done_addresses[client_core] = null_rpc_local_sv;
	}

	if (measurement_location == ML_CLIENTS) {
		/* Additional initialization is needed if we are counting events. */
		if (measurement_metric == MM_NUMBER_OF_EVENTS) {
			if (client_core % NUMBER_OF_CORES_PER_DIE
					== (NUMBER_OF_CORES_PER_DIE - 1)) {
				/* We will use PAPI in this thread. */
				PAPI_thread_init((unsigned long (*)(void)) pthread_self);

				events[0] = g_monitored_event_id;

				if (PAPI_create_eventset(&event_set) != PAPI_OK)
					fatal("PAPI_create_eventset");
				if (PAPI_add_events(event_set, events, 1) != PAPI_OK)
					fatal("PAPI_add_events");

				/* This seemingly helps increasing PAPI's accuracy. */
				if (PAPI_start(event_set) != PAPI_OK)
					fatal("PAPI_start");
				if (PAPI_stop(event_set, values) != PAPI_OK)
					fatal("PAPI_stop");

			}
		} else {
			PAPI_thread_init((unsigned long (*)(void)) pthread_self);
		}
	}

	t_shared_variables_memory_area = shared_variables_memory_area;
	t_number_of_shared_variables = number_of_shared_variables;
	t_number_of_context_variables = number_of_context_variables;
	t_access_order = access_order;
	t_local_permutations_array = local_permutations_array;
	t_global_permutations_array = global_permutations_array;

	/* We're ready. */
	g_ready[client_id + 1] = 1;

	/* Synchronization barriers */
	if (critical_sections_type == CST_NULL_RPCS) {
		//pthread_barrier_wait(&barrier);
		for (i = 0; i <= number_of_clients; i++)
			while (!g_ready[i])
				PAUSE();
	} else {
		//pthread_barrier_wait(&barrier);
		for (i = 1; i <= number_of_clients; i++)
			while (!g_ready[i])
				PAUSE();
	}

	if (measurement_location == ML_CLIENTS) {
		/* Does the user wish to measure the number of elapsed cycles? */
		if (measurement_metric == MM_NUMBER_OF_CYCLES
				&& measurement_type == MT_GLOBAL) {
			/* If so, get the current cycle count. We could also use
			 * PAPI_start with the TOT_CYC event. */
			start_cycles = PAPI_get_real_cyc();
		} else if (measurement_metric == MM_NUMBER_OF_EVENTS
				&& client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
			/* Otherwise we start counting events. */
			if (PAPI_start(event_set) != PAPI_OK)
				fatal("PAPI_start");
		}
	}

	/* ###################################################################### */
	/* # Main loop (client)                                                 # */
	/* ###################################################################### */
	/* First implementation : using spinlocks. */
#ifndef USE_BLOCKING_LOCKS
	/* ###################################################################### */
	/* NULL RPCs                                                              */
	/* ###################################################################### */
	if (critical_sections_type == CST_NULL_RPCS) {
		/* We execute number_of_iterations_per_client RPCs. */
		for (i = 0; i < number_of_iterations_per_client; i++) {
			if (!use_multiple_locks)
				*null_rpc_local_sv = 0;

			/* We access context variables */
			access_variables(context_variables_local_memory_area, 0,
					number_of_context_variables, 0, local_permutations_array);

			if (measurement_type == MT_LOCK_ACQUISITIONS
					|| measurement_type == MT_CRITICAL_SECTIONS) {
				if (measurement_metric == MM_NUMBER_OF_CYCLES) {
					main_lock_acquisition_beginning = PAPI_get_real_cyc();
				} else if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					if (PAPI_read(event_set, &events_begin) != PAPI_OK)
						perror("PAPI_read");
				}
			}

			/* ============================================================== */
			/* BEGIN Lock acquisition                                         */
			/* ============================================================== */
			if (use_multiple_locks) {
				*null_rpc_local_sv = 1;
			} else {
				if (measurement_metric == MM_FAILED_ATTEMPTS) {
					/* FIXME: doesn't work with multiple locks! */
					/* FIXME: handle the g_skip_first_cs parameter? */
					if (!read_only_wait) {
						/* We use a compare and swap to avoid mutexes. */
						while (!__sync_bool_compare_and_swap(null_rpc_global_sv,
								0, null_rpc_local_sv)) {
							number_of_failed_attempts++;
							PAUSE();
						}
					} else {
						for (;;) {
							if (!(*null_rpc_global_sv)) {
								if (!__sync_bool_compare_and_swap(
										null_rpc_global_sv, 0,
										null_rpc_local_sv)) {
									number_of_failed_attempts++;
									continue;
								} else
									break;
							}

							PAUSE();
						}
					}
				} else {
					if (!read_only_wait) {
						/* We use a compare and swap to avoid mutexes. */
						while (!__sync_bool_compare_and_swap(null_rpc_global_sv,
								0, null_rpc_local_sv))
							PAUSE();
					} else {
						for (;;) {
							if (!(*null_rpc_global_sv)) {
								if (!__sync_bool_compare_and_swap(
										null_rpc_global_sv, 0,
										null_rpc_local_sv))
									continue;
								else
									break;
							}

							PAUSE();
						}
					}
				}
			}
			/* ============================================================== */
			/* END Lock acquisition                                           */
			/* ============================================================== */

			if (measurement_type == MT_LOCK_ACQUISITIONS
					&& (!skip_first_cs || i > 0)) {
				if (measurement_metric == MM_NUMBER_OF_CYCLES) {
					main_lock_acquisition_end = PAPI_get_real_cyc();

					total_latency += main_lock_acquisition_end
							- main_lock_acquisition_beginning;

					if (execution_mode == EM_SINGLE_RUN_SAMPLED) {
						g_latencies[client_id] = main_lock_acquisition_end
								- main_lock_acquisition_beginning;
					}
				} else if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					if (PAPI_read(event_set, &events_end) != PAPI_OK)
						perror("PAPI_read");

					n_events += events_end - events_begin;
				}
			}
			/*
			 else if (measurement_type == FAILED_ATTEMPTS)
			 {
			 local_average_number_of_failed_attempts +=
			 (double)number_of_failed_attempts /
			 local_number_of_iterations_per_client;

			 number_of_failed_attempts = 0;
			 }
			 */

			/* ============================================================== */
			/* BEGIN Lock release                                             */
			/* ============================================================== */
			if (use_multiple_locks) {
				// if (PAPI_reset(event_set) != PAPI_OK)
				//     perror("PAPI_reset");
				while (*null_rpc_local_sv == 1)
					PAUSE();
				// if (PAPI_accum(event_set, values) != PAPI_OK)
				//     perror("PAPI_accum");
			} else {
				while (*null_rpc_local_sv == 0)
					PAUSE();
			}
			/* ============================================================== */
			/* END Lock release                                               */
			/* ============================================================== */

			if (measurement_type == MT_CRITICAL_SECTIONS
					&& (!skip_first_cs || i > 0)) {
				if (measurement_metric == MM_NUMBER_OF_CYCLES) {
					main_lock_acquisition_end = PAPI_get_real_cyc();

					total_latency += main_lock_acquisition_end
							- main_lock_acquisition_beginning;

					if (execution_mode == EM_SINGLE_RUN_SAMPLED) {
						g_latencies[client_id] = main_lock_acquisition_end
								- main_lock_acquisition_beginning;
					}
				} else if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					if (PAPI_read(event_set, &events_end) != PAPI_OK)
						perror("PAPI_read");

					n_events += events_end - events_begin;
				}
			}

			/* We could avoid the delay introduced by the test by moving the
			 if outside of the loop, however, tests show that no delay or
			 a small delay doesn't alter the results significantly. */
			if (delay > 0) {
				/* Delay */
				cycles = PAPI_get_real_cyc();
				while ((PAPI_get_real_cyc() - cycles) < delay)
					;

				/*
				 cycles = PAPI_get_real_cyc();
				 random_delay = rand() % local_delay;

				 while ((PAPI_get_real_cyc() - cycles) < random_delay)
				 ;
				 */
			}
		}
	}
	/* ###################################################################### */
	/* Spin locks                                                             */
	/* ###################################################################### */
	else if (critical_sections_type == CST_LOCK_ACQUISITIONS) {
		if (execution_mode != EM_SINGLE_RUN_SAMPLED) {
			for (i = 0; i < number_of_iterations_per_client; i++) {
				/* We access context variables */
				access_variables(context_variables_local_memory_area, 0,
						number_of_context_variables, 0,
						local_permutations_array);

				/*
				 access_variables(null_rpc_local_sv,
				 1,
				 local_number_of_context_variables,
				 0);
				 */

				if (measurement_type == MT_LOCK_ACQUISITIONS
						|| measurement_type == MT_CRITICAL_SECTIONS) {
					if (measurement_metric == MM_NUMBER_OF_CYCLES) {
						main_lock_acquisition_beginning = PAPI_get_real_cyc();
					} else if (client_core % NUMBER_OF_CORES_PER_DIE
							== (NUMBER_OF_CORES_PER_DIE - 1)) {
						if (PAPI_read(event_set, &events_begin) != PAPI_OK)
							perror("PAPI_read");
					}
				}

				/* ========================================================== */
				/* BEGIN Lock acquisition                                     */
				/* ========================================================== */
				if (measurement_metric == MM_FAILED_ATTEMPTS) {
					if (!read_only_wait) {
						if (use_mcs) {
							lock_mcs(g_mcs_m_ptr, l_mcs_me_ptr);
						} else {
							while (!__sync_bool_compare_and_swap(
									lock_acquisitions_global_sv, 0, 1)) {
								number_of_failed_attempts++;
								PAUSE();
							}
						}
					} else {
						for (;;) {
							if (!*null_rpc_global_sv) {
								if (!__sync_bool_compare_and_swap(
										lock_acquisitions_global_sv, 0, 1)) {
									number_of_failed_attempts++;
									continue;
								}

								else
									break;
							}

							PAUSE();
						}
					}
				} else {
					if (!read_only_wait) {
						if (use_mcs) {
							lock_mcs(g_mcs_m_ptr, l_mcs_me_ptr);
						} else {
							while (!__sync_bool_compare_and_swap(
									lock_acquisitions_global_sv, 0, 1))
								PAUSE();
						}
					} else {
						for (;;) {
							if (!*null_rpc_global_sv) {
								if (!__sync_bool_compare_and_swap(
										lock_acquisitions_global_sv, 0, 1))
									continue;

								else
									break;
							}

							PAUSE();
						}
					}
				}
				/* ========================================================== */
				/* END Lock acquisition                                       */
				/* ========================================================== */

				//__sync_synchronize();
				if (measurement_type == MT_LOCK_ACQUISITIONS
						&& (!skip_first_cs || i > 0)) {
					if (measurement_metric == MM_NUMBER_OF_CYCLES) {
						main_lock_acquisition_end = PAPI_get_real_cyc();

						total_latency += main_lock_acquisition_end
								- main_lock_acquisition_beginning;
					} else if (client_core % NUMBER_OF_CORES_PER_DIE
							== (NUMBER_OF_CORES_PER_DIE - 1)) {
						if (PAPI_read(event_set, &events_end) != PAPI_OK)
							perror("PAPI_read");

						n_events += events_end - events_begin;
					}
				}

				/* We access context variables */
				access_variables(context_variables_local_memory_area, 0,
						number_of_context_variables, access_order,
						local_permutations_array);

				/* We access shared variables */
				access_variables(shared_variables_memory_area, 0,
						number_of_shared_variables, access_order,
						global_permutations_array);

				//__sync_synchronize();

				/* ========================================================== */
				/* BEGIN Lock release                                         */
				/* ========================================================== */
				if (use_mcs) {
					unlock_mcs(g_mcs_m_ptr, l_mcs_me_ptr);
				} else {
					*lock_acquisitions_global_sv = 0;
				}
				/* ========================================================== */
				/* END Lock release                                           */
				/* ========================================================== */

				if (measurement_type == MT_CRITICAL_SECTIONS
						&& (!skip_first_cs || i > 0)) {
					if (measurement_metric == MM_NUMBER_OF_CYCLES) {
						main_lock_acquisition_end = PAPI_get_real_cyc();

						total_latency += main_lock_acquisition_end
								- main_lock_acquisition_beginning;
					} else if (client_core % NUMBER_OF_CORES_PER_DIE
							== (NUMBER_OF_CORES_PER_DIE - 1)) {
						if (PAPI_read(event_set, &events_end) != PAPI_OK)
							perror("PAPI_read");

						n_events += events_end - events_begin;
					}
				}

				if (delay > 0) {
					/* Delay */
					cycles = PAPI_get_real_cyc();
					while ((PAPI_get_real_cyc() - cycles) < delay)
						;

					/*
					 cycles = PAPI_get_real_cyc();
					 random_delay = rand() % delay;
					 while ((PAPI_get_real_cyc() - cycles) < random_delay)
					 ;
					 */
				}
			}
		}
	}
	/* ###################################################################### */
	/* Liblock                                                                */
	/* ###################################################################### */
	else {
		for (i = 0; i < number_of_iterations_per_client; i++) {
			/* We access context variables */
			access_variables(context_variables_local_memory_area, 0,
					number_of_context_variables, 0, local_permutations_array);

			/* FIXME: MT_LOCK_ACQUISTIONS can't be handled. Warning message
			 (@main()) */

			if (measurement_type == MT_CRITICAL_SECTIONS) {
				if (measurement_metric == MM_NUMBER_OF_CYCLES) {
					main_lock_acquisition_beginning = PAPI_get_real_cyc();
				} else if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					if (PAPI_read(event_set, &events_begin) != PAPI_OK)
						perror("PAPI_read");
				}
			}

			//__sync_synchronize();

			// LIBLOCK -> fill me here
			//{ static int zzz = 0; if(!(__sync_fetch_and_add(&zzz, 1) % 500))
			//printf("executes %d for client %d\n", zzz, self.id); }
			liblock_exec(&g_liblock_lock, cs,
					(void *) context_variables_local_memory_area);

			//__sync_synchronize();

			if (measurement_type == MT_CRITICAL_SECTIONS
					&& (!skip_first_cs || i > 0)) {
				if (measurement_metric == MM_NUMBER_OF_CYCLES) {
					main_lock_acquisition_end = PAPI_get_real_cyc();

					total_latency += main_lock_acquisition_end
							- main_lock_acquisition_beginning;
				} else if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					if (PAPI_read(event_set, &events_end) != PAPI_OK)
						perror("PAPI_read");

					n_events += events_end - events_begin;
				}
			}

			if (delay > 0) {
				/* Delay */
				cycles = PAPI_get_real_cyc();
				while ((PAPI_get_real_cyc() - cycles) < delay)
					;
			}
		}
	}
	/* Second implementation : using blocking locks. Deprecated. */
#else
	/* We execute number_of_iterations_per_client RPCs. */
	for (i = 0; i < local_number_of_iterations_per_client; i++)
	{
		*null_rpc_local_sv = 0;

		/* This lock corresponds to the CAS from the previous implementation. */
		pthread_mutex_lock(&mutex_rpc_done_addr_null);

		/* We wait for the server to be ready. */
		while(global->rpc_done_addr != 0)
		pthread_cond_wait(&cond_rpc_done_addr_null,
				&mutex_rpc_done_addr_null);

		/* The server is ready, let's execute a RPC. */
		pthread_mutex_lock(&mutex_rpc_done_addr_not_null);
		*null_rpc_global_sv = null_rpc_local_sv;
		pthread_cond_signal(&cond_rpc_done_addr_not_null);
		pthread_mutex_unlock(&mutex_rpc_done_addr_not_null);

		pthread_mutex_unlock(&mutex_rpc_done_addr_null);

		/* We wait until the server is done with our RPC. */
		pthread_mutex_lock(&mutex_rpc_done_positive);
		while (*null_rpc_local_sv == 0)
		pthread_cond_wait(&cond_rpc_done_positive,
				&mutex_rpc_done_positive);
		pthread_mutex_unlock(&mutex_rpc_done_positive);
	}
#endif
	/* ###################################################################### */
	/* # End                                                                # */
	/* ###################################################################### */

	if (measurement_location == ML_CLIENTS) {
		/* Are we counting cycles? */
		if (measurement_metric == MM_NUMBER_OF_CYCLES) {
			if (measurement_type == MT_GLOBAL) {
				/* If so, get the current cycle count. */
				end_cycles = PAPI_get_real_cyc();

				if (measurement_unit != MU_TOTAL_CYCLES_MAX) {
					/* We return the number of cycles per RPC. */
					g_iteration_result[client_id + 1] = (double) (end_cycles
							- start_cycles) / number_of_iterations_per_client;
				} else {
					g_iteration_result[client_id + 1] = (double) (end_cycles
							- start_cycles);
				}
			} else if (measurement_type == MT_LOCK_ACQUISITIONS
					|| measurement_type == MT_CRITICAL_SECTIONS) {
				g_iteration_result[client_id + 1] = total_latency
						/ (number_of_iterations_per_client
								- (skip_first_cs ? 1 : 0));
			}
		} else if (measurement_metric == MM_NUMBER_OF_EVENTS) {
			if (measurement_type == MT_GLOBAL) {
				if (PAPI_stop(event_set, values) != PAPI_OK)
					fatal("PAPI_stop");
			}

			/* When using the liblock, we rely on an improved implementation
			 that uses PAPI_read. */
			if (critical_sections_type == CST_LIBLOCK) {
				if (client_core % NUMBER_OF_CORES_PER_DIE
						== (NUMBER_OF_CORES_PER_DIE - 1)) {
					g_iteration_result[client_id + 1] = (double) n_events
							/ number_of_iterations_per_client;
				} else {
					g_iteration_result[client_id + 1] = -1;
				}
			} else {
				g_iteration_result[client_id + 1] = (double) values[0]
						/ number_of_iterations_per_client;
			}
		} else /* if (local_measurement_metric == MM_FAILED_ATTEMPTS) */
		{
			/*
			 iteration_result[client_id + 1] =
			 local_average_number_of_failed_attempts;
			 */
			g_iteration_result[client_id + 1] =
					(double) number_of_failed_attempts
							/ (number_of_iterations_per_client
									- (skip_first_cs ? 1 : 0));
		}

	}

	/* In the ordered mode, we save the order in which RPCs are serviced. */
	if (execution_mode == EM_SINGLE_RUN_ORDERED) {
		g_order[client_id] = *null_rpc_local_sv;
	}

	if (measurement_metric != MM_NUMBER_OF_EVENTS
			|| client_id % NUMBER_OF_CORES_PER_DIE
					== (NUMBER_OF_CORES_PER_DIE - 1)) {
		PAPI_unregister_thread();
	}

	int fin_num = 0;
	// Debug
	//printf("g_number_of_finished_clients is %d  g_number_of_clients is %d\n", g_number_of_finished_clients, g_number_of_clients);
	//~Debug
	if ((fin_num = __sync_add_and_fetch(&g_number_of_finished_clients, 1))
			== g_number_of_clients)
		if (g_critical_sections_type == CST_LIBLOCK) {
			liblock_lock_destroy(&g_liblock_lock);
		}

	if (fin_num == g_number_of_clients - 1 && g_liblock_name && strcmp(g_liblock_name, "saml") == 0){
		liblock_unlock_in_cs(&g_liblock_lock);
	}


	return NULL;
}

/* Function executed by the server */
void *server_main(void *thread_arguments_block_pointer) {
	int i, j;
	/* We avoid using global variables in the main loop to limit data cache
	 misses. */
	const int number_of_clients = g_number_of_clients;
	const int number_of_iterations = g_number_of_iterations_per_client
			* g_number_of_clients;

	/* Sampling-related variables. */
	const int number_of_samples = number_of_iterations
			/ g_number_of_iterations_per_sample;
	const int number_of_iterations_per_sample_m1 =
			g_number_of_iterations_per_sample - 1;
	int sample_start_cycles, sample_end_cycles;

	const int number_of_context_variables = g_number_of_context_variables;
	const int number_of_shared_variables = g_number_of_shared_variables;

	const int measurement_metric = g_measurement_metric;
	const int measurement_type = g_measurement_type;
	const int access_order = g_access_order;
	const int use_multiple_locks = g_use_multiple_locks;

	const int measurement_location = g_measurement_location;

	const int measurement_unit = g_measurement_unit;
	const int execution_mode = g_execution_mode;

	thread_arguments_block_t thread_arguments_block;

	volatile uint64_t * volatile *null_rpc_global_sv;

	volatile uint64_t *context_variables_global_memory_area,
			*shared_variables_memory_area;

	/* This variable is used to pin the thread to the right core. */
	cpu_set_t cpuset;

	/* PAPI variables */
	int event_set = PAPI_NULL;
	int events[1];
	long long values[1];
	long long start_cycles = 0, end_cycles;

	int *local_permutations_array, *global_permutations_array;

	/* We pin this thread to the right core. */
	CPU_ZERO(&cpuset);
	CPU_SET(g_physical_to_virtual_core_id[g_server_core], &cpuset);

	/* TODO: free */
	local_permutations_array = alloc(number_of_context_variables * sizeof(int));

	for (i = 0; i < number_of_context_variables; i++) {
		local_permutations_array[i] = g_local_permutations_array[i];
	}

	/* TODO: free */
	global_permutations_array = alloc(number_of_shared_variables * sizeof(int));

	for (i = 0; i < number_of_shared_variables; i++) {
		global_permutations_array[i] = g_global_permutations_array[i];
	}

	thread_arguments_block =
			*((thread_arguments_block_t *) thread_arguments_block_pointer);

	null_rpc_global_sv = thread_arguments_block.null_rpc_global_sv;

	context_variables_global_memory_area =
			thread_arguments_block.context_variables_global_memory_area;
	shared_variables_memory_area =
			thread_arguments_block.shared_variables_memory_area;

	/* Are we monitoring cycles/events on the server? */
	if (measurement_location == ML_SERVER) {
		/* If so, we will use PAPI in this thread. */
		PAPI_thread_init((unsigned long (*)(void)) pthread_self);

		/* If we are counting events... */
		if (measurement_metric == MM_NUMBER_OF_EVENTS) {
			events[0] = g_monitored_event_id;

			/* We initialize the even set. */
			if (PAPI_create_eventset(&event_set) != PAPI_OK)
				fatal("PAPI_create_eventset");
			if (PAPI_add_events(event_set, events, 1) != PAPI_OK)
				fatal("PAPI_add_events");

			/* Starting and stopping PAPI once before measurements seems to
			 improve accuracy. */
			if (PAPI_start(event_set) != PAPI_OK)
				fatal("PAPI_start");
			if (PAPI_stop(event_set, values) != PAPI_OK)
				fatal("PAPI_stop");
		}
	}

	/* We are ready. */
	g_ready[0] = 1;

	/* We want all the clients to be ready before we start servicing RPCs. */
	for (i = /*1*/0; i <= number_of_clients; i++)
		while (!g_ready[i])
			;

	PAUSE();
	// pthread_barrier_wait(&barrier);

	/* If the measurements take place on the server... */
	if (measurement_location == ML_SERVER && measurement_type == MT_GLOBAL) {
		/* ...and if we're county cycles... */
		if (measurement_metric == MM_NUMBER_OF_CYCLES) {
			/* ...we get the current cycle count. */
			start_cycles = PAPI_get_real_cyc();
		} else /* if (local_measurement_metric == MM_NUMBER_OF_EVENTS) */
		{
			/* We start the event counter. */
			if (PAPI_start(event_set) != PAPI_OK)
				fatal("PAPI_start");
		}
	}

	j = 1;

	/* ###################################################################### */
	/* # Main loop (server)                                                 # */
	/* ###################################################################### */
	/* First implementation : using spinlocks. */
#ifndef USE_BLOCKING_LOCKS
	/* Sampling is off. */
	if (execution_mode != EM_SINGLE_RUN_SAMPLED) {
		/* We start answering RPCs. */
		for (i = 0; i < number_of_iterations; i++) {
			if (use_multiple_locks) {
				for (;;) {
					for (j = 1; j <= number_of_clients; j++) {
						if (*((volatile uint64_t *) ((uintptr_t) null_rpc_global_sv
								+ j * 2 * sizeof(cache_line_t))))
							goto out;

					}
					PAUSE();
				}
			} else {

				/* No address received from a client? That means no RPC has been
				 requested. */
				while (!(*null_rpc_global_sv))
					PAUSE();
			}

			out:

			//__sync_synchronize();

			if (use_multiple_locks) {
				access_variables(
						(volatile uint64_t *) ((uintptr_t) context_variables_global_memory_area
								+ (j - 1) * 2 * sizeof(cache_line_t)
										* (number_of_context_variables + 1)), 1,
						number_of_context_variables, access_order,
						local_permutations_array);
			} else {
				access_variables((volatile uint64_t *) (*null_rpc_global_sv), 1,
						number_of_context_variables, access_order,
						local_permutations_array);
			}

			/* We access shared variables */
			access_variables(shared_variables_memory_area, 0,
					number_of_shared_variables, access_order,
					global_permutations_array);

			//__sync_synchronize();

			if (use_multiple_locks) {
				*((volatile uint64_t *) ((uintptr_t) null_rpc_global_sv
						+ j * 2 * sizeof(cache_line_t))) = 0;
			} else {
				/* We notify the client we are done with his RPC by setting the
				 variable it provided to i. */
				**null_rpc_global_sv = /*i + 1*/1;

				/* We are done with our RPC. */
				*null_rpc_global_sv = 0;
			}

			//__sync_synchronize();
		}
	}
	/* Sampling is on. */
	else {
		/* Outer loop: one iteration per sample. */
		for (j = 0; j < number_of_samples; j++) {
			/* Same as before, except now we get cycle statistics for each
			 sample. */
			sample_start_cycles = PAPI_get_real_cyc();

			for (i = 0; i < number_of_iterations_per_sample_m1; i++) {
				while (!(*null_rpc_global_sv))
					PAUSE();

				access_variables((volatile uint64_t *) (*null_rpc_global_sv), 1,
						number_of_context_variables, access_order,
						local_permutations_array);

				/* We access shared variables */
				access_variables(shared_variables_memory_area, 0,
						number_of_shared_variables, access_order,
						global_permutations_array);

				**null_rpc_global_sv = /* i + 1 */1;
				*null_rpc_global_sv = 0;
			}

			while (!(*null_rpc_global_sv))
				PAUSE();

			access_variables((volatile uint64_t *) (*null_rpc_global_sv), 1,
					number_of_context_variables, access_order,
					local_permutations_array);

			/* We access shared variables */
			access_variables(shared_variables_memory_area, 0,
					number_of_shared_variables, access_order,
					global_permutations_array);

			**null_rpc_global_sv = /* i + 1 */1;

			sample_end_cycles = PAPI_get_real_cyc();

			/* We need to know which core was serviced last. */
			g_multiple_samples_rpc_done_addrs[j] = *null_rpc_global_sv;

			if (measurement_type == MT_LOCK_ACQUISITIONS
					|| measurement_type == MT_CRITICAL_SECTIONS) {
				/* FIXME */
				int k;

				/* FIXME: too slow, can be improved (if the client returns the
				 * pointer to a struct containing the right info) */
				k = 0;

				/*
				 for (k = 0; k < number_of_clients; k++)
				 {
				 if (multiple_samples_rpc_done_addrs[j] ==
				 rpc_done_addresses[k])
				 break;
				 }
				 */

				g_multiple_samples_results[j] = g_latencies[k];
			} else {
				/* We save the results. */
				g_multiple_samples_results[j] = (double) (sample_end_cycles
						- sample_start_cycles)
						/ g_number_of_iterations_per_sample; /* FIXME: g */
			}

			*null_rpc_global_sv = 0;
		}
	}
	/* Second implementation : using blocking locks. Deprecated. */
#else
	/* We start answering RPCs. */
	for (i = 0; i < number_of_iterations; i++)
	{
		/* No address received from a client? That means no RPC has been
		 requested. */
		pthread_mutex_lock(&mutex_rpc_done_addr_not_null);
		while (!(*null_rpc_global_sv))
		pthread_cond_wait(&cond_rpc_done_addr_not_null,
				&mutex_rpc_done_addr_not_null);
		pthread_mutex_unlock(&mutex_rpc_done_addr_not_null);

		/* We notify the client we are done with his RPC by setting the
		 variable it provided to i. */
		pthread_mutex_lock(&mutex_rpc_done_positive);
		**null_rpc_global_sv = i;
		pthread_cond_signal(&cond_rpc_done_positive);
		pthread_mutex_unlock(&mutex_rpc_done_positive);

		/* We are done with our RPC. */
		pthread_mutex_lock(&mutex_rpc_done_addr_null);
		*null_rpc_global_sv = 0;
		pthread_cond_signal(&cond_rpc_done_addr_null);
		pthread_mutex_unlock(&mutex_rpc_done_addr_null);
	}
#endif
	/* ###################################################################### */
	/* # End                                                                # */
	/* ###################################################################### */

	/* We gather statistics if measurements are to take place on the server. */
	if (measurement_location == ML_SERVER) {
		/* Are we counting cycles? */
		if (measurement_metric == MM_NUMBER_OF_CYCLES) {
			/* If so, get the current cycle count. */
			end_cycles = PAPI_get_real_cyc();

			if (measurement_unit != MU_TOTAL_CYCLES_MAX) {
				/* We return the number of cycles per RPC. */
				g_iteration_result[0] = (double) (end_cycles - start_cycles)
						/ number_of_iterations;
			} else {
				g_iteration_result[0] = (double) (end_cycles - start_cycles);
			}
		}
		/* We're counting events. */
		else /* if (local_measurement_metric == MM_NUMBER_OF_EVENTS) */
		{
			/* Otherwise, we were counting events, therefore, we read the number
			 of events. */
			if (PAPI_stop(event_set, values) != PAPI_OK)
				fatal("PAPI_stop");

			/* We return the number of events. */
			g_iteration_result[0] = (double) values[0] / number_of_iterations;
		}
	}

	PAPI_unregister_thread();

	return NULL;
}

/* This function accesses one variable per cache line. */
inline void access_variables(volatile uint64_t *memory_area,
		int first_variable_number, int number_of_variables, int access_order,
		int *permutations_array) {
	int k, random_number;

	int i = 0;
	access_order = AO_CUSTOM_RANDOM;
	switch (access_order) {
	case AO_SEQUENTIAL: {
		for (k = 0; k < number_of_variables; k++) {
			(*((volatile uint64_t *) ((uintptr_t) memory_area
					+ (k + first_variable_number) * 2 * sizeof(cache_line_t))))++;
		}
	}
		break;

	case AO_DEPENDENT: {
		for (k = 0; k < number_of_variables; k++) {
			i =
					(*((volatile uint64_t *) ((uintptr_t) memory_area
							+ (i % 2) * sizeof(uint64_t)
							+ (k + first_variable_number) * 2
									* sizeof(cache_line_t))))++;
		}
	}
		break;

	case AO_RANDOM: {
		for (k = 0; k < number_of_variables; k++)
			(*((volatile uint64_t *) ((uintptr_t) memory_area
					+ (rand() % number_of_variables + first_variable_number) * 2
							* sizeof(cache_line_t))))++;
	}
		break;

	case AO_CUSTOM_RANDOM: {
		random_number = (int) PAPI_get_real_cyc();

		for (k = 0; k < number_of_variables; k++) {
			random_number = _rand(random_number);

			(*((volatile uint64_t *) ((uintptr_t) memory_area
					+ (random_number % number_of_variables
							+ first_variable_number) * 2 * sizeof(cache_line_t))))++;
		}
	}
		break;

	case AO_PERMUTATIONS: {
		for (k = 0; k < number_of_variables; k++)
			(*((volatile uint64_t *) ((uintptr_t) memory_area
					+ (permutations_array[k] + first_variable_number) * 2
							* sizeof(cache_line_t))))++;
	}
		break;
	}
}

inline int _rand(int next) {
	next = next * 1103515245 + 12345;

	return (unsigned int) (next / 65536) % 32768;
}

void generate_permutations_array(volatile int **permutations_array, int size) {
	int i, r;
	int *seen;

	/* FIXME: allocate outside, and free */
	*permutations_array = alloc(size * sizeof(int));
	seen = alloc(size * sizeof(int));

	memset(seen, 0, size * sizeof(int));

	for (i = 0; i < size; i++) {

		do {
			r = rand() % size;
		} while (seen[r]);

		seen[r] = 1;

		(*permutations_array)[i] = r;
	}

	//for (i = 0; i < size; i++)
	//  printf("%i\n", (*permutations_array)[i]);

	free(seen);
}

/* Wrapper for malloc that checks for errors. */
void *alloc(int size) {
	void *result;

	/* We just call malloc and check for errors. */
	if ((result = malloc(size)) == NULL)
		fatal("malloc");

	/* We return the result. */
	return result;
}

/* This function gets the CPU speed then allocates and fills the
 virtual_to_physical_core_id array. */
void get_cpu_info() {
	int i = 0, j, n;
	float previous_core_frequency = -1, core_frequency = 0;

	/* Compute frequency. */
	for (i = 0; i < topology->nb_cores; i++) {
		core_frequency = topology->cores[i].frequency;
		if (previous_core_frequency == -1)
			previous_core_frequency = core_frequency;
		else if (previous_core_frequency != core_frequency)
			fatal("all processing cores must use the same clock frequency");
	}

	/* We update the global cpu_frequency variable. */
	g_cpu_frequency = core_frequency;

	g_number_of_cores = topology->nb_cores;

	/* We allocate the physical_to_virtual_core_id array. */
	g_physical_to_virtual_core_id = malloc(g_number_of_cores * sizeof(int));

	for (i = 0, n = 0; i < topology->nb_nodes; i++)
		for (j = 0; j < topology->nodes[i].nb_cores; j++) {
			g_physical_to_virtual_core_id[n++] =
					topology->nodes[i].cores[j]->core_id;
		}

#if 0   
	printf("p2v: ");
	for (i = 0; i < g_number_of_cores; i++) {
		printf("%d, ", g_physical_to_virtual_core_id[i]);
	}
	printf("\n");
#endif
}

// topology of amd48    
//     int fixed_virtual_to_physical_core_id[] = 
//         {
//             0, 12, 24, 36, 1, 13, 25, 37, 2, 14, 26, 38, 3, 15, 27, 39,
//             4, 16, 28, 40, 5, 17, 29, 41, 6, 18, 30, 42, 7, 19, 31, 43, 
//             8, 20, 32, 44, 9, 21, 33, 45, 10, 22, 34, 46, 11, 23, 35, 47
//         };
//     /*

/* This function writes the usage string to stderr and exits the application. */
void wrong_parameters_error(char *application_name) {
	const char *usage = "usage: see source code";
	//        "usage: %s [-R] [-L] \n"
	//        "       [-A number_of_runs] [-S #_iterations_per_sample] [-O]\n"
	//        "       [-s core] [-c #_clients] [-n #_iterations_per_client] "
	//        "[-d delay] [-r] [-l #_of_local_variables] "
	//        "[-g #_of_context_variables] [-z] [-x] [-p]\n"
	//        "       [-e event_type] [-a] [-f] [-m] [-y] [-t] [-v] [-i]\n";

	/* TODO: print detailed usage. */

	fprintf(stderr, usage, application_name);
	exit(EXIT_FAILURE);
}

// avoid useless warning from libnuma<
void numa_warn(int number, char *where, ...) {
}

void liblock_print_stats() {
} /* don't print stats */

static struct core* get_core(unsigned int physical_core) {
	return &topology->cores[g_physical_to_virtual_core_id[physical_core]];
}

static int g_event_set = PAPI_NULL;
static volatile int g_events_monitored_server = -1;
static volatile int g_events_number_of_threads = 0;

void liblock_on_server_thread_start(const char* lib, unsigned int thread_id) {
	long long values[1];

	PAPI_thread_init((unsigned long (*)(void)) pthread_self);

	//fprintf(stderr, "yop start: %d %d\n", g_events_monitored_server, g_events_number_of_threads);
	if (g_monitored_event_id != 0) {
		if (__sync_val_compare_and_swap(&g_events_monitored_server, -1,
				self.running_core->core_id)) {
			if (PAPI_create_eventset(&g_event_set) != PAPI_OK)
				warning("PAPI_create_eventset");
			else if (PAPI_add_event(g_event_set,
					g_monitored_event_id) != PAPI_OK)
				warning("PAPI_add_events...");
			/* This seemingly helps increasing PAPI's accuracy. */
			else {
				if (PAPI_start(g_event_set) != PAPI_OK)
					warning("PAPI_start");
				if (PAPI_stop(g_event_set, values) != PAPI_OK)
					warning("PAPI_stop");
				if (PAPI_start(g_event_set) != PAPI_OK)
					warning("PAPI_start");
			}
		}
		if (g_events_monitored_server == self.running_core->core_id)
			__sync_fetch_and_add(&g_events_number_of_threads, 1);
	}
}

void liblock_on_server_thread_end(const char* lib, unsigned int thread_id) {
	long long values[1];

	//fprintf(stderr, "yop end: %d %d\n", g_events_monitored_server, g_events_number_of_threads);
	if (g_monitored_event_id != 0
			&& g_events_monitored_server == self.running_core->core_id
			&& !__sync_sub_and_fetch(&g_events_number_of_threads, 1)) {

		if (PAPI_stop(g_event_set, values) != PAPI_OK)
			warning("PAPI_stop ???");
		else
			g_event_count = values[0];
	}
	//printf("Server DCM: %lld\n", g_event_count);
}
