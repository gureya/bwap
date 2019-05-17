#include <numa.h>
#include <numaif.h>

#include <memory>
#include <functional>

#include "unstickymem/mode/DisabledMode.hpp"
#include "unstickymem/Logger.hpp"
#include <unstickymem/PerformanceCounters.hpp>

namespace unstickymem {

static Mode::Registrar<DisabledMode> registrar(DisabledMode::name(),
                                               DisabledMode::description());

po::options_description DisabledMode::getOptions() {
  po::options_description options("Disabled Mode Options");
  return options;
}

void DisabledMode::printParameters() {
  LINFO("No parameters");
}

void DisabledMode::start() {
   //get_elapsed_stall_rate();
  // interleave memory by default
 /* LINFO("Setting default memory policy to interleaved");
  set_mempolicy(MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
                numa_get_mems_allowed()->size);*/
}

}  // namespace unstickymem
