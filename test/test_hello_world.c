#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unstickymem/unstickymem.h>

#define SIZE      100 * 1024 * 1024
#define NUM_ELEMS SIZE / sizeof(int)

int main() {
  printf("Hello world\n");

  int *x = malloc(SIZE);
  printf("acessing unallocated value: %d\n", x[0]);
  printf("x_base: %p, size(mb): %d\n", x, SIZE / 1000);
 // unstickymem_start();
  x[0] = 123;
  for(size_t i=1; i < NUM_ELEMS; i++) {
    x[i] = x[0] * x[0] % 1000000;
  }

  unstickymem_start();
  sleep(60);
  free(x);
  return 0;
}
