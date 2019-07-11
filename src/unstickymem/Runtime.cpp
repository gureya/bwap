#include <sys/mman.h>

#include <algorithm>
#include <iostream>
#include <fstream>

#include <boost/exception/diagnostic_information.hpp>

#include "better-enums/enum.h"

#include "unstickymem/mode/Mode.hpp"
#include "unstickymem/Runtime.hpp"
#include "unstickymem/Logger.hpp"

namespace unstickymem {

Runtime& Runtime::getInstance(void) {
  static Runtime *object = nullptr;
  if (!object) {
    LDEBUG("Creating Runtime singleton object");
    void *buf = WRAP(mmap)(nullptr, sizeof(Runtime), PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    DIEIF(buf == MAP_FAILED, "error allocating space for runtime object");
    object = new (buf) Runtime();
  }
  return *object;
}

Runtime::Runtime() {
  loadConfiguration();
  printConfiguration();
  if (_autostart) {
    startSelectedMode();
  }
}

void Runtime::loadConfiguration() {
  std::ifstream ini_filename("unstickymem.ini");
  bool option_help;
  std::string option_loglevel;

  // library-level options
  po::options_description lib_options("Library Options");
  lib_options.add_options()("UNSTICKYMEM_HELP",
                            po::value<bool>(&option_help)->default_value(false),
                            "Prints library options")(
      "UNSTICKYMEM_MODE",
      po::value < std::string > (&_mode_name)->default_value("wadaptive"),
      "The algorithm to be ran")(
      "UNSTICKYMEM_AUTOSTART",
      po::value<bool>(&_autostart)->default_value(false),
      "Run the algorithm automatically at startup")(
      "UNSTICKYMEM_LOGLEVEL",
      po::value < std::string > (&option_loglevel)->default_value("info"),
      "Log level (trace, debug, info, warn, error, fatal, off)");

  // load library options from environment
  po::variables_map lib_env;
  po::store(
      po::parse_environment(lib_options, [lib_options](const std::string& var) {
        return std::any_of(
            lib_options.options().cbegin(),
            lib_options.options().cend(),
            [var](auto opt) {return var == opt->long_name();}) ? var : "";
      }),
      lib_env);
  po::store(po::parse_config_file(ini_filename, lib_options, true), lib_env);
  po::notify(lib_env);

  // get options of selected mode
  _mode = Mode::getMode(_mode_name);
  po::options_description mode_options = _mode->getOptions();

  // put all the options together
  po::options_description all_options("unstickymem options");
  all_options.add(mode_options);

  // parse all options (and ignore undeclared)
  po::variables_map env;
  po::store(
      po::parse_environment(all_options, [all_options](const std::string& var) {
        return std::any_of(
            all_options.options().cbegin(),
            all_options.options().cend(),
            [var](auto opt) {return var == opt->long_name();}) ? var : "";
      }),
      env);
  po::store(po::parse_config_file(ini_filename, all_options, true), env);
  po::notify(env);

  // check if user wants help
  if (option_help) {
    std::cout << std::endl << all_options << std::endl;
    exit(0);
  }

  // set log level
  L->loglevel(option_loglevel);
}

void Runtime::printConfiguration() {
  LINFOF("Mode:      %s", _mode_name.c_str());
  LINFOF("Autostart: %s", _autostart ? "enabled" : "disabled");
}

std::shared_ptr<Mode> Runtime::getMode() {
  return _mode;
}

void Runtime::startSelectedMode() {
  //LINFO("Mode parameters:");
  //_mode->printParameters();
  LINFO("Starting the Mode:");
  _mode->start();
}

void Runtime::startMemoryInitialization() {
  //LINFO("Mode parameters:");
  //_mode->printParameters();
  LINFO("Starting the Memory Initialization:");
  _mode->startMemInit();
}

}  // namespace unstickymem
