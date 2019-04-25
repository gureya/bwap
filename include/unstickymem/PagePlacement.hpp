#ifndef UNSTICKYMEM_PAGE_PLACEMENT_HPP_
#define UNSTICKYMEM_PAGE_PLACEMENT_HPP_

#include <unistd.h>
#include <numaif.h>
#include <numa.h>

#include "unstickymem/memory/MemoryMap.hpp"
#include "unstickymem/memory/MemorySegment.hpp"

#define PAGE_ALIGN_DOWN(x) (((intptr_t) (x)) & PAGE_MASK)
#define PAGE_ALIGN_UP(x) ((((intptr_t) (x)) + ~PAGE_MASK) & PAGE_MASK)

namespace unstickymem {

static const int PAGE_SIZE = sysconf(_SC_PAGESIZE);
static const int PAGE_MASK = (~(PAGE_SIZE - 1));

// XXX temporary workaround for bug in numactl XXX
// https://github.com/numactl/numactl/issues/38
#ifndef MPOL_LOCAL
#define MPOL_LOCAL 4
#endif

void force_uniform_interleave(char *addr, unsigned long len);
void force_uniform_interleave(MemorySegment &segment);
void place_pages(void *addr, unsigned long len, double ratio);
void place_pages_weighted_initial(const MemorySegment &segment);
void place_pages_weighted_initial(void *addr, unsigned long len);
void place_all_pages(MemoryMap &segments, double ratio);
void place_all_pages(double ratio);

void place_pages_weighted_contiguous(const MemorySegment &segment);
void place_pages_weighted_contiguous(void *addr, unsigned long len);

void place_pages_weighted_s(void *addr, unsigned long len, double s);
void place_pages_weighted(void *addr, unsigned long len);
void place_all_pages_adaptive(double ratio);

void place_all_pages_adaptive(MemoryMap &segments, double ratio);
void place_pages_adaptive(MemorySegment &segment, double ratio);

}  // namespace unstickymem

#endif  // UNSTICKYMEM_PAGE_PLACEMENT_HPP_
