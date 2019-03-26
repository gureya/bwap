#ifndef UNSTICKYMEM_DISABLEDMODE_HPP_
#define UNSTICKYMEM_DISABLEDMODE_HPP_

#include <string>
#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

class DisabledMode : public Mode {
 public:
  static std::string name() {
    return "disabled";
  }
  static std::string description() {
    return "Do Nothing";
  }
  static std::unique_ptr<Mode> createInstance() {
    return std::make_unique<DisabledMode>();
  }

  po::options_description getOptions();

  void printParameters();

  void start();
};

}  // namespace unstickymem

#endif  // UNSTICKYMEM_DISABLEDMODE_HPP_
