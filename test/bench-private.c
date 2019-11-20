/*
 * In this benchmark, each worker thread/core performs
 * a sequence of (64-bit) memory load instructions on contiguous
 * memory locations inside a per-thread buffer
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <numa.h>
#include <numaif.h>
#include <sched.h>
#include <linux/unistd.h>
#include <sys/sysinfo.h>
#include <sys/syscall.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <unstickymem/unstickymem.h>

// XXX temporary workaround for bug in numactl XXX
// https://github.com/numactl/numactl/issues/38
#ifndef MPOL_LOCAL
#define MPOL_LOCAL 4
#endif

#define COLOR_RED "\x1B[31m"
#define COLOR_NONE "\x1B[0m"

#define die(msg, args...)                                                 \
  do {                                                                    \
    fprintf(stderr,"(%s,%d) " msg "\n", __FUNCTION__ , __LINE__, ##args); \
    exit(-1);                                                             \
  } while(0)

#define handle_error_en(en, msg)                                          \
  do { errno = en; perror(msg); exit(EXIT_FAILURE); } while (0)

// The cache size of your processor, in bytes. Adjust accordingly.
#define cacheSize 5 * 1024 * 1024 // 5 * kB * mB

#define DEFAULT_BENCH_TIME (10LL) // In seconds
#define DEFAULT_MEMORY_BENCH_SIZE_TO_BENCH      (64*1024*1024) // In bytes

pthread_t * threads;
pthread_barrier_t barrier;
pthread_mutex_t running_mutex = PTHREAD_MUTEX_INITIALIZER;
static int *core_to_node;
static int **node_to_cores;
static long *node_to_used_memory;
int ncpus;
int nnodes;
int * cores;  //keeps the numbers of the passed cores
static int ncpus_per_node;
uint64_t bench_time = DEFAULT_BENCH_TIME;

uint64_t* nb_bytes_processed; /* Number of bytes actually processed by a thread */

int size_per_thread, iterations, nthreads, PR_SH_RATIO, STEP;

struct thread_data {
  unsigned long thread_no;
  unsigned long assigned_core;
  unsigned long assigned_node;
};

double node_bw[8];
uint64_t memory_size = 0;

/*
 * Benchmark
 * The "O0" attribute is required to make sure that the compiler does not optimize the code
 */
uint64_t bench_seq_read(uint64_t* memory_to_access, uint64_t memory_size,
                        uint64_t time, uint32_t thread_no) {
  int i, j, fake = 0;
  uint64_t nb_iterations = 0;

  /*
   * The loop is partially unrolled in order to increase
   * the ratio of load instructions vs branch instructions
   */
  for (i = 0; i < 500; i++) {
    for (j = 0; j < (memory_size / sizeof(*memory_to_access)); j += 8) {
      fake += memory_to_access[j];
      fake += memory_to_access[j + 1];
      fake += memory_to_access[j + 2];
      fake += memory_to_access[j + 3];
      fake += memory_to_access[j + 4];
      fake += memory_to_access[j + 5];
      fake += memory_to_access[j + 6];
      fake += memory_to_access[j + 7];
    }

    nb_iterations++;
  }

  return memory_size * nb_iterations;
}

uint64_t get_cpu_freq(void) {
  FILE * fd;
  uint64_t freq = 0;
  float freqf = 0;
  char *line = NULL;
  size_t len = 0;

  fd = fopen("/proc/cpuinfo", "r");
  if (!fd) {
    fprintf(stderr, "failed to get cpu frequency\n");
    perror(NULL);
    return freq;
  }

  while (getline(&line, &len, fd) != EOF) {
    if (sscanf(line, "cpu MHz\t: %f", &freqf) == 1) {
      freqf = freqf * 1000000UL;
      freq = (uint64_t) freqf;
      break;
    }
  }

  fclose(fd);
  return freq;
}

unsigned long time_diff(struct timeval* start, struct timeval* stop) {
  unsigned long sec_res = stop->tv_sec - start->tv_sec;
  unsigned long usec_res = stop->tv_usec - start->tv_usec;
  return 1000000 * sec_res + usec_res;
}

static pid_t gettid(void) {
  return syscall(__NR_gettid);
}

void set_affinity(int tid, int core_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(core_id, &mask);

  int r = sched_setaffinity(tid, sizeof(mask), &mask);
  if (r < 0) {
    fprintf(stderr, "#WARN!! Couldn't set affinity for %d on %d\n", tid,
            core_id);
  }
}

void init_array(double *array, int size) {
  int i;
  for (i = 0; i < size; ++i) {
    array[i] = 0.0;
  }
}

