/**
 * Copyright 2018 João Neto
 * Wrapper for native libc/pthread functions
 **/

#ifndef INCLUDE_UNSTICKYMEM_WRAP_HPP_
#define INCLUDE_UNSTICKYMEM_WRAP_HPP_

#include <pthread.h>

#define WRAP(x) _unstickymem_real_##x

extern void* (*WRAP(malloc))(size_t);
extern void* (*WRAP(calloc))(size_t, size_t);
extern void* (*WRAP(realloc))(void*, size_t);
extern void* (*WRAP(reallocarray))(void*, size_t, size_t);
extern void (*WRAP(free))(void*);

extern int (*WRAP(posix_memalign))(void**, size_t, size_t);

extern void* (*WRAP(mmap))(void*, size_t, int, int, int, off_t);
extern int (*WRAP(munmap))(void*, size_t);
extern void* (*WRAP(mremap))(void*, size_t, size_t, int, ...);

extern int (*WRAP(brk))(void*);
extern void* (*WRAP(sbrk))(intptr_t);

extern long (*WRAP(mbind))(void*, unsigned long, int, const unsigned long*,
                           unsigned long, unsigned);

namespace unstickymem {

void init_real_functions();

}  // namespace unstickymem

#endif  // INCLUDE_UNSTICKYMEM_WRAP_HPP_
