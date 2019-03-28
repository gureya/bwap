#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>

#include <numa.h>
#include <numaif.h>

#include <thread>
#include <iostream>

#include <boost/program_options.hpp>

#include "unstickymem/unstickymem.h"
#include "unstickymem/PerformanceCounters.hpp"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/mode/ScanMode.hpp"
#include "unstickymem/Logger.hpp"

namespace po = boost::program_options;

namespace unstickymem {

static Mode::Registrar<ScanMode> registrar(ScanMode::name(),
                                           ScanMode::description());

po::options_description ScanMode::getOptions() {
  po::options_description mode_options("Scan mode parameters");
  mode_options.add_options()(
      "UNSTICKYMEM_WAIT_START",
      po::value<unsigned int>(&_wait_start)->default_value(2),
      "Time (in seconds) to wait before starting scan")(
      "UNSTICKYMEM_NUM_POLLS",
      po::value<unsigned int>(&_num_polls)->default_value(20),
      "How many measurements to make for each placement ratio")(
      "UNSTICKYMEM_NUM_POLL_OUTLIERS",
      po::value<unsigned int>(&_num_poll_outliers)->default_value(5),
      "How many of the top-N and bottom-N measurements to discard")(
      "UNSTICKYMEM_POLL_SLEEP",
      po::value < useconds_t > (&_poll_sleep)->default_value(200000),
      "Time (in microseconds) between measurements")(
      "UNSTICKYMEM_EXIT_WHEN_FINISHED",
      po::value<bool>(&_exit_when_finished)->default_value(true),
      "Time (in microseconds) between measurements");
  return mode_options;
}

void ScanMode::printParameters() {
  LINFOF("UNSTICKYMEM_WAIT_START:         %lu", _wait_start);
  LINFOF("UNSTICKYMEM_NUM_POLLS:          %lu", _num_polls);
  LINFOF("UNSTICKYMEM_NUM_POLL_OUTLIERS:  %lu", _num_poll_outliers);
  LINFOF("UNSTICKYMEM_POLL_SLEEP:         %lu", _poll_sleep);
  LINFOF("UNSTICKYMEM_EXIT_WHEN_FINISHED: %s",
         _exit_when_finished ? "Yes" : "No");
}

void ScanMode::processSegmentAddition(const MemorySegment& segment) {

  if (segment.length() > (1UL << 14)) {
    //segment.print();
    place_pages_weighted_initial(segment);
  }

}

void ScanMode::scannerThread() {
  // start with everything interleaved
  //double prev_stall_rate = std::numeric_limits<double>::infinity();
  //double best_stall_rate = std::numeric_limits<double>::infinity();
  //double stall_rate;

  // pin thread to core zero
  // FIXME(dgureya): is this required when using likwid? - I don't think so
  // cpu_set_t mask;
  // CPU_ZERO(&mask);
  // CPU_SET(0, &mask);
  // DIEIF(sched_setaffinity(syscall(SYS_gettid), sizeof(mask), &mask) < 0,
  //		"could not set affinity for hw monitor thread");

  //get_stall_rate_v2();
  //sleep(_wait_start);

  // dump mapping information
  //MemoryMap &segments = MemoryMap::getInstance();
  //segments.print();
  //set sum_ww & sum_nww & initialize the weights!
  //get_sum_nww_ww(OPT_NUM_WORKERS_VALUE);

  /*int i;
   for (i = 0; i <= sum_nww; i += ADAPTATION_STEP) {
   LINFOF("checking ratio %d", i);
   place_all_pages(segments, i);
   sleep(1);
   unstickymem_log(i);
   stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
   _num_poll_outliers);
   //print stall_rate to a file for debugging!
   unstickymem_log(i, stall_rate);
   LINFOF("Ratio: %d StallRate: %1.10lf (previous %1.10lf; best %1.10lf)", i,
   stall_rate, prev_stall_rate, best_stall_rate);
   // compute the minimum rate
   best_stall_rate = std::min(best_stall_rate, stall_rate);
   prev_stall_rate = stall_rate;
   }*/

  if (_exit_when_finished) {
    exit(0);
  }
}

void ScanMode::start() {
  // set default memory policy to interleaved
  /*LDEBUG("Setting default memory policy to interleaved");
   set_mempolicy(MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
   numa_get_mems_allowed()->size);*/

  // start scanner thread
  std::thread scanThread(&ScanMode::scannerThread, this);

  // dont wait for it to finish
  scanThread.detach();
}

}  // namespace unstickymem
