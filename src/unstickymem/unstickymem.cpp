#include <unistd.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <numa.h>
#include <numaif.h>

#include <cstdio>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <boost/interprocess/shared_memory_object.hpp>

#include "unstickymem/unstickymem.h"
#include "unstickymem/PerformanceCounters.hpp"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/Logger.hpp"
#include "unstickymem/wrap.hpp"
#include "unstickymem/Runtime.hpp"
#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/mode/Mode.hpp"

// hold the nodes information ids and weights
RECORD nodes_info[MAX_NODES];
// sum of worker nodes weights
double sum_ww = 0;
// sum of non-worker nodes weights
double sum_nww = 0;
// number of workers
static bool OPT_NUM_WORKERS = false;
int OPT_NUM_WORKERS_VALUE = 1;

namespace unstickymem {

static bool is_initialized = false;
thread_local static bool inside_unstickymem = false;
Runtime *runtime;
MemoryMap *memory;

void read_config(void) {
  OPT_NUM_WORKERS = std::getenv("UNSTICKYMEM_WORKERS") != nullptr;
  if (OPT_NUM_WORKERS) {
    OPT_NUM_WORKERS_VALUE = std::stoi(std::getenv("UNSTICKYMEM_WORKERS"));
  }
}

void print_config(void) {
  LINFOF("num_workers: %s",
         OPT_NUM_WORKERS ? std::to_string(OPT_NUM_WORKERS_VALUE).c_str() : "no");
}

// library initialization
__attribute__((constructor)) void libunstickymem_initialize(void) {
  LDEBUG("Initializing");

  // initialize pointers to wrapped functions
  unstickymem::init_real_functions();

  // initialize likwid
  //initialize_likwid();

  // parse and display the configuration
  //read_config();
  //print_config();

  //set sum_ww & sum_nww & initialize the weights!
  //get_sum_nww_ww(OPT_NUM_WORKERS_VALUE);

  // set default memory policy to interleaved
  /*LDEBUG("Setting default memory policy to interleaved");
   set_mempolicy(MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
   numa_get_mems_allowed()->size + 1);*/

  // remove the previous unstickymem library segment (if exists)
  // boost::interprocess::shared_memory_object::remove("unstickymem");
  // initialize the memory
  memory = &MemoryMap::getInstance();

  // start the runtime
  runtime = &Runtime::getInstance();

  is_initialized = true;
  LDEBUG("Initialized");
}

// library destructor
__attribute((destructor)) void libunstickymem_finalize(void) {
  // cleanup shared memory object
  // boost::interprocess::shared_memory_object::remove("unstickymem");

  //get the elapsed stall_rate
  //double stall_rate = get_elapsed_stall_rate();
  //unstickymem_log_v1(stall_rate, runtime->_mode_name);

  // stop all the counters
  //stop_all_counters();
  LINFO("Finalized");
}

}  // namespace unstickymem

