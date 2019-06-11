#ifndef UNSTICKYMEM_SCANMODE_HPP_
#define UNSTICKYMEM_SCANMODE_HPP_

#include <string>

#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

class ScanMode : public Mode {
 private:
  bool _started = false;
  unsigned int _wait_start;
  unsigned int _num_polls;
  unsigned int _num_poll_outliers;
  useconds_t _poll_sleep;
  bool _exit_when_finished;

 public:
  static std::string name() {
    return "scan";
  }

  static std::string description() {
    return "Check stall rate for all local/remote page placement ratios";
  }

  static std::unique_ptr<Mode> createInstance() {
    return std::make_unique<ScanMode>();
  }

  po::options_description getOptions();
  void printParameters();
  void scannerThread();
  void start();
  void processSegmentAddition(const MemorySegment& segment);
};

}  // namespace unstickymem

#endif  // UNSTICKYMEM_SCANMODE_HPP_
