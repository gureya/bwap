#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>

#include <numa.h>
#include <numaif.h>

#include <thread>

#include <boost/program_options.hpp>

#include "unstickymem/PerformanceCounters.hpp"
#include "unstickymem/unstickymem.h"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/Logger.hpp"
#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/mode/AdaptiveMode.hpp"

namespace unstickymem {

static Mode::Registrar<AdaptiveMode> registrar(AdaptiveMode::name(),
                                               AdaptiveMode::description());

po::options_description AdaptiveMode::getOptions() {
  po::options_description mode_options("Adaptive mode parameters");
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
      "Time (in microseconds) between measurements");
  return mode_options;
}

void AdaptiveMode::printParameters() {
  LINFOF("UNSTICKYMEM_WAIT_START:         %lu", _wait_start);
  LINFOF("UNSTICKYMEM_NUM_POLLS:          %lu", _num_polls);
  LINFOF("UNSTICKYMEM_NUM_POLL_OUTLIERS:  %lu", _num_poll_outliers);
  LINFOF("UNSTICKYMEM_POLL_SLEEP:         %lu", _poll_sleep);
}

void AdaptiveMode::adaptiveThread() {
  // start with everything interleaved
  double local_ratio = 1.0 / numa_num_configured_nodes();
  double prev_stall_rate = std::numeric_limits<double>::infinity();
  double best_stall_rate = std::numeric_limits<double>::infinity();
  double stall_rate;

  // pin thread to core zero
  // FIXME(dgureya): is this required when using likwid? - I don't think so!
  // cpu_set_t mask;
  // CPU_ZERO(&mask);
  // CPU_SET(0, &mask);
  // DIEIF(sched_setaffinity(syscall(SYS_gettid), sizeof(mask), &mask) < 0,
  //		"could not set affinity for hw monitor thread");

  get_stall_rate_v2();
  get_elapsed_stall_rate();
  sleep(_wait_start);

  // dump mapping information
  MemoryMap &segments = MemoryMap::getInstance();
  // segments.print();

  // slowly achieve awesomeness
  for (uint64_t local_percentage = (100 / numa_num_configured_nodes() + 4) / 5
      * 5; local_percentage <= 100; local_percentage += ADAPTATION_STEP) {
    if(local_percentage % 10 == 0 && local_percentage != 100) continue;
    local_ratio = ((double) local_percentage) / 100;
    LINFOF("going to check a ratio of %3.1lf%%", local_ratio * 100);
    place_all_pages_adaptive(segments, local_ratio);
    usleep(200000);
    unstickymem_log(local_ratio);
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);
    //print stall_rate to a file for debugging!
    unstickymem_log(local_ratio, stall_rate);

    LINFOF("Ratio: %1.2lf StallRate: %1.10lf (previous %1.10lf; best %1.10lf)",
           local_ratio, stall_rate, prev_stall_rate, best_stall_rate);
    /*std::string s = std::to_string(stall_rate);
     s.replace(s.find("."), std::string(".").length(), ",");
     fprintf(stderr, "%s\n", s.c_str());*/
    // compute the minimum rate
    best_stall_rate = std::min(best_stall_rate, stall_rate);
    // check if we are getting worse
    if (stall_rate > best_stall_rate * 1.001) {
      // just make sure that its not something transient...!
      LINFO("Hmm... Is this the best we can do?");
      if (get_average_stall_rate(_num_polls * 2, _poll_sleep,
                                 _num_poll_outliers * 2)) {
        LINFO("I guess so!");
        break;
      }
    }
    prev_stall_rate = stall_rate;
  }
  LINFO("My work here is done! Enjoy the speedup");
  LINFOF("Ratio: %1.2lf", local_ratio);
  LINFOF("Stall Rate: %1.10lf", stall_rate);
  LINFOF("Best Measured Stall Rate: %1.10lf", best_stall_rate);
}

void AdaptiveMode::start() {
  // interleave memory by default
  /*LINFO("Setting default memory policy to interleaved");
  set_mempolicy(MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
                numa_get_mems_allowed()->size);*/

  // start adaptive thread
  std::thread adaptiveThread(&AdaptiveMode::adaptiveThread, this);

  // dont want for it to finish
  adaptiveThread.detach();
}

}  // namespace unstickymem