#ifdef __cplusplus
extern "C" {
#endif

int check_sum(RECORD n_i[MAX_NODES]) {
  double sum = 0;
  int i = 0;

  for (i = 0; i < MAX_NODES; i++) {
    sum += n_i[i].weight;
  }
  return std::lround(sum);
}

void unstickymem_nop(void) {
  LDEBUG("unstickymem NO-OP!");
}

void unstickymem_start(void) {
  LDEBUG("Starting the unstickymem thread!");
  unstickymem::runtime->startSelectedMode();
}

void unstickymem_print_memory(void) {
  unstickymem::memory->print();
}

void read_weights(char filename[]) {
  FILE * fp;
  char * line = NULL;
  size_t len = 0;
  ssize_t read;

  const char s[2] = " ";
  char *token;

  int retcode;
  // First sort the file if not sorted
  char cmdbuf[256];
  snprintf(cmdbuf, sizeof(cmdbuf), "sort -n -o %s %s", filename, filename);
  retcode = system(cmdbuf);
  if (retcode == -1) {
    printf("Unable to sort the weights!");
    exit (EXIT_FAILURE);
  }

  int j = 0;

  fp = fopen(filename, "r");
  if (fp == NULL) {
    printf("Weights have not been provided!\n");
    exit (EXIT_FAILURE);
  }

  while ((read = getline(&line, &len, fp)) != -1) {
    char *strtok_saveptr;
    // printf("Retrieved line of length %zu :\n", read);
    // printf("%s", line);

    // get the first token
    token = strtok_r(line, s, &strtok_saveptr);
    nodes_info[j].weight = atof(token);
    // printf(" %s\n", token);

    // get the second token
    token = strtok_r(NULL, s, &strtok_saveptr);
    nodes_info[j].id = atoi(token);
    // printf(" %s\n", token);
    j++;
  }

  /* int i;
   printf("Initial Weights:\t");
   for (i = 0; i < MAX_NODES; i++) {
   printf("id: %d w: %.1f\t", nodes_info[i].id, nodes_info[i].weight);
   }
   printf("\n");*/

  fclose(fp);
  if (line)
    free(line);

  LINFO("weights initialized!");

  return;
}

void get_sum_nww_ww(int num_workers) {
  int i;

  if (num_workers == 1) {
    //workers: 0
    char weights[] = "/home/dgureya/devs/unstickymem/config/weights_1w.txt";
    read_weights(weights);
    //printf("Worker Nodes:\t");
    LDEBUG("Worker Nodes: 0");
    for (i = 0; i < MAX_NODES; i++) {
      if (nodes_info[i].id == 0) {
        //printf("nodes_info[%d].id=%d", i, nodes_info[i].id);
        sum_ww += nodes_info[i].weight;
      } else {
        sum_nww += nodes_info[i].weight;
      }
    }
  } else if (num_workers == 2) {
    //workers: 0,1
    char weights[] = "/home/dgureya/devs/unstickymem/config/weights_2w.txt";
    read_weights(weights);
    //printf("Worker Nodes:\t");
    LDEBUG("Worker Nodes: 0,1");
    for (i = 0; i < MAX_NODES; i++) {
      if (nodes_info[i].id == 0 || nodes_info[i].id == 1) {
        //printf("nodes_info[%d].id=%d\t", i, nodes_info[i].id);
        sum_ww += nodes_info[i].weight;
      } else {
        sum_nww += nodes_info[i].weight;
      }
    }
  } else if (num_workers == 3) {
    //workers: 1,2,3
    char weights[] = "/home/dgureya/devs/unstickymem/config/weights_3w.txt";
    read_weights(weights);
    //printf("Worker Nodes:\t");
    LDEBUG("Worker Nodes: 1,2,3");
    for (i = 0; i < MAX_NODES; i++) {
      if (nodes_info[i].id == 1 || nodes_info[i].id == 2
          || nodes_info[i].id == 3) {
        //printf("nodes_info[%d].id=%d\t", i, nodes_info[i].id);
        sum_ww += nodes_info[i].weight;
      } else {
        sum_nww += nodes_info[i].weight;
      }
    }
  } else if (num_workers == 4) {
    //workers: 0,1,2,3
    char weights[] = "/home/dgureya/devs/unstickymem/config/weights_4w.txt";
    read_weights(weights);
    //printf("Worker Nodes:\t");
    LDEBUG("Worker Nodes: 0,1,2,3");
    for (i = 0; i < MAX_NODES; i++) {
      if (nodes_info[i].id == 0 || nodes_info[i].id == 1
          || nodes_info[i].id == 2 || nodes_info[i].id == 3) {
        //printf("nodes_info[%d].id=%d\t", i, nodes_info[i].id);
        sum_ww += nodes_info[i].weight;
      } else {
        sum_nww += nodes_info[i].weight;
      }
    }
  } else if (num_workers == 8) {
    //workers: all
    char weights[] = "/home/dgureya/devs/unstickymem/config/weights_8w.txt";
    read_weights(weights);
    //printf("Worker Nodes:\t");
    LDEBUG("Worker Nodes: 0,1,2,3");
    for (i = 0; i < MAX_NODES; i++) {
      if (nodes_info[i].id == 0 || nodes_info[i].id == 1
          || nodes_info[i].id == 2 || nodes_info[i].id == 3) {
        //printf("nodes_info[%d].id=%d\t", i, nodes_info[i].id);
        sum_ww += nodes_info[i].weight;
      } else {
        sum_nww += nodes_info[i].weight;
      }
    }
  } else {
    LDEBUGF("Sorry, %d workers is not supported at the moment!", num_workers);
    exit (EXIT_FAILURE);
  }

  if ((int) round((sum_nww + sum_ww)) != 100) {
    LDEBUGF(
        "Sum of WW and NWW must be equal to 100! WW=%.2f\tNWW=%.2f\tSUM=%.2f\n",
        sum_ww, sum_nww, sum_nww + sum_ww);
    exit(-1);
  } else {
    LDEBUGF("WW = %.2f\tNWW = %.2f\n", sum_ww, sum_nww);
  }

  return;
}

// Wrapped functions

void *malloc(size_t size) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void* (*)(size_t)) dlsym(RTLD_NEXT, "malloc"))(size);
  }

  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_malloc(size);
  unstickymem::inside_unstickymem = false;
  LTRACEF("malloc(%zu) => %p", size, result);
  return result;
}

// XXX this is a hack XXX
// this is to solve the recursion in calloc -> dlsym -> calloc -> ...
#define DLSYM_CALLOC_BUFFER_LENGTH 1024*1024
thread_local static unsigned char calloc_buffer[DLSYM_CALLOC_BUFFER_LENGTH];
thread_local static bool calloc_buffer_in_use = false;

