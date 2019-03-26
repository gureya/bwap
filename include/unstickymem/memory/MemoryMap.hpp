#ifndef INCLUDE_UNSTICKYMEM_MEMORY_MEMORYMAP_HPP_
#define INCLUDE_UNSTICKYMEM_MEMORY_MEMORYMAP_HPP_

#include <stdlib.h>

#include <list>
#include <iterator>
#include <mutex>
#include <string>

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/list.hpp>

#include "unstickymem/memory/MemorySegment.hpp"

namespace unstickymem {

namespace ipc = boost::interprocess;
using Segment = ipc::managed_shared_memory;
using Manager = Segment::segment_manager;

template<typename T>
using Alloc = ipc::allocator<T, Manager>;

template<typename K>
using List = ipc::list<K, Alloc<K> >;

typedef std::list<MemorySegment> SegmentsList;

class MemoryMap {
 private:
  SegmentsList *_segments;
  MemorySegment *_heap = nullptr;
  MemorySegment *_stack = nullptr;
  MemorySegment *_text = nullptr;
  MemorySegment *_data = nullptr;
  std::mutex _segments_lock;

  // FIXME(joaomlneto): this won't work if multiple unstickymem processes
  //                    are running!!!
  //Segment _segment { ipc::create_only, "unstickymem", 1ul << 40 };

 private:
  MemoryMap();

 public:
  // singleton
  static MemoryMap& getInstance(void);
  MemoryMap(MemoryMap const&) = delete;
  void operator=(MemoryMap const&) = delete;

  void *getHeapStartAddress(void) const;
  void print(void) const;
  void updateHeap(void);

  // iterators
  SegmentsList::iterator begin() noexcept;
  SegmentsList::iterator end() noexcept;
  SegmentsList::const_iterator cbegin() const noexcept;
  SegmentsList::const_iterator cend() const noexcept;

  // handle allocations/deallocations
  void* handle_malloc(size_t size);
  void* handle_calloc(size_t nmemb, size_t size);
  void* handle_realloc(void *ptr, size_t size);
  void* handle_reallocarray(void *ptr, size_t nmemb, size_t size);
  void handle_free(void *ptr);
  int handle_posix_memalign(void **memptr, size_t alignment, size_t size);
  void* handle_mmap(void *addr, size_t length, int prot, int flags, int fd,
                    off_t offset);
  int handle_munmap(void *addr, size_t length);
  void *handle_mremap(void *old_address, size_t old_size, size_t new_size,
                      int flags, ... /* void *new_address */);
  int handle_brk(void* addr);
  void* handle_sbrk(intptr_t increment);
  long handle_mbind(void* addr, unsigned long len, int mode,
                    const unsigned long *nodemask, unsigned long maxnode,
                    unsigned flags);
};

}  // namespace unstickymem

#endif  // INCLUDE_UNSTICKYMEM_MEMORY_MEMORYMAP_HPP_