void * doWork(void *pdata) {
  struct thread_data *tn = pdata;

  struct timeval tstart, tend;
  double throughput;

  int tid;

  /** Set thread affinity **/
  tid = gettid();
  set_affinity(tid, tn->assigned_core);

  /**
   Makes sure that arrays are on different pages to prevent possible page sharing. Only usefull for small arrays.
   Use large pages if we can to reduce the TLB impact on performance (mostly useful when measuring latency)
   **/
  uint64_t * memory_to_access;
  /*
   #ifdef MADV_HUGEPAGE
   if(use_large_pages) {
   size_t hpage_size = get_hugepage_size();

   if(!hpage_size) {
   fprintf(stderr, "(thread %lu) Cannot determine huge page size. Falling back to regular pages\n", tn->thread_no);
   assert(posix_memalign((void**)&memory_to_access, sysconf(_SC_PAGESIZE), memory_size) == 0);
   }
   else {
   assert(posix_memalign((void**)&memory_to_access, get_hugepage_size(), memory_size) == 0);
   if(madvise(memory_to_access, memory_size, MADV_HUGEPAGE)) {
   fprintf(stderr, "(thread %lu) Cannot use large pages.\n", tn->thread_no);
   }
   }
   }
   else {
   assert(posix_memalign((void**)&memory_to_access, sysconf(_SC_PAGESIZE), memory_size) == 0);
   }
   #else*/
  assert(
      posix_memalign((void**) &memory_to_access, sysconf(_SC_PAGESIZE),
                     memory_size) == 0);
//#endif

  unsigned long length;
  int initialized = 0;

  for (int i = 0; i < 10; i++) {

    if (!initialized) {
      memset(memory_to_access, 0, memory_size);
      /** wait everyone to set data and thread affinity and initialize **/
      if (pthread_barrier_wait(&barrier) == PTHREAD_BARRIER_SERIAL_THREAD) {
        //printf("threads initialized memory! let's go!\n");
      }
      if (tn->thread_no == 0) {
        unstickymem_start();
      }
      initialized = 1;
    }

    if (pthread_barrier_wait(&barrier) == PTHREAD_BARRIER_SERIAL_THREAD) {
      //printf("starting new iteration..............\n");
    }
    gettimeofday(&tstart, NULL);
    uint64_t bytes = bench_seq_read(memory_to_access, memory_size, bench_time,
                                    tn->thread_no);
    nb_bytes_processed[tn->thread_no] = bytes;
    gettimeofday(&tend, NULL);

    length = time_diff(&tstart, &tend);
    int barrier_id = pthread_barrier_wait(&barrier);
    assert(barrier_id == 0 || barrier_id == PTHREAD_BARRIER_SERIAL_THREAD);
    if (barrier_id == PTHREAD_BARRIER_SERIAL_THREAD) {
    }
    //printf("iteration concluded in %ldms\n", length / 1000);
  }

  throughput = ((double) nb_bytes_processed[tn->thread_no] / 1024. / 1024.)
      / ((double) length / 1000000.);

  //aggregate cores throughput
  pthread_mutex_lock(&running_mutex);
  node_bw[numa_node_of_cpu(tn->assigned_core)] += throughput;
  pthread_mutex_unlock(&running_mutex);

  printf("%lu\t%.2f\n", tn->assigned_core, throughput);

  //numa_free(data_shared, sizeof(double) * size_per_thread);
  free(memory_to_access);
  free(pdata);
  return NULL;
}

