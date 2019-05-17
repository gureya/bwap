#include <unistd.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>

#include <numa.h>
#include <numaif.h>

#include <thread>

#include <boost/program_options.hpp>

#include "unstickymem/unstickymem.h"
#include "unstickymem/PerformanceCounters.hpp"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/Logger.hpp"
#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/mode/WeightedAdaptiveMode.hpp"

namespace unstickymem {

static Mode::Registrar<WeightedAdaptiveMode> registrar(
    WeightedAdaptiveMode::name(), WeightedAdaptiveMode::description());

po::options_description WeightedAdaptiveMode::getOptions() {
  po::options_description mode_options("Weighted Adaptive mode parameters");
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

void WeightedAdaptiveMode::printParameters() {
  LINFOF("UNSTICKYMEM_WAIT_START:         %lu", _wait_start);
  LINFOF("UNSTICKYMEM_NUM_POLLS:          %lu", _num_polls);
  LINFOF("UNSTICKYMEM_NUM_POLL_OUTLIERS:  %lu", _num_poll_outliers);
  LINFOF("UNSTICKYMEM_POLL_SLEEP:         %lu", _poll_sleep);
}

void WeightedAdaptiveMode::processSegmentAddition(
    const MemorySegment& segment) {
///*  if (!_started) {
  //   return;
  // }
  if (segment.length() > (1UL << 14)) {
    //segment.print();
    //LINFOF("segment size (MB): %lu", segment.length() / (1024 * 1024));
    //place_pages_weighted_initial(segment);
  }
  //*/
}

void WeightedAdaptiveMode::adaptiveThread() {
  double prev_stall_rate = std::numeric_limits<double>::infinity();
  double best_stall_rate = std::numeric_limits<double>::infinity();
  double stall_rate;

  get_stall_rate_v2();
  //get_elapsed_stall_rate();
  sleep(_wait_start);
  // dump mapping information
  MemoryMap &segments = MemoryMap::getInstance();
  //segments.print();
  //double i = 50;
  //for (double i = 0; i <= 100; i += ADAPTATION_STEP) {
  //LINFOF("Going to check a ratio of %lf", i);
  //place_all_pages(segments, i);
  //sleep(5);
  //}
  // slowly achieve awesomeness - asymmetric weights version!
  double i;
  //bool terminate = false;
  //for (i = 0; !terminate; i += ADAPTATION_STEP) {
  // if (i > sum_nww) {
  // i = sum_nww;
  // terminate = true;
  // }
  for (i = 0; i <= 100; i += ADAPTATION_STEP) {
    LINFOF("Going to check a ratio of %lf", i);
    //First check the stall rate of the initial weights without moving pages!
    if (i != 0) {
      place_all_pages(segments, i);
    }
    //usleep(200000);
    //sleep(1);
    //unstickymem_log(i);
    stall_rate = get_average_stall_rate(_num_polls, _poll_sleep,
                                        _num_poll_outliers);
    //print stall_rate to a file for debugging!
    //unstickymem_log(i, stall_rate);
    LINFOF("Ratio: %lf StallRate: %1.10lf (previous %1.10lf; best %1.10lf)", i,
           stall_rate, prev_stall_rate, best_stall_rate);

    // compute the minimum rate
    best_stall_rate = std::min(best_stall_rate, stall_rate);
    // check if we are geting worse
    if (stall_rate > best_stall_rate * 1.001) {
      // just make sure that its not something transient...!
      LINFO("Hmm... Is this the best we can do?");
      if (get_average_stall_rate(_num_polls * 2, _poll_sleep,
                                 _num_poll_outliers * 2)
          > (best_stall_rate * 1.001)) {
        LINFO("I guess so!");
        break;
      }
    }
    prev_stall_rate = stall_rate;
  }
  LINFO("My work here is done! Enjoy the speedup");
  LINFOF("Ratio: %lf", i);
  LINFOF("Stall Rate: %1.10lf", stall_rate);
  LINFOF("Best Measured Stall Rate: %1.10lf", best_stall_rate);
}

void WeightedAdaptiveMode::start() {
  // interleave memory by default
  /*LINFO("Setting default memory policy to interleaved");
   set_mempolicy(MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
   numa_get_mems_allowed()->size);
   return;*/
  // use weighted interleave as a default if the memory allocations are small!
  /*MemoryMap &segments = MemoryMap::getInstance();
   for (auto &segment : segments) {
   if (segment.length() > (1UL << 14)) {
   // segment.print();
   place_pages_weighted_initial(segment);
   }
   }

   _started = true;
   */
  // start adaptive thread
  std::thread adaptiveThread(&WeightedAdaptiveMode::adaptiveThread, this);

  // dont want for it to finish
  adaptiveThread.detach();
}

}  // namespace unstickymem
