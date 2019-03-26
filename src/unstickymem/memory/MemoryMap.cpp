#include <sys/mman.h>

#include <algorithm>
#include <numeric>
#include <iostream>

#include "unstickymem/Runtime.hpp"
#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/Logger.hpp"
#include "unstickymem/wrap.hpp"

extern void *etext;
extern void *edata;
extern void *end;

namespace unstickymem {

MemoryMap::MemoryMap() {
  // create independent segment to store the MemoryMap
  LFATAL("going to create segment");
  /*Manager *segment_manager = _segment.get_segment_manager();
  _segments = _segment.construct < SegmentsList
      > ("unstickymem")(segment_manager);*/
  _segments = new std::list<MemorySegment>();

  // open maps file
  FILE *maps = fopen("/proc/self/maps", "r");
  DIEIF(maps == nullptr, "error opening maps file");

  // parse the maps file, searching for the heap start
  char *line = NULL;
  size_t line_size = 0;
  while (getline(&line, &line_size, maps) > 0) {
    MemorySegment s(line);
    if (s.name() == "[heap]") {
      // found the heap!
      _segments->emplace_back(s.startAddress(), s.endAddress(), "heap");
      _heap = &_segments->back();
      Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
    } else if (s.name() == "[stack]") {
      // found the stack!
      _segments->emplace_back(s.startAddress(), s.endAddress(), "stack");
      _stack = &_segments->back();
    } else if (s.contains(&etext - 1)) {
      // found the text segment (read-only data)
      _segments->emplace_back(s.startAddress(), s.endAddress(), "text");
      _text = &_segments->back();
      Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
    } else if (s.contains(&edata - 1)) {
      // found the data segment (global variables)
      _segments->emplace_back(s.startAddress(), s.endAddress(), "data");
      _data = &_segments->back();
      Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
    } else if (s.name() == "") {
      _segments->emplace_back(s.startAddress(), s.endAddress(), "anonymous");
      Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
    }
  }

  DIEIF(_heap == nullptr, "didnt find the heap!");
  DIEIF(_stack == nullptr, "didnt find the stack!");
  DIEIF(_text == nullptr, "didnt find the text segment!");
  DIEIF(_data == nullptr, "didnt find the data segment!");

  // cleanup
  WRAP(free)(line);
  DIEIF(!feof(maps) || ferror(maps), "error parsing maps file");
  DIEIF(fclose(maps), "error closing maps file");
}

MemoryMap& MemoryMap::getInstance(void) {
  static MemoryMap *object = nullptr;
  if (!object) {
    LDEBUG("Creating MemoryMap singleton object");
    void *buf = WRAP(mmap)(nullptr, sizeof(MemoryMap), PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    DIEIF(buf == MAP_FAILED, "error allocating space for memory map object");
    LFATAL("before constructor");
    object = new (buf) MemoryMap();
  }
  return *object;
}

void MemoryMap::print(void) const {
  for (auto &segment : *_segments) {
    segment.print();
  }
}

void MemoryMap::updateHeap(void) {
  void *addr = WRAP(sbrk)(0);
  if (_heap->endAddress() != addr) {
    _heap->endAddress(WRAP(sbrk)(0));
    //Runtime::getInstance().getMode()->processSegmentAddition(*_heap);
  }
}

// iterators
SegmentsList::iterator MemoryMap::begin() noexcept {
  return _segments->begin();
}

SegmentsList::iterator MemoryMap::end() noexcept {
  return _segments->end();
}

SegmentsList::const_iterator MemoryMap::cbegin() const noexcept {
  return _segments->cbegin();
}

SegmentsList::const_iterator MemoryMap::cend() const noexcept {
  return _segments->cend();
}

void *MemoryMap::handle_malloc(size_t size) {
  void *result = WRAP(malloc)(size);

  // compute the end address
  void *start = result;
  void *end = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(result) + size
      - 1);

  // update the heap
  updateHeap();

  // if it was not placed in the heap, means it is a new region!
  if (!_heap->contains(result)) {
    std::scoped_lock lock(_segments_lock);
    _segments->emplace_back(start, end, "malloc");
    Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
  }
  return result;
}

void* MemoryMap::handle_calloc(size_t nmemb, size_t size) {
  void *result = WRAP(calloc)(nmemb, size);

  // compute end address
  void *start = result;
  void *end = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(result)
      + (nmemb * size) - 1);

  // update the heap
  updateHeap();