void usage(char * app_name) {
  fprintf(
      stderr,
      "Usage: %s -x <array size multiple of cache size> -c <list of cores> -s <STEP SIZE> -r [PR_SH_RATIO] -t [Iterations]\n",
      app_name);
  fprintf(stderr, "\t-x: array size: multiple of cache size:\n");

  fprintf(
      stderr,
      "\t-c: list of cores separated by commas or dashes (e.g, -c 0-7,9,15-20)\n");
  fprintf(stderr, "\t-s: STEP SIZE\n");
  fprintf(stderr, "\t-r: PR_SH_RATIO\n");
  fprintf(stderr, "\t-t: Iterations\n");
  fprintf(stderr, "\t-h: display usage\n");
  exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {

  int i, x;

  //unstickymem_nop();

  ncpus = get_nprocs();
  nnodes = numa_num_configured_nodes();
  ncpus_per_node = ncpus / nnodes;
  printf("===========================================\n");
  printf("| nodes - %d: cpus - %d: cpus_per_node - %d |\n", nnodes, ncpus,
         ncpus_per_node);
  printf("===========================================\n");

  int current_buf_size = ncpus;
  cores = (int*) malloc(current_buf_size * sizeof(int));

  //initialize the node aggregate total bw
  for (i = 0; i < 8; i++) {
    node_bw[i] = 0;
  }

  /** Parsing options **/
  opterr = 0;
  int c;

  while ((c = getopt(argc, argv, "rhc:x:t:s:")) != -1) {
    char * result = NULL;
    char * end_str = NULL;
    switch (c) {
      case 'c':
        result = strtok_r(optarg, ",", &end_str);
        while (result != NULL) {
          char * end_str2;
          int prev = -1;

          char * result2 = strtok_r(result, "-", &end_str2);
          while (result2 != NULL) {
            if (prev < 0) {
              prev = atoi(result2);
              /* Add to cores array */
              if (++nthreads > current_buf_size) {
                current_buf_size += ncpus;
                cores = realloc(cores, current_buf_size * sizeof(int));
                assert(cores);
              }
              cores[nthreads - 1] = prev;
              if (cores[nthreads - 1] < 0 || cores[nthreads - 1] >= ncpus) {
                fprintf(
                    stderr,
                    "%d is not a valid core number. Must be comprised between 0 and %d\n",
                    cores[nthreads - 1], ncpus - 1);
                exit(EXIT_FAILURE);
              }
            } else {
              int i;
              int core = atoi(result2);
              if (prev > core) {
                fprintf(stderr, "%d-%d is not a valid core range\n", prev,
                        core);
                exit(EXIT_FAILURE);
              }
              for (i = prev + 1; i <= core; i++) {
                /* Add to cores array */
                if (++nthreads > current_buf_size) {
                  current_buf_size += ncpus;
                  cores = realloc(cores, current_buf_size * sizeof(int));
                  assert(cores);
                }
                cores[nthreads - 1] = i;
                if (cores[nthreads - 1] < 0 || cores[nthreads - 1] >= ncpus) {
                  fprintf(
                      stderr,
                      "%d is not a valid core number. Must be comprised between 0 and %d\n",
                      cores[nthreads - 1], ncpus - 1);
                  exit(EXIT_FAILURE);
                }
              }
              prev = core;
            }
            result2 = strtok_r(NULL, "-", &end_str2);
          }
          result = strtok_r(NULL, ",", &end_str);
        }
        break;
      case 's':
        STEP = atoi(optarg);
        if (STEP < -1) {
          die("%d is not a valid STEP size (step size >= 0)", STEP);
        }
        break;
      case 'x':
        x = atoi(optarg);
        if (x <= 0) {
          die("%d is not a valid array size (array size > 0)", x);
        }
        break;
      case 'r':
        PR_SH_RATIO = atoi(optarg);
        break;
      case 't':
        iterations = atoi(optarg);
        if (iterations <= 0) {
          die("%d is not a valid iteration (iterations > 0)", iterations);
        }
        break;
      case 'h':
        usage(argv[0]);
      case '?':
        if (optopt == 'c' || optopt == 's' || optopt == 'x' || optopt == 'r')
          fprintf(stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint(optopt))
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
        usage(argv[0]);
      default:
        usage(argv[0]);
    }
  }

  if (nthreads == 0) {
    fprintf(
        stderr, "Some options are not properly filled (%s)\n\n",
        nthreads == 0 ? "Cores are not specified" : "Array Size should be > 0");
    usage(argv[0]);
  }

  //size_per_thread = (cacheSize * x);
  memory_size = DEFAULT_MEMORY_BENCH_SIZE_TO_BENCH;

  /** Scale the bench_time in cycles */
  bench_time = bench_time * get_cpu_freq();

  printf("size: %d\titer: %d\tRATIO: %d\tSTEP: %d\tnum_threads: %d\n",
         size_per_thread, iterations, PR_SH_RATIO, STEP, nthreads);

  /** Allocation stuff **/
  nb_bytes_processed = calloc(nthreads, sizeof(uint64_t));
  threads = (pthread_t *) malloc(nthreads * sizeof(pthread_t));
  assert(nb_bytes_processed);
  assert(threads);

  pthread_barrier_init(&barrier, NULL, nthreads);

  //Machine Information
  node_to_cores = calloc(nnodes, sizeof(*node_to_cores));
  core_to_node = calloc(ncpus, sizeof(*core_to_node));
  node_to_used_memory = calloc(nnodes, sizeof(*node_to_used_memory));

  //unstickymem_start();
  /* 1. Create a thread for each core specified on the command line */
  struct thread_data * pdata;

  for (i = 0; i < nthreads; ++i) {
    pdata = malloc(sizeof(struct thread_data));
    assert(pdata);
    pdata->assigned_core = cores[i];
    pdata->thread_no = i;

    assert(pthread_create(&threads[i], NULL, doWork, (void *) pdata) == 0);
  }

  for (i = 0; i < nthreads; ++i) {
    pthread_join(threads[i], NULL);
  }

  pthread_barrier_destroy(&barrier);

  printf("Node's bytes processed (MB/s)!\n");
  double total = 0;
  for (i = 0; i < 8; i++) {
    printf("%d\t%.2f\n", i, node_bw[i]);
    total += node_bw[i];
  }
  printf("\t[TOTAL] throughput: %.2f\n", total);

  return 0;
}
