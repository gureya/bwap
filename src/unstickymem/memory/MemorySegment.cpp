#include "unstickymem/memory/MemorySegment.hpp"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/Logger.hpp"

namespace unstickymem {

MemorySegment::MemorySegment(void *start, void *end, std::string name)
    : _startAddress(start),
      _endAddress(end),
      _name(name) {
}

MemorySegment::MemorySegment(char *line) {
  int name_start = 0;
  int name_end = 0;
  uintptr_t addr_start;
  uintptr_t addr_end;
  char perms_str[8];
  uint64_t offset;
  unsigned int deviceMajor;
  unsigned int deviceMinor;
  ino_t inode;

  // parse string
  DIEIF(
      sscanf(line, "%lx-%lx %7s %lx %u:%u %lu %n%*[^\n]%n", &addr_start,
             &addr_end, perms_str, &offset, &deviceMajor, &deviceMinor, &inode,
             &name_start, &name_end) < 7,
      "FAILED TO PARSE");

  // convert addresses
  _startAddress = reinterpret_cast<void*>(addr_start);
  _endAddress = reinterpret_cast<void*>(addr_end - 1);

  // copy name
  if (name_end > name_start) {
    line[name_end] = '\0';
    _name.assign(&line[name_start]);
  }
}

void* MemorySegment::startAddress() const {
  return _startAddress;
}

void* MemorySegment::endAddress() const {
  return _endAddress;
}

std::string MemorySegment::name() const {
  return _name;
}

void MemorySegment::startAddress(void *addr) {
  _startAddress = addr;
}

void MemorySegment::endAddress(void *addr) {
  _endAddress = addr;
}

void MemorySegment::name(std::string name) {
  _name = name;
}

void* MemorySegment::pageAlignedStartAddress() const {
  return reinterpret_cast<void*>(PAGE_ALIGN_DOWN(_startAddress));
}

void* MemorySegment::pageAlignedEndAddress() const {
  return reinterpret_cast<void*>(PAGE_ALIGN_UP(_endAddress));
}

size_t MemorySegment::length() const {
  return reinterpret_cast<intptr_t>(endAddress())
      - reinterpret_cast<intptr_t>(startAddress()) + 1;
}

size_t MemorySegment::pageAlignedLength() const {
  return reinterpret_cast<intptr_t>(pageAlignedEndAddress())
      - reinterpret_cast<intptr_t>(pageAlignedStartAddress());
}

void MemorySegment::print() const {
  char info[1024];
  snprintf(info, sizeof(info), "[%18p-%18p] (%8lu pages) %s", _startAddress,
           _endAddress, (length() + 1) / sysconf(_SC_PAGESIZE), _name.c_str());
  L->printHorizontalRule(info, 4);
}

bool MemorySegment::contains(void *addr) const {
  return addr >= startAddress() && addr <= endAddress();
}

bool MemorySegment::contains(const MemorySegment & otherSegment) const {
  return contains(otherSegment.startAddress())
      && contains(otherSegment.endAddress());
}

bool MemorySegment::isContainedIn(const MemorySegment & otherSegment) const {
  return otherSegment.contains(*this);
}

bool MemorySegment::intersectsWith(const MemorySegment & segment) const {
  return contains(segment.startAddress()) || contains(segment.endAddress())
      || segment.contains(startAddress()) || segment.contains(endAddress());
}

bool MemorySegment::isDisjointWith(const MemorySegment & segment) const {
  return !intersectsWith(segment);
}

}  // namespace unstickymem
