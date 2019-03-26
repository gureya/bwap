/**
 * Copyright 2018 João Neto
 * Wrapper for native libc/pthread functions
 **/
#include <pthread.h>
#include <dlfcn.h>
#include <cassert>

#include "unstickymem/Logger.hpp"
#include "unstickymem/wrap.hpp"

#define SET_WRAPPED(x, handle) \
  do {\
    WRAP(x) = (__typeof__(WRAP(x))) dlsym(handle, #x);\
    assert(#x);\
    if (WRAP(x) == NULL) {\
      LFATALF("Could not wrap function %s", #x);\
    }\
  } while (0)

// linux
void* (*WRAP(malloc))(size_t);
void* (*WRAP(calloc))(size_t, size_t);
void* (*WRAP(realloc))(void*, size_t);
void* (*WRAP(reallocarray))(void*, size_t, size_t);
void (*WRAP(free))(void*);

int (*WRAP(posix_memalign))(void**, size_t, size_t);

void* (*WRAP(mmap))(void*, size_t, int, int, int, off_t);
int (*WRAP(munmap))(void*, size_t);
void* (*WRAP(mremap))(void*, size_t, size_t, int, ...);

int (*WRAP(brk))(void*);
void* (*WRAP(sbrk))(intptr_t);

long (*WRAP(mbind))(void*, unsigned long, int, const unsigned long*,
                    unsigned long, unsigned);

namespace unstickymem {

void init_real_functions() {
  LDEBUG("Initializing references to replaced library functions");

  // linux memory allocations
  SET_WRAPPED(malloc, RTLD_NEXT);
  SET_WRAPPED(calloc, RTLD_NEXT);
  SET_WRAPPED(realloc, RTLD_NEXT);
  SET_WRAPPED(reallocarray, RTLD_NEXT);
  SET_WRAPPED(free, RTLD_NEXT);

  SET_WRAPPED(posix_memalign, RTLD_NEXT);

  SET_WRAPPED(mmap, RTLD_NEXT);
  SET_WRAPPED(munmap, RTLD_NEXT);
  SET_WRAPPED(mremap, RTLD_NEXT);

  SET_WRAPPED(brk, RTLD_NEXT);
  SET_WRAPPED(sbrk, RTLD_NEXT);

  SET_WRAPPED(mbind, RTLD_NEXT);
}

}  // namespace unstickymem
