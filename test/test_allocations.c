#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unstickymem/unstickymem.h>

extern void *etext;
extern void *edata;
extern void *end;

const int global_const = 123;
const int const_array[4096] = {1};

int global_variable;
int global_array[4096] = {1};

int main() {
  printf("\n");
  printf("etext: %p\n", &etext);
  printf("edata: %p\n", &edata);
  printf("end:   %p\n\n", &end);
  printf("const      @ %p\n", &global_const);
  printf("global var @ %p\n\n\n", &global_variable);

  /**
   * MALLOC / CALLOC / REALLOC / FREE
   * TODO: reallocarray
   */

  void *x = malloc(1024*1024*1024);
  printf("\nafter malloc\n");
  unstickymem_print_memory();

  x = realloc(x, 1024*1024);
  printf("\nafter realloc\n");
  unstickymem_print_memory();

  x = realloc(x, 128);
  printf("\nafter realloc\n");
  unstickymem_print_memory();

  free(x);
  printf("\nafter free\n");
  unstickymem_print_memory();

  x = calloc(1024, 1024*1024);
  printf("\nafter calloc\n");
  unstickymem_print_memory();

  free(x);
  printf("\nafter free\n");
  unstickymem_print_memory();

  /**
   * posix_memalign
   */

  // small posix_memalign that goes in the heap
  posix_memalign(&x, 32, 65536);
  printf("\nafter posix_memalign\n");
  unstickymem_print_memory();

  free(x);
  printf("\nafter free\n");
  unstickymem_print_memory();

  // big posix_memalign that goes in a separate region
  posix_memalign(&x, 32, 1024*1024*1024);
  printf("\nafter posix_memalign\n");
  unstickymem_print_memory();

  free(x);
  printf("\nafter free\n");
  unstickymem_print_memory();

  /**
   * MMAP / MUNMAP
   * TODO: mremap
   */

  void *ptrs[100];
  for (int i=0; i < 100; i++) {
    ptrs[i] = mmap(NULL, 1024*1024*1024, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  }
  printf("after 100 mmaps of 1GB each\n");
  unstickymem_print_memory();

  for (int i=0; i < 99; i++) {
    munmap(ptrs[i], 1024*1024*1024);
  }
  printf("after 99 munmaps\n");
  unstickymem_print_memory();

  void *small_mmap = mmap(NULL, 1, PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  printf("after 1 byte mmap\n");
  unstickymem_print_memory();

  return 0;
}
