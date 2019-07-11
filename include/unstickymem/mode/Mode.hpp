#ifndef UNSTICKYMEM_MODE_HPP_
#define UNSTICKYMEM_MODE_HPP_

#include <memory>
#include <map>
#include <string>

#include <boost/program_options.hpp>

#include "unstickymem/memory/MemorySegment.hpp"
#include "unstickymem/Logger.hpp"
#include "unstickymem/wrap.hpp"

namespace po = boost::program_options;

namespace unstickymem {

class Mode {
  using create_f = std::unique_ptr<Mode>();
  using Description = struct {
    create_f* create_function;
    std::string description;
  };

 private:
  static std::map<std::string, Description> & registry();

 public:
  virtual po::options_description getOptions() = 0;
  virtual void printParameters() = 0;
  virtual void start() = 0;
  virtual void startMemInit() = 0;

  virtual void processSegmentAddition(const MemorySegment& segment) {
  }
  virtual void processSegmentRemoval(const MemorySegment& segment) {
  }

  static void registerMode(std::string const & name, Description desc) {
    // disallow replacing entries
    DIEIF(registry().count(name) == 1, "Mode already registered");
    registry()[name] = desc;
  }

  static std::unique_ptr<Mode> getMode(std::string const & name) {
    if (registry().count(name) == 0) {
      printAvailableModes();
      DIE("Please select one of the available modes :-)");
    }
    return registry()[name].create_function();
  }

  static void printAvailableModes() {
    LWARN("Available Modes:");
  for (auto & [name, d] : registry()) {
    LWARNF("> %-10s (%s)", name.c_str(), d.description.c_str());
  }
}

template<typename ModeImplementation>
struct Registrar {
  explicit Registrar(std::string const & name,
                     std::string const & description) {
    Mode::registerMode(name,
                       { &ModeImplementation::createInstance, description });
  }
};
};

}  // namespace unstickymem

#endif  // UNSTICKYMEM_MODE_HPP_
