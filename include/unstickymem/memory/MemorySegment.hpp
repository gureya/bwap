#ifndef INCLUDE_UNSTICKYMEM_MEMORY_MEMORYSEGMENT_HPP_
#define INCLUDE_UNSTICKYMEM_MEMORY_MEMORYSEGMENT_HPP_

#include <stdlib.h>
#include <string>
#include <vector>

namespace unstickymem {

class MemorySegment {
 protected:
  void* _startAddress;
  void* _endAddress;
  std::string _name;

 public:
  MemorySegment(void *start, void *end, std::string name);
  explicit MemorySegment(char *line);

  // getters
  void* startAddress() const;
  void* endAddress() const;
  std::string name() const;

  // get derived attributes
  void* pageAlignedStartAddress() const;
  void* pageAlignedEndAddress() const;
  size_t length() const;
  size_t pageAlignedLength() const;

  // setters
  void startAddress(void *addr);
  void endAddress(void *addr);
  void name(std::string name);

  // utility functions
  void print() const;

  // set/interval functions
  bool contains(void *addr) const;
  bool contains(const MemorySegment & segment) const;
  bool isContainedIn(const MemorySegment & otherSegment) const;
  bool intersectsWith(const MemorySegment & other) const;
  bool isDisjointWith(const MemorySegment & segment) const;

  // friend functions
  friend class MemoryMap;
};

}  // namespace unstickymem

#endif  // INCLUDE_UNSTICKYMEM_MEMORY_MEMORYSEGMENT_HPP_
