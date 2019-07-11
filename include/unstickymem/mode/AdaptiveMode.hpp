#ifndef UNSTICKYMEM_ADAPTIVEMODE_HPP_
#define UNSTICKYMEM_ADAPTIVEMODE_HPP_

#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

class AdaptiveMode : public Mode {
 private:
  unsigned int _wait_start;
  unsigned int _num_polls;
  unsigned int _num_poll_outliers;
  useconds_t _poll_sleep;
 public:
  static std::string name() {
    return "adaptive";
  }

  static std::string description() {
    return "Look for optimal local/remote page placement";
  }

  static std::unique_ptr<Mode> createInstance() {
    return std::make_unique<AdaptiveMode>();
  }

  po::options_description getOptions();
  void printParameters();
  void adaptiveThread();
  void start();
  void startMemInit();
};

}  // namespace unstickymem

#endif  // UNSTICKYMEM_ADAPTIVEMODE_HPP_
