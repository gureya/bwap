#ifndef UNSTICKYMEM_FIXEDRATIOMODE_HPP_
#define UNSTICKYMEM_FIXEDRATIOMODE_HPP_

#include <string>

#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

class FixedRatioMode : public Mode {
 private:
  double _local_ratio;
  unsigned int _num_polls;
  unsigned int _num_poll_outliers;
  useconds_t _poll_sleep;

 public:
  static std::string name() {
    return "fixed";
  }

  static std::string description() {
    return "Places all pages with a predefined local_ratio";
  }

  static std::unique_ptr<Mode> createInstance() {
    return std::make_unique<FixedRatioMode>();
  }

  po::options_description getOptions();
  void printParameters();
  void pollerThread();
  void start();
  void processSegmentAddition(const MemorySegment& segment);
};

}  // namespace unstickymem

#endif  // UNSTICKYMEM_FIXEDRATIOMODE_HPP_