  // if it was not placed in the heap, means it is a new region!
  if (!_heap->contains(result)) {
    std::scoped_lock lock(_segments_lock);
    _segments->emplace_back(start, end, "calloc");
    Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
  }
  return result;
}

void* MemoryMap::handle_realloc(void *ptr, size_t size) {
  // check if object is in heap before realloc
  bool was_in_heap = _heap->contains(ptr);

  // do the realloc
  void *result = WRAP(realloc)(ptr, size);

  // check if object is in heap after realloc
  updateHeap();
  bool is_in_heap = _heap->contains(result);

  // determine if object before was outside the heap
  if (!was_in_heap) {
    std::scoped_lock lock(_segments_lock);
    _segments->remove_if([ptr](const MemorySegment& s) {
      if (s.contains(ptr)) {
        std::shared_ptr<Mode> mode = Runtime::getInstance().getMode();
        mode->processSegmentRemoval(s);
        return true;
      }
      return false;
    });
  }

  if (!is_in_heap) {
    // compute start and end address
    void *start = result;
    void *end = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(result)
        + size - 1);
    // insert the new segment
    std::scoped_lock lock(_segments_lock);
    _segments->emplace_back(start, end, "realloc");
    Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
  }
  return result;
}

void* MemoryMap::handle_reallocarray(void *ptr, size_t nmemb, size_t size) {
  void *result = WRAP(reallocarray)(ptr, nmemb, size);
  LFATAL("REALLOCARRAY NOT IMPLEMENTED");
  return result;
}

void MemoryMap::handle_free(void *ptr) {
  bool was_in_heap = _heap->contains(ptr);
  WRAP(free)(ptr);

  // check where the segment was allocated
  if (was_in_heap) {
    updateHeap();
  } else {
    // if not in heap, remove the mapped segment
    std::scoped_lock lock(_segments_lock);
    _segments->remove_if([ptr](const MemorySegment& s) {
      if (s.contains(ptr)) {
        std::shared_ptr<Mode> mode = Runtime::getInstance().getMode();
        mode->processSegmentRemoval(s);
        return true;
      }
      return false;
    });
  }
}

int MemoryMap::handle_posix_memalign(void **memptr, size_t alignment,
                                     size_t size) {
  // call the actual function
  int result = WRAP(posix_memalign)(memptr, alignment, size);

  // update the heap
  updateHeap();

  // compute region start and address
  void *start = *memptr;
  void *end = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(*memptr) + size
      - 1);

  // add the new region
  if (!_heap->contains(*memptr)) {
    std::scoped_lock lock(_segments_lock);
    _segments->emplace_back(start, end, "posix_memalign");
    Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());
  }

  return result;
}

void* MemoryMap::handle_mmap(void *addr, size_t length, int prot, int flags,
                             int fd, off_t offset) {
  void *result = WRAP(mmap)(addr, length, prot, flags, fd, offset);

  // compute the end address
  void *start = result;
  void *end = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(result)
      + length - 1);

  // insert the new segment
  std::scoped_lock lock(_segments_lock);
  _segments->emplace_back(start, end, "mmap");
  Runtime::getInstance().getMode()->processSegmentAddition(_segments->back());

  // return the result
  return result;
}

int MemoryMap::handle_munmap(void *addr, size_t length) {
  int result = WRAP(munmap)(addr, length);

  // remove the mapped region
  std::scoped_lock lock(_segments_lock);
  _segments->remove_if([addr](const MemorySegment& s) {
    if (s.contains(addr)) {
      std::shared_ptr<Mode> mode = Runtime::getInstance().getMode();
      mode->processSegmentRemoval(s);
      return true;
    }
    return false;
  });

  return result;
}

void* MemoryMap::handle_mremap(void *old_address, size_t old_size,
                               size_t new_size, int flags,
                               ... /* void *new_address */) {
  DIE("NOT IMPLEMENTED");
}

int MemoryMap::handle_brk(void* addr) {
  DIE("NOT IMPLEMENTED");
}

void* MemoryMap::handle_sbrk(intptr_t increment) {
  DIE("NOT IMPLEMENTED");
}

long MemoryMap::handle_mbind(void* addr, unsigned long len, int mode,
                             const unsigned long *nodemask,
                             unsigned long maxnode, unsigned flags) {
  long result = WRAP(mbind)(addr, len, mode, nodemask, maxnode, flags);
  DIE("NOT IMPLEMENTED");
  return result;
}

}  // namespace unstickymem
