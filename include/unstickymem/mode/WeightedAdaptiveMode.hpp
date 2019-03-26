#ifndef INCLUDE_UNSTICKYMEM_MODE_WEIGHTEDADAPTIVEMODE_HPP_
#define INCLUDE_UNSTICKYMEM_MODE_WEIGHTEDADAPTIVEMODE_HPP_

#include <string>

#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

class WeightedAdaptiveMode : public Mode {
 private:
  bool _started = false;
  unsigned int _wait_start;
  unsigned int _num_polls;
  unsigned int _num_poll_outliers;
  useconds_t _poll_sleep;
 public:
  static std::string name() {
    return "wadaptive";
  }

  static std::string description() {
    return "Adaptive mode with weighted interleaving";
  }

  static std::unique_ptr<Mode> createInstance() {
    return std::make_unique<WeightedAdaptiveMode>();
  }

  po::options_description getOptions();
  void printParameters();
  void adaptiveThread();
  void start();
  void processSegmentAddition(const MemorySegment& segment);
};

}  // namespace unstickymem

#endif  // INCLUDE_UNSTICKYMEM_MODE_WEIGHTEDADAPTIVEMODE_HPP_