void *calloc(size_t nmemb, size_t size) {
  thread_local static bool inside_dlsym = false;

  // XXX beware: ugly hack! XXX
  // check if we are inside dlsym -- return a temporary buffer for it!
  if (inside_dlsym) {
    DIEIF(calloc_buffer_in_use, "calling dlsym requires more buffers");
    calloc_buffer_in_use = true;
    memset(calloc_buffer, 0, DLSYM_CALLOC_BUFFER_LENGTH);
    return calloc_buffer;
  }

  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    inside_dlsym = true;
    void *result = ((void* (*)(size_t, size_t)) dlsym(RTLD_NEXT, "calloc"))(
        nmemb, size);
    inside_dlsym = false;
    return result;
  }

  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_calloc(nmemb, size);
  unstickymem::inside_unstickymem = false;
  LTRACEF("calloc(%zu, %zu) => %p", nmemb, size, result);
  return result;
}

void *realloc(void *ptr, size_t size) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void *(*)(void*, size_t)) dlsym(RTLD_NEXT, "realloc"))(ptr, size);
  }

  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_realloc(ptr, size);
  LTRACEF("realloc(%p, %zu) => %p", ptr, size, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

void *reallocarray(void *ptr, size_t nmemb, size_t size) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void *(*)(void*, size_t, size_t)) dlsym(RTLD_NEXT, "reallocarray"))(
        ptr, nmemb, size);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_reallocarray(ptr, nmemb, size);
  LTRACEF("reallocarray(%p, %zu, %zu) => %p", ptr, nmemb, size, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

void free(void *ptr) {
  // check if this is the temporary buffer passed to dlsym (see calloc)
  if (ptr == calloc_buffer) {
    calloc_buffer_in_use = false;
    return;
  }
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void (*)(void*)) dlsym(RTLD_NEXT, "free"))(ptr);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  unstickymem::memory->handle_free(ptr);
  LTRACEF("free(%p)", ptr);
  unstickymem::inside_unstickymem = false;
}

int posix_memalign(void **memptr, size_t alignment, size_t size) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((int (*)(void**, size_t, size_t)) dlsym(RTLD_NEXT, "posix_memalign"))(
        memptr, alignment, size);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  int result = unstickymem::memory->handle_posix_memalign(memptr, alignment,
                                                          size);
  LTRACEF("posix_memalign(%p, %zu, %zu) => %d", memptr, alignment, size, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

void *mmap(void *addr, size_t length, int prot, int flags, int fd,
           off_t offset) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void *(*)(void*, size_t, int, int, int, off_t)) dlsym(RTLD_NEXT,
                                                                   "mmap"))(
        addr, length, prot, flags, fd, offset);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_mmap(addr, length, prot, flags, fd,
                                                  offset);
  LTRACEF("mmap(%p, %zu, %d, %d, %d, %d) => %p", addr, length, prot, flags, fd,
          offset, result);
  unstickymem::inside_unstickymem = false;
  // return the result
  return result;
}

int munmap(void *addr, size_t length) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((int (*)(void*, size_t)) dlsym(RTLD_NEXT, "munmap"))(addr, length);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  int result = unstickymem::memory->handle_munmap(addr, length);
  LTRACEF("munmap(%p, %zu) => %d", addr, length, result);
  unstickymem::inside_unstickymem = false;
  // return the result
  return result;
}

void *mremap(void *old_address, size_t old_size, size_t new_size, int flags,
             ... /* void *new_address */) {
  DIE("TODO");
}

int brk(void *addr) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((int (*)(void*)) dlsym(RTLD_NEXT, "brk"))(addr);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  int result = unstickymem::memory->handle_brk(addr);
  LTRACEF("brk(%p) => %d", addr, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

void *sbrk(intptr_t increment) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((void *(*)(intptr_t)) dlsym(RTLD_NEXT, "sbrk"))(increment);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  void *result = unstickymem::memory->handle_sbrk(increment);
  LTRACEF("sbrk(%zu) => %p", increment, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

long mbind(void *addr, unsigned long len, int mode,
           const unsigned long *nodemask, unsigned long maxnode,
           unsigned flags) {
  // dont do anything fancy if library is not initialized
  if (!unstickymem::is_initialized || unstickymem::inside_unstickymem) {
    return ((long (*)(void*, unsigned long, int, const unsigned long*,
                      unsigned long, unsigned)) dlsym(RTLD_NEXT, "mbind"))(
        addr, len, mode, nodemask, maxnode, flags);
  }
  // handle the function ourselves
  unstickymem::inside_unstickymem = true;
  long result = unstickymem::memory->handle_mbind(addr, len, mode, nodemask,
                                                  maxnode, flags);
  LTRACEF("mbind(%p, %lu, %d, %p, %lu, %u) => %ld", addr, len, mode, nodemask,
          maxnode, flags, result);
  unstickymem::inside_unstickymem = false;
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
