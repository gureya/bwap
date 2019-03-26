#include <cstdint>
#include <cstddef>
#include <iostream>
#include <cstring>
#include <vector>
#include <algorithm>
#include <numeric>
#include "unstickymem/unstickymem.h"
#include <unstickymem/PerformanceCounters.hpp>
#include <unstickymem/Logger.hpp>

#include <likwid.h>

#include <numa.h>
#include <numaif.h>

//format specifiers for the intN_t types
#include <inttypes.h>

namespace unstickymem {

static bool initiatialized = false;
static FILE *f = fopen("/home/dgureya/devs/unstickymem/unstickymem_log.txt",
                       "a");
static FILE *f_1 = fopen("/home/dgureya/devs/unstickymem/elapsed_stall_rate_log.txt",
                        "a");

//output stall rate to a log file
void unstickymem_log(double ratio, double sr) {
  DIEIF(f == nullptr, "error opening file");
  fprintf(f, "%1.2lf %1.10lf\n", ratio, sr);
}

void unstickymem_log(double ratio) {
  DIEIF(f == nullptr, "error opening file");
  fprintf(f, "Stall rates for %1.2lf\n", ratio);
}

void unstickymem_log_v1(double sr, std::string mode) {
  DIEIF(f_1 == nullptr, "error opening file");
  fprintf(f_1, "%s: elapsed stall rate\t%1.2lf\n", mode.c_str(), sr);
}


/*
 * A function that uses the likwid library to measure the stall rates
 *
 * On AMD we use the following counters
 * EventSelect 0D1h Dispatch Stalls: The number of processor cycles where the decoder
 * is stalled for any reason (has one or more instructions ready but can't dispatch
 * them due to resource limitations in execution)
 * &
 * EventSelect 076h CPU Clocks not Halted: The number of clocks that the CPU is not in a halted state.
 *
 * On Intel we use the following counters
 * RESOURCE_STALLS: Cycles Allocation is stalled due to Resource Related reason
 * &
 * UNHALTED_CORE_CYCLES:  Count core clock cycles whenever the clock signal on the specific
 * core is running (not halted)
 *
 */
int err;
int* cpus;
int gid;
CpuInfo_t info;
static int nnodes;
static int ncpus_per_node;
static int ncpus;
static int active_cpus;

//list of all the events for the different architectures supported
//char amd_estr[] = "CPU_CLOCKS_UNHALTED:PMC0,DISPATCH_STALLS:PMC1"; //AMD
char amd_estr[] = "DISPATCH_STALLS:PMC0";  //AMD DISPATCH_STALL_LDQ_FULL,DISPATCH_STALL_FP_SCHED_Q_FULL
//char amd_estr[] = "DISPATCH_STALL_INSTRUCTION_RETIRED_Q_FULL:PMC0";
//char intel_estr[] =
//		"CPU_CLOCK_UNHALTED_THREAD_P:PMC0,RESOURCE_STALLS_ANY:PMC1"; //Intel Broadwell EP
char intel_estr[] = "RESOURCE_STALLS_ANY:PMC0";  //Intel Broadwell EP

void initialize_likwid() {
  if (!initiatialized) {
    //perfmon_setVerbosity(3);
    //Load the topology module and print some values.
    err = topology_init();
    if (err < 0) {
      LDEBUG("Failed to initialize LIKWID's topology module\n");
      //return 1;
      exit(-1);
    }
    // CpuInfo_t contains global information like name, CPU family, ...
    //CpuInfo_t info = get_cpuInfo();
    info = get_cpuInfo();
    // CpuTopology_t contains information about the topology of the CPUs.
    CpuTopology_t topo = get_cpuTopology();
    // Create affinity domains. Commonly only needed when reading Uncore counters
    affinity_init();

    LINFOF("Likwid Measuremennts on a %s with %d CPUs\n", info->name,
           topo->numHWThreads);

    ncpus = topo->numHWThreads;
    nnodes = numa_num_configured_nodes();
    ncpus_per_node = ncpus / nnodes;

    //active_cpus = OPT_NUM_WORKERS_VALUE * ncpus_per_node;
    OPT_NUM_WORKERS_VALUE=3;
    active_cpus = 1;

    LINFOF(
        "| [NODES] - %d: [CPUS] - %d: [CPUS_PER_NODE] - %d: [NUM_WORKERS] - %d: [ACTIVE_CPUS] - %d |\n",
        nnodes, ncpus, ncpus_per_node, OPT_NUM_WORKERS_VALUE, active_cpus);

    //cpus = (int*) malloc(topo->numHWThreads * sizeof(int));
    //for now only monitor one CPU
    cpus = (int*) malloc(active_cpus * sizeof(int));
   
    if (!cpus)
      exit(-1);		//return 1;

    if (OPT_NUM_WORKERS_VALUE == 3) {
    for (int i = 0; i < active_cpus; i++) {
      cpus[i] = 8;
      //printf("cpu[%d]\n", cpus[i]);
      //printf("threadpool[%d]", topo->threadPool[i].apicId);
      //exit(-1);
    }
  } else {
    for (int i = 0; i < active_cpus; i++) {
      cpus[i] = topo->threadPool[i].apicId;
      //printf("threadpool[%d]\n", topo->threadPool[i].apicId);
      //printf("cpu[%d]", cpus[i]);
      //exit(-1);
    }
  }

    // Must be called before perfmon_init() but only if you want to use another
    // access mode as the pre-configured one. For direct access (0) you have to
    // be root.
    //accessClient_setaccessmode(0);
    // Initialize the perfmon module.
    //err = perfmon_init(topo->numHWThreads, cpus);
    err = perfmon_init(active_cpus, cpus);
    if (err < 0) {
      LDEBUG("Failed to initialize LIKWID's performance monitoring module\n");
      topology_finalize();
      //return 1;
      exit(-1);
    }

    /*
     * pick the right event based on the architecture,
     * currently tested on AMD {amd64_fam15h_interlagos && amd64_fam10h_istanbul}
     * and INTEL {Intel Broadwell EP}
     * uses a simple flag to do this, may use the more accurate cpu names or families
     *
     */
    LINFOF("Short name of the CPU: %s\n", info->short_name);
    LINFOF("Intel flag: %d\n", info->isIntel);
    LINFOF("CPU family ID: %" PRIu32 "\n", info->family);
    // Add eventset string to the perfmon module.
    //Intel CPU's
    if (info->isIntel == 1) {
      LINFOF("Setting up events %s for %s\n", intel_estr, info->short_name);
      gid = perfmon_addEventSet(intel_estr);
    }
    //for AMD!
    else if (info->isIntel == 0) {
      LINFOF("Setting up events %s for %s\n", amd_estr, info->short_name);
      gid = perfmon_addEventSet(amd_estr);
    } else {
      LINFO("Unsupported Architecture at the moment\n");
      exit(-1);
    }

    if (gid < 0) {
      LDEBUGF(
          "Failed to add event string %s to LIKWID's performance monitoring module\n",
          intel_estr);
      perfmon_finalize();
      topology_finalize();
      //return 1;
      exit(-1);
    }

    // Setup the eventset identified by group ID (gid).
    err = perfmon_setupCounters(gid);
    if (err < 0) {
      LDEBUGF(
          "Failed to setup group %d in LIKWID's performance monitoring module\n",
          gid);
      perfmon_finalize();
      topology_finalize();
      //return 1;
      exit(-1);
    }

    // Start all counters in the previously set up event set.
    err = perfmon_startCounters();
    if (err < 0) {
      LDEBUGF("Failed to start counters for group %d for thread %d\n", gid,
              (-1 * err) - 1);
      perfmon_finalize();
      topology_finalize();
      exit(-1);
      //return 1;
    }
    initiatialized = true;
    //printf("Setting up Likwid statistics for the first time\n");
  }

}

double get_elapsed_stall_rate() {
  int i, j;
  double result = 0.0;

  //static double prev_cycles = 0;
  static double elapsed_stalls = 0;
  static uint64_t elapsed_clockcounts = 0;

  // Stop all counters in the previously started event set before doing a read.
  err = perfmon_stopCounters();
  if (err < 0) {
    LDEBUGF("Failed to stop counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }

  // Read the result of every thread/CPU for all events in estr.
  // For now just read/print for the active cores only, actually just one core at the moment!
  // double cycles = 0;
  double stalls = 0;
  j = 0;
  // char* ptr = NULL;
  //Results depending on the architecture!
  if (info->isIntel == 1) {
    //ptr = strtok(intel_estr, ",");
    // ptr = intel_estr;
  } else if (info->isIntel == 0) {
    //ptr = strtok(amd_estr, ",");
    // ptr = amd_estr;
  } else {
    LDEBUG(
        "Error: Something went wrong, can't get the measurements at the moment!\n");
    exit(-1);
  }

  //while (ptr != NULL) {
  //for (i = 0; i < active_cpus; i++) {
    //result = perfmon_getResult(gid, j, i);
    //if (j == 0) {
    //  cycles += result;
    //} else {
    //stalls += result;
    //}
    //printf("Measurement result for event set %s at CPU %d: %f\n", ptr,
    //    cpus[i], result);
  //}
  //  ptr = strtok(NULL, ",");
  //  j++;
  //}
   if (OPT_NUM_WORKERS_VALUE == 3) {
    result = perfmon_getResult(gid, 0, 0);
    stalls += result;
  } else {
    result = perfmon_getResult(gid, 0, 0);
    stalls += result;
  }


  uint64_t clock = readtsc();  // read clock
  //double stall_rate = (stalls - prev_stalls) / (cycles - prev_cycles);
  stalls = stalls / active_cpus;
  double stall_rate = ((double) (stalls - elapsed_stalls))
      / (clock - elapsed_clockcounts);

  //printf("clock: %" PRIu64 " prev_clockcounts: %" PRIu64 " clock - prev_clockcounts: %" PRIu64 "\n", clock, prev_clockcounts, (clock - prev_clockcounts));
  //printf("stalls: %.0f prev_stalls: %.0f stalls - prev_stalls: %.0f\n",
  //    stalls, prev_stalls, (stalls - prev_stalls));
  //printf("stall_rate: %f\n", stall_rate);

  //prev_cycles = cycles;
  elapsed_stalls = stalls;
  elapsed_clockcounts = clock;

  err = perfmon_startCounters();
  if (err < 0) {
    LDEBUGF("Failed to start counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    exit(-1);
    //return 1;
  }

  return stall_rate;
}


double get_stall_rate_v2() {
  int i, j;
  double result = 0.0;

  //static double prev_cycles = 0;
  static double prev_stalls = 0;
  static uint64_t prev_clockcounts = 0;

  // Stop all counters in the previously started event set before doing a read.
  err = perfmon_stopCounters();
  if (err < 0) {
    LDEBUGF("Failed to stop counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }

  // Read the result of every thread/CPU for all events in estr.
  // For now just read/print for the active cores only, actually just one core at the moment!
  // double cycles = 0;
  double stalls = 0;
  j = 0;
  // char* ptr = NULL;
  //Results depending on the architecture!
  if (info->isIntel == 1) {
    //ptr = strtok(intel_estr, ",");
    // ptr = intel_estr;
  } else if (info->isIntel == 0) {
    //ptr = strtok(amd_estr, ",");
    // ptr = amd_estr;
  } else {
    LDEBUG(
        "Error: Something went wrong, can't get the measurements at the moment!\n");
    exit(-1);
  }

  //while (ptr != NULL) {
  //for (i = 0; i < active_cpus; i++) {
    //result = perfmon_getResult(gid, j, i);
    //if (j == 0) {
    //	cycles += result;
    //} else {
    //stalls += result;
    //}
    //printf("Measurement result for event set %s at CPU %d: %f\n", ptr,
    //		cpus[i], result);
  //}
  //	ptr = strtok(NULL, ",");
  //	j++;
  //}
  if (OPT_NUM_WORKERS_VALUE == 3) {
    result = perfmon_getResult(gid, 0, 0);
    stalls += result;
  } else {
    result = perfmon_getResult(gid, 0, 0);
    stalls += result;
  }


  uint64_t clock = readtsc();  // read clock
  //double stall_rate = (stalls - prev_stalls) / (cycles - prev_cycles);
  stalls = stalls / active_cpus;
  double stall_rate = ((double) (stalls - prev_stalls))
      / (clock - prev_clockcounts);

  //printf("clock: %" PRIu64 " prev_clockcounts: %" PRIu64 " clock - prev_clockcounts: %" PRIu64 "\n", clock, prev_clockcounts, (clock - prev_clockcounts));
  //printf("stalls: %.0f prev_stalls: %.0f stalls - prev_stalls: %.0f\n",
  //		stalls, prev_stalls, (stalls - prev_stalls));
  //printf("stall_rate: %f\n", stall_rate);

  //prev_cycles = cycles;
  prev_stalls = stalls;
  prev_clockcounts = clock;

  err = perfmon_startCounters();
  if (err < 0) {
    LDEBUGF("Failed to start counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    exit(-1);
    //return 1;
  }

  return stall_rate;
  //return stalls;
}

void stop_all_counters() {
  err = perfmon_stopCounters();
  if (err < 0) {
    LDEBUGF("Failed to stop counters for group %d for thread %d\n", gid,
            (-1 * err) - 1);
    perfmon_finalize();
    topology_finalize();
    //return 1;
    exit(-1);
  }
  free(cpus);
  // Uninitialize the perfmon module.
  perfmon_finalize();
  affinity_finalize();
  // Uninitialize the topology module.
  topology_finalize();
  LINFO("All counters have been stopped\n");
}

// checks performance counters and computes stalls per second since last call
double get_stall_rate() {
  const int pmc_num = 0x00000000;  // program counter monitor number
  //static bool initialized = false;
  static uint64_t prev_clockcounts = 0;
  static uint64_t prev_pmcounts = 0;
  // wait a bit to get a baseline first time function is called
  /*if (!initialized) {
   prev_clockcounts = readtsc();
   prev_pmcounts = readpmc(pmc_num);
   usleep(POLL_SLEEP);
   initialized = true;
   }*/
  uint64_t clock = readtsc();
  uint64_t pmc = readpmc(pmc_num);
  double stall_rate = ((double) (pmc - prev_pmcounts))
      / (clock - prev_clockcounts);
  prev_pmcounts = pmc;
  prev_clockcounts = clock;
  return stall_rate;
}

// samples stall rate multiple times and filters outliers
double get_average_stall_rate(size_t num_measurements,
                              useconds_t usec_between_measurements,
                              size_t num_outliers_to_filter) {
  //return 0.0;
  std::vector<double> measurements(num_measurements);

  //throw away a measurement, just because
  //get_stall_rate();
  get_stall_rate_v2();
  usleep(usec_between_measurements);

  // do N measurements, T usec apart
  for (size_t i = 0; i < num_measurements; i++) {
    //measurements[i] = get_stall_rate();
    measurements[i] = get_stall_rate_v2();
    //unstickymem_log(measurements[i], i);
    usleep(usec_between_measurements);
  }

  /*for (auto m : measurements) {
   std::cout << m << " ";
   }
   std::cout << std::endl;*/

  // filter outliers
  std::sort(measurements.begin(), measurements.end());
  measurements.erase(measurements.end() - num_outliers_to_filter,
                     measurements.end());
  measurements.erase(measurements.begin(),
                     measurements.begin() + num_outliers_to_filter);

  int i = 0;
  for (auto m : measurements) {
    unstickymem_log(i, m);
    i++;
  }

  // return the average
  double sum = std::accumulate(measurements.begin(), measurements.end(), 0.0);
  return sum / measurements.size();
}

#if defined(__unix__) || defined(__linux__)
// System-specific definitions for Linux

// read time stamp counter
inline uint64_t readtsc(void) {
  uint32_t lo, hi;
  __asm __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi) : : );
  return lo | (uint64_t)hi << 32;
}

// read performance monitor counter
inline uint64_t readpmc(int32_t n) {
  uint32_t lo, hi;
  __asm __volatile__ ("rdpmc" : "=a"(lo), "=d"(hi) : "c"(n) : );
  return lo | (uint64_t)hi << 32;
}

#else  // not Linux

#error We only support Linux

#endif

}
  // namespace unstickymem
