#include "unstickymem/mode/Mode.hpp"

namespace unstickymem {

std::map<std::string, Mode::Description> & Mode::registry() {
  static std::map<std::string, Mode::Description> r;
  return r;
}

}  // namespace unstickymem
