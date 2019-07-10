#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>

#include <time.h>

#include <numeric>
#include <iostream>
#include <cmath>
#include <random>

#include "unstickymem/unstickymem.h"
#include "unstickymem/Logger.hpp"
#include "unstickymem/PagePlacement.hpp"
#include "unstickymem/wrap.hpp"

static int pagesize;

RECORD nodes_info_temp[MAX_NODES];
int weight_initialized = 0;

namespace unstickymem {

void print_command(char *cmd) {
  FILE *fp;
  char buf[1024];

  if ((fp = popen(cmd, "r")) == NULL) {
    perror("popen");
    exit(-1);
  }

  while (fgets(buf, sizeof(buf), fp) != NULL) {
    printf("%s", buf);
  }

  if (pclose(fp)) {
    perror("pclose");
    exit(-1);
  }
}

void print_node_allocations() {
  char buf[1024];
  snprintf(buf, sizeof(buf), "numastat -c %d", getpid());
  printf("\x1B[32m");
  print_command(buf);
  printf("\x1B[0m");
}

void place_on_node(char *addr, unsigned long len, int node) {
  DIEIF(node < 0 || node >= numa_num_configured_nodes(),
        "invalid NUMA node id");
  struct bitmask *nodemask = numa_bitmask_alloc(numa_num_configured_nodes());
  numa_bitmask_setbit(nodemask, node);
  DIEIF(
      WRAP(mbind)(addr, len, MPOL_BIND, nodemask->maskp, nodemask->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
      "mbind error");
}

void force_uniform_interleave(char *addr, unsigned long len) {
  const size_t len_per_call = 64 * PAGE_SIZE;
  int num_nodes = numa_num_configured_nodes();

  // validate input
  DIEIF(len % PAGE_SIZE != 0,
        "Size of region must be a multiple of the page size");

  // compute nodemasks for each node
  std::vector<struct bitmask *> nodemasks(num_nodes);
  for (int i = 0; i < num_nodes; i++) {
    nodemasks[i] = numa_bitmask_alloc(num_nodes);
    numa_bitmask_setbit(nodemasks[i], i);
  }

  // interleave all pages through all nodes
  int node_to_bind = 0;
  while (len > 0) {
    unsigned long mbind_len = std::min(len_per_call, len);
    /*LTRACEF("mbind(%p, %lu, MPOL_BIND, 0x%x, %d, MPOL_MF_MOVE | MPOL_MF_STRICT)",
     addr, mbind_len, *(nodemasks[node_to_bind]->maskp),
     nodemasks[node_to_bind]->size + 1);*/
    DIEIF(
        WRAP(mbind)(addr, mbind_len, MPOL_BIND, nodemasks[node_to_bind]->maskp, nodemasks[node_to_bind]->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
        "mbind error");
    addr += mbind_len;
    len -= mbind_len;
    node_to_bind = (node_to_bind + 1) % num_nodes;
  }

  // free resources
  for (int i = 0; i < num_nodes; i++) {
    numa_bitmask_free(nodemasks[i]);
  }
}

void force_uniform_interleave(MemorySegment &segment) {
  force_uniform_interleave(reinterpret_cast<char*>(segment.startAddress()),
                           segment.length());
}

//fault pages so that the move system call can take into effect!
/*void fault_pages(void *start, unsigned long len){

 pagesize = numa_pagesize();

 char *pages;

 int i, rc;

 void **addr;
 int *status;
 int *nodes;

 int page_count = len / pagesize;

 //page_count = page_count;

 //LINFOF("page_base=%p, page_count=%d", start, page_count);
 addr = (void **) malloc(sizeof(char *) * page_count);
 status = (int *) malloc(page_count * sizeof(int *));
 nodes = (int *) malloc(page_count * sizeof(int *));

 if(!start || !addr || !status || !nodes){
 LINFO("Unable to allocate memory");
 exit(1);
 }

 //pages = (char*) ((void *) ((((long)start) & ~((long)(pagesize - 1))) + pagesize));
 pages = (char *) start;
 //LINFOF("page_base: %p, size(mb): %d pages: %p, pagesize: %d\n", start, len / 1000, pages, pagesize);

 for (i = 0; i < page_count; i++) {
 addr[i] = pages + i * pagesize;
 nodes[i] = 1;
 status[i] = -123;
 }

 LINFO("Making sure pages are faulted before allocation");
 rc = numa_move_pages(0, page_count, addr, NULL, status, 0);
 int *fake;
 for( i = 0; i < page_count; i++){
 //LINFOF("Page %d vaddr=%p status=%d", i, page_base + i * pagesize, status[i]);
 if (status[i] < 0) {
 fake = (int *) (pages + i * pagesize);
 *fake = 123;
 //exit(1);
 }
 }

 LINFO("All pages have been faulted!");
 }*/

//place pages with the move_pages system call
//courtesy: https://stackoverflow.com/questions/10989169/numa-memory-page-migration-overhead/11148999
void move_pages_remote(void *start, unsigned long len, double remote_ratio) {

  pagesize = numa_pagesize();

  char *pages;

  int i, rc;

  void **addr;
  int *status;
  int *nodes;

  int page_count = len / pagesize;

  double interleaved_pages;
  int remote_node, local_node;

  addr = (void **) malloc(sizeof(char *) * page_count);
  status = (int *) malloc(page_count * sizeof(int *));
  nodes = (int *) malloc(page_count * sizeof(int *));

  if (!start || !addr || !status || !nodes) {
    LINFO("Unable to allocate memory");
    exit(1);
  }

  pages = (char *) start;

  //set the remote and local nodes here
  if (OPT_NUM_WORKERS_VALUE == 2) {
    remote_node = 0;
    local_node = 0;
  } else {
    remote_node = 0;
    local_node = 0;
  }

  //uniform distribution memory allocation (using the bwap style format)
  if (remote_ratio <= 50) {
    interleaved_pages = (remote_ratio / 100 * (double) page_count) * MAX_NODES;
    //LINFOF("page_count:%d interleaved_pages:%d", page_count, (int) interleaved_pages);
    for (i = 0; i < page_count; ++i) {
      addr[i] = pages + i * pagesize;
      if (i < interleaved_pages) {
        if (i % 2 == 0) {
          nodes[i] = local_node;
        } else {
          nodes[i] = remote_node;
        }
      } else {
        nodes[i] = local_node;
      }
      status[i] = -123;
    }

    rc = move_pages(0, page_count, addr, nodes, status, MPOL_MF_MOVE);
    if (rc < 0 && errno != ENOENT) {
      perror("move_pages");
      exit(1);
    }

  } else {
    interleaved_pages = ((100 - remote_ratio) / 100 * (double) page_count)
        * MAX_NODES;
    //LINFOF("page_count:%d interleaved_pages:%d", page_count, (int) interleaved_pages);
    for (i = 0; i < page_count; ++i) {
      addr[i] = pages + i * pagesize;
      if (i < interleaved_pages) {
        if (i % 2 == 0) {
          nodes[i] = local_node;
        } else {
          nodes[i] = remote_node;
        }
      } else {
        nodes[i] = remote_node;
      }
      status[i] = -123;
    }

    rc = move_pages(0, page_count, addr, nodes, status, MPOL_MF_MOVE);
    if (rc < 0 && errno != ENOENT) {
      perror("move_pages");
      exit(1);
    }
  }

  //Move pages
  //LINFOF("move_pages....(%d pages)", page_count);
  //pid_t pid = getpid(); //pid of the current process!
  /*rc = move_pages(0, page_count, addr, nodes, status, MPOL_MF_MOVE);
   if (rc < 0 && errno != ENOENT) {
   perror("move_pages");
   exit(1);
   }*/

  //return if the segment is not initialized!
  //check this by moving 1 page and checking the status of the page
  /*addr[0] = pages;
   nodes[0] = local_node;
   status[0] = -123;
   rc = numa_move_pages(0, 1, addr, NULL, status, 0);
   //LINFOF("rc=%d vaddr=%p node=%d status=%d", rc, addr[0], nodes[0], status[0]);
   if (rc < 0 && errno != ENOENT) {
   perror("move_pages");
   exit(1);
   }

   //LINFOF("Segment status=%d", status[0]);
   if (status[0] == remote_node){
   LINFOF("Returning, segment seems to have been moved before, status=%d", status[0]);
   return;
   }*/

  //contiguous memory allocation
  /*struct timeval tval_before, tval_after, tval_result;
   gettimeofday(&tval_before, NULL);
   for (i = 0; i < page_count; ++i) {
   addr[i] = pages + i * pagesize;
   if (i < moved_pages){
   nodes[i] = remote_node;
   }
   else{
   nodes[i] = local_node;
   }
   status[i] = -123;
   }
   gettimeofday(&tval_after, NULL);
   timersub(&tval_after, &tval_before, &tval_result);
   LINFOF("Elapsed time (seconds) to process the nodes of this segment(pages=%d): %ld.%06ld", page_count, (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
   */
  //uniform distribution memory allocation (using a uniform random generator!)
  /*const int range_from = 0;
   const int range_to = page_count;
   std::random_device rand_dev;
   std::mt19937 generator(rand_dev());
   std::uniform_int_distribution<int> distr(range_from, range_to);

   for (i = 0; i < page_count; ++i){
   addr[i] = pages + i * pagesize;
   int check_page = distr(generator);
   if (check_page < moved_pages){
   nodes[i] = remote_node;
   }
   else{
   nodes[i] = local_node;
   }
   status[i] = -123;
   }*/

  //try mbind if possible, to bind the pages that have not been allocated yet!
  /* struct bitmask *node_set = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();
   numa_bitmask_setall(node_set);

   DIEIF(
   WRAP(mbind)(start, (moved_pages * pagesize), MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
   "mbind interleave failed");*/

  /*LINFO("Verifying after pages have been moved");
   for( i = 0; i < page_count; i++){
   //LINFOF("Page %d vaddr=%p status=%d", i, page_base + i * pagesize, status[i]);
   if (status[i] != 1) {
   LINFOF("Bad page state before migrate_pages. Page %d vaddr=%p status %d", i, pages + i * pagesize, status[i]);
   break;
   // exit(1);
   }
   }*/

  free(addr);
  free(status);
  free(nodes);
  //numa_bitmask_free(node_set);

}

// interleave pages using the weights
void place_pages_weighted(void *addr, unsigned long len) {

  size_t size = len;
  void *start = addr;
  int i;
  pagesize = numa_pagesize();
// printf("original size: %zu\n", size);

// nodes that can still receive pages
  struct bitmask *node_set = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();
  numa_bitmask_setall(node_set);

  float w = 0;  // weight that has already been allocated among the nodes that can still receive pages
  int a = MAX_NODES;  // number of nodes which can still receive pages

  size_t total_size = 0;  // total size interleaved so far
  size_t my_size = 0;

  size_t remaining_a;

  for (i = 0; i < MAX_NODES; ++i) {
    if (total_size == size) {
      break;
    }

    // b = size that remains to allocate in the next node with smallest beta
    float b = nodes_info_temp[i].weight - w;
    // printf("i: %d\tb: %.2f\ta:%d\n", i, b, a);
    // if (b != 0) {
    my_size = a * (b / 100) * size;
    // printf("my_size_a: %ld\n", my_size);

    // round up to multiple of the page size
    my_size = PAGE_ALIGN_UP(my_size);

    remaining_a = size - total_size;
    if (my_size > remaining_a) {
      my_size = remaining_a;
    }
    // printf("my_size_b: %zu\n", my_size);
    total_size += my_size;

    // only interleave if memory is in the region
    if (my_size != 0) {
      // LDEBUGF("mbind(%p, %lu, INTERLEAVE, %lx, %zu, MOVE|STRICT)",
      //       start, my_size, *(node_set->maskp), node_set->size + 1);
      DIEIF(
          WRAP(mbind)(start, my_size, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
          "mbind interleave failed");
    }

    start =
        reinterpret_cast<void*>(reinterpret_cast<intptr_t>(start) + my_size);  // increment base to a new location
    a--;  // one less node where to allocate pages
    w = nodes_info_temp[i].weight;  // we update the size already allocated in the remaining nodes
    if (numa_bitmask_isbitset(node_set, nodes_info_temp[i].id)) {
      numa_bitmask_clearbit(node_set, nodes_info_temp[i].id);  // remove node i from the set of remaining nodes
    }
  }

  numa_bitmask_free(node_set);
}

//From local to remote weighted page placement respecting dwp
void place_pages_weighted_dwp(void *addr, unsigned long len, double s) {
  int i = 0;

  if (weight_initialized == 0) {
    double new_s = 0;

    new_s = sum_ww - s;
    // Calculate new weights
    printf("NODE Weights: \t");
    double sum = 0;

    for (i = 0; i < MAX_NODES; i++) {
      switch (OPT_NUM_WORKERS_VALUE) {
        case 1:
          // workers: 0
          if (nodes_info[i].id == 0) {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                nodes_info[i].weight / sum_ww * new_s);
            printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          } else {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(100 - new_s);
            printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          }
          break;
        case 2:
          // workers: 1
          if (nodes_info[i].id == 1) {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                nodes_info[i].weight / sum_ww * new_s);
            printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          } else {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(100 - new_s);
            printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          }
          break;
        default:
          LINFOF("Sorry, %d Worker nodes is not supported at the moment!\n",
                 OPT_NUM_WORKERS_VALUE)
          ;
          exit(-1);
      }
    }

    printf("%.2f\n", sum);

    printf("NODE IDs: \t");
    for (i = 0; i < MAX_NODES; i++) {
      printf("%d\t", nodes_info_temp[i].id);
    }
    printf("\n");

    if ((check_sum(nodes_info_temp)) != 100) {
      printf("**Sum of New weights must be equal to 100, sum=%d!**\n",
             check_sum(nodes_info_temp));
      exit(-1);
    }

    /* printf(
     "===========================================================================\n");*/
    weight_initialized = 1;
  }

// Enforce the new weights!
  //place_pages_weighted(addr, len);
  move_pages_remote(addr, len, s);
}

// weighted placement with interleaving respecting s
void place_pages_weighted_s(void *addr, unsigned long len, double s) {
  int i = 0;

  if (weight_initialized == 0) {
    double new_s = 0;

    new_s = sum_ww + s;
    // Calculate new weights
    // printf("NODE Weights: \t");
    double sum = 0;

    for (i = 0; i < MAX_NODES; i++) {
      switch (OPT_NUM_WORKERS_VALUE) {
        case 1:
          // workers: 0
          if (nodes_info[i].id == 0) {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                (nodes_info[i].weight / sum_ww * new_s) * 10) / 10;
            //printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          } else {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                (nodes_info[i].weight / sum_nww * (100 - new_s)) * 10) / 10;
            //printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          }
          break;
        case 2:
          // workers: 1
          if (nodes_info[i].id == 1) {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                (nodes_info[i].weight / sum_ww * new_s) * 10) / 10;
            //printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          } else {
            nodes_info_temp[i].id = nodes_info[i].id;
            nodes_info_temp[i].weight = round(
                (nodes_info[i].weight / sum_nww * (100 - new_s)) * 10) / 10;
            //printf("%.2f\t", nodes_info_temp[i].weight);
            sum += nodes_info_temp[i].weight;
          }
          break;
          /*case 2:
           // workers: 0, 1
           if (nodes_info[i].id == 0 || nodes_info[i].id == 1) {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_ww * new_s) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           } else {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_nww * (100 - new_s)) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           }
           break;
           case 3:
           // workers: 1,2,3
           if (nodes_info[i].id == 1 || nodes_info[i].id == 2
           || nodes_info[i].id == 3) {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_ww * new_s) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           } else {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_nww * (100 - new_s)) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           }
           break;
           case 4:
           // workers: 0,1,2,3
           if (nodes_info[i].id == 0 || nodes_info[i].id == 1
           || nodes_info[i].id == 2 || nodes_info[i].id == 3) {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_ww * new_s) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           } else {
           nodes_info_temp[i].id = nodes_info[i].id;
           nodes_info_temp[i].weight = round(
           (nodes_info[i].weight / sum_nww * (100 - new_s)) * 10) / 10;
           //printf("%.2f\t", nodes_info_temp[i].weight);
           sum += nodes_info_temp[i].weight;
           }
           break;*/
        default:
          LINFOF("Sorry, %d Worker nodes is not supported at the moment!\n",
                 OPT_NUM_WORKERS_VALUE)
          ;
          exit(-1);
      }
    }

    /* printf("%.2f\n", sum);

     printf("NODE IDs: \t");
     for (i = 0; i < MAX_NODES; i++) {
     printf("%d\t", nodes_info_temp[i].id);
     }
     printf("\n");*/

    if ((check_sum(nodes_info_temp)) != 100) {
      printf("**Sum of New weights must be equal to 100, sum=%d!**\n",
             check_sum(nodes_info_temp));
      exit(-1);
    }

    /* printf(
     "===========================================================================\n");*/
    weight_initialized = 1;
  }

// Enforce the new weights!
  place_pages_weighted(addr, len);
}

void place_pages(void *addr, unsigned long len, double r) {
// compute the ratios to input to `mbind`
  double local_ratio = r - (1.0 - r) / (numa_num_configured_nodes() - 1);
  double interleave_ratio = 1.0 - local_ratio;

// compute the lengths of the interleaved and local segments
  unsigned long interleave_len = interleave_ratio * len;
  interleave_len &= PAGE_MASK;

  unsigned long local_len = (len - interleave_len) & PAGE_MASK;

// the starting address for the local segment
  void *local_addr = ((char*) addr) + interleave_len;

// validate input
  DIEIF(r < 0.0 || r > 1.0, "specified ratio must be between 0 and 1!");
  DIEIF(local_ratio < 0.0 || local_ratio > 1.0, "bad local_ratio calculation");
  DIEIF(interleave_ratio < 0.0 || interleave_ratio > 1.0,
        "bad interleave_ratio calculation");
  DIEIF(local_len % PAGE_SIZE != 0,
        "local length must be multiple of the page size");
  DIEIF(interleave_len % PAGE_SIZE != 0,
        "interleave length must be multiple of the page size");
  DIEIF((local_len + interleave_len) != (len & PAGE_MASK),
        "local and interleave lengths should total the original length");

// interleave some portion of the memory segment between all NUMA nodes
  /*LTRACEF("mbind(%p, %lu, MPOL_INTERLEAVE, numa_get_mems_allowed(), MPOL_MF_MOVE | MPOL_MF_STRICT)",
   addr, interleave_len);
   DIEIF(mbind(addr, interleave_len, MPOL_INTERLEAVE, numa_get_mems_allowed()->maskp,
   numa_get_mems_allowed()->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
   "mbind interleave failed");*/

// check if there is something left to bind to local
  if (local_len <= 0)
    return;

// bind the remainder to the local node
  struct bitmask *node_set = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();
  if (OPT_NUM_WORKERS_VALUE == 1) {
    //set the worker bitmask
    numa_bitmask_setbit(node_set, 0);
    DIEIF(
        WRAP(mbind)(local_addr, local_len, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
        "mbind interleave failed");
  } else if (OPT_NUM_WORKERS_VALUE == 2) {
    numa_bitmask_setbit(node_set, 0);
    numa_bitmask_setbit(node_set, 1);
    DIEIF(
        WRAP(mbind)(local_addr, local_len, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
        "mbind interleave failed");
  } else if (OPT_NUM_WORKERS_VALUE == 3) {
    numa_bitmask_setbit(node_set, 1);
    numa_bitmask_setbit(node_set, 2);
    numa_bitmask_setbit(node_set, 3);
    DIEIF(
        WRAP(mbind)(local_addr, local_len, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
        "mbind interleave failed");
  } else if (OPT_NUM_WORKERS_VALUE == 4) {
    numa_bitmask_setbit(node_set, 0);
    numa_bitmask_setbit(node_set, 1);
    numa_bitmask_setbit(node_set, 2);
    numa_bitmask_setbit(node_set, 3);
    DIEIF(
        WRAP(mbind)(local_addr, local_len, MPOL_INTERLEAVE, node_set->maskp, node_set->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
        "mbind interleave failed");
  }

  else {
    //do nothing!
  }

  numa_bitmask_free(node_set);
//unsigned long zero_mask = 0;
//LTRACEF("mbind(%p, %lu, MPOL_LOCAL, NULL, 0, MPOL_MF_MOVE | MPOL_MF_STRICT)",
//        local_addr, local_len);
//DIEIF(
//    WRAP(mbind)(local_addr, local_len, MPOL_LOCAL, &zero_mask, 8, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
//    "mbind local failed");
}

void place_pages(MemorySegment &segment, double ratio) {
//LINFOF("segment %s [%p:%p] length: %lu ratio: %lf", segment.name().c_str(), segment.startAddress(), segment.endAddress(), segment.length(), ratio);
// segment.print();
  //place_pages_weighted_s(segment.pageAlignedStartAddress(),
  //                       segment.pageAlignedLength(), ratio);
  //place_pages_weighted_dwp(segment.pageAlignedStartAddress(),
  //                         segment.pageAlignedLength(), ratio);
  move_pages_remote(segment.pageAlignedStartAddress(),
                    segment.pageAlignedLength(), ratio);
}

//place pages the adaptive way
void place_pages_adaptive(MemorySegment &segment, double ratio) {
// LDEBUGF("segment %s [%p:%p] ratio: %lf", segment.name().c_str(), segment.startAddress(), segment.endAddress(), ratio);
// segment.print();
  place_pages(segment.pageAlignedStartAddress(), segment.pageAlignedLength(),
              ratio);
}

/*
 * The next four functions handle the initial page placement at the time
 * of segment creation
 * as a replacement for the uniform interleave policy
 */
void place_pages_weighted_initial(const MemorySegment &segment) {
  if (segment.length() > 1ULL << 20) {
    // LINFOF("segment %s [%p:%p]", segment.name().c_str(), segment.startAddress(),
    //      segment.endAddress());
    //segment.print();
   // place_pages_weighted_initial(segment.pageAlignedStartAddress(),
   //                              segment.pageAlignedLength());
    move_pages_initial(segment.pageAlignedStartAddress(),
                                     segment.pageAlignedLength());
  }
}

void place_pages_weighted_contiguous(const MemorySegment &segment) {
  if (segment.length() > 1ULL << 20) {
    place_pages_weighted_contiguous(segment.pageAlignedStartAddress(),
                                    segment.pageAlignedLength());
  }
}

//weighted interleave with a contiguous memory mapping!
void place_pages_weighted_contiguous(void *addr, unsigned long len) {
  size_t size = len;
  void *start = addr;
  int i;
  pagesize = numa_pagesize();

// nodes that can still receive pages
  struct bitmask *node_set_initial = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();

  size_t total_size = 0;  // total size interleaved so far
  size_t my_size = 0;

  size_t remaining_a;

  for (i = 0; i < MAX_NODES; ++i) {
    if (total_size == size) {
      break;
    }

    numa_bitmask_setbit(node_set_initial, nodes_info[i].id);

    my_size = (nodes_info[i].weight / 100) * size;

    // round up to multiple of the page size
    my_size = PAGE_ALIGN_UP(my_size);

    remaining_a = size - total_size;
    if (my_size > remaining_a) {
      my_size = remaining_a;
    }
    total_size += my_size;

    // only mbind if memory is in the region
    if (my_size != 0) {
      DIEIF(
          WRAP(mbind)(start, my_size, MPOL_BIND, node_set_initial->maskp, node_set_initial->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
          "mbind interleave failed");

      start = reinterpret_cast<void*>(reinterpret_cast<intptr_t>(start)
          + my_size);  // increment base to a new location
    }
    if (numa_bitmask_isbitset(node_set_initial, nodes_info[i].id)) {
      numa_bitmask_clearbit(node_set_initial, nodes_info[i].id);  // remove node i from the set of remaining nodes
    }
  }

  numa_bitmask_free(node_set_initial);
}

//initial page placement with weighted interleave
void move_pages_initial(void *start, unsigned long len) {

  pagesize = numa_pagesize();

  char *pages;

  int i, j, rc;

  void **addr;
  int *status;
  int *nodes;

  int page_count = len / pagesize;

  addr = (void **) malloc(sizeof(char *) * page_count);
  status = (int *) malloc(page_count * sizeof(int *));
  nodes = (int *) malloc(page_count * sizeof(int *));

  if (!start || !addr || !status || !nodes) {
    LINFO("Unable to allocate memory");
    exit(1);
  }

  pages = (char *) start;

  //uniform distribution memory allocation (using the bwap style format)
  //first set the page addresses, openmp for faster processing
//#pragma omp parallel for firstprivate(pages,pagesize)
  for (i = 0; i < page_count; i++) {
    addr[i] = pages + i * pagesize;
    nodes[i] = 0;  //incase the last page is not initialized
  }

  //set the page distribution using a weighted version
  double i_p;  //interleaved_pages
  double w = 0;  // weight that has already been allocated among the nodes that can still receive pages
  int a = MAX_NODES;  // number of nodes which can still receive pages
  int i_k = 0;  //lower_bound for the pages
  int r_pages = 0;  //remaining pages
  int my_node;  //the node of a page

  //create a vector of node id's
  std::vector<int> node_ids;
  for (i = 0; i < MAX_NODES; i++) {
    node_ids.push_back(nodes_info[i].id);
  }

  for (i = 0; i < MAX_NODES; i++) {

    double b = nodes_info[i].weight - w;
    i_p = a * (b / 100) * page_count;

    r_pages = page_count - i_k;
    if (i_p > r_pages) {
      i_p = r_pages;
    }

    if (i_k == page_count) {
      break;
    }

    if (i_p != 0) {
      int upper_bound = i_k + i_p;
      for (j = i_k; j < upper_bound; j++) {
        my_node = j % a;
        nodes[j] = node_ids.at(my_node);
      }
    }

    node_ids.erase(node_ids.begin());
    a--;
    w = nodes_info[i].weight;
    i_k += i_p;

  }

  //get_node_mappings(page_count, nodes);
  rc = move_pages(0, page_count, addr, nodes, status, MPOL_MF_MOVE);
  if (rc < 0 && errno != ENOENT) {
    perror("move_pages");
    exit(1);
  }

  free(addr);
  free(status);
  free(nodes);
}

// interleave pages using the weights - use the initial weights!
void place_pages_weighted_initial(void *addr, unsigned long len) {
  size_t size = len;
  void *start = addr;
  int i;
  pagesize = numa_pagesize();
// printf("original size: %zu\n", size);

// nodes that can still receive pages
  struct bitmask *node_set_initial = numa_bitmask_alloc(MAX_NODES);  // numa_allocate_nodemask();
  numa_bitmask_setall(node_set_initial);

  float w = 0;  // weight that has already been allocated among the nodes that can still receive pages
  int a = MAX_NODES;  // number of nodes which can still receive pages

  size_t total_size = 0;  // total size interleaved so far
  size_t my_size = 0;

  size_t remaining_a;

  for (i = 0; i < MAX_NODES; ++i) {
    if (total_size == size) {
      break;
    }

    // b = size that remains to allocate in the next node with smallest beta
    float b = nodes_info[i].weight - w;
    // printf("i: %d\tb: %.2f\ta:%d\n", i, b, a);
    // if (b != 0) {
    my_size = a * (b / 100) * size;
    // printf("my_size_a: %ld\n", my_size);

    // round up to multiple of the page size
    my_size = PAGE_ALIGN_UP(my_size);

    remaining_a = size - total_size;
    if (my_size > remaining_a) {
      my_size = remaining_a;
    }
    // printf("my_size_b: %zu\n", my_size);
    total_size += my_size;

    // only interleave if memory is in the region
    if (my_size != 0) {
      // LDEBUGF("mbind(%p, %lu, INTERLEAVE, %lx, %zu, MOVE|STRICT)",
      //       start, my_size, *(node_set->maskp), node_set->size + 1);
      DIEIF(
          WRAP(mbind)(start, my_size, MPOL_INTERLEAVE, node_set_initial->maskp, node_set_initial->size + 1, MPOL_MF_MOVE | MPOL_MF_STRICT) != 0,
          "mbind interleave failed");
    }

    start =
        reinterpret_cast<void*>(reinterpret_cast<intptr_t>(start) + my_size);  // increment base to a new location
    a--;  // one less node where to allocate pages
    w = nodes_info[i].weight;  // we update the size already allocated in the remaining nodes
    if (numa_bitmask_isbitset(node_set_initial, nodes_info[i].id)) {
      numa_bitmask_clearbit(node_set_initial, nodes_info[i].id);  // remove node i from the set of remaining nodes
    }
  }

  numa_bitmask_free(node_set_initial);
}
//end initial page placement functions!

void place_all_pages(MemoryMap &segments, double ratio) {
  for (auto &segment : segments) {
    if (segment.length() > 1ULL << 20) {
      place_pages(segment, ratio);
    }
  }
//print_node_allocations();
  weight_initialized = 0;
}

//place pages the adaptive way!
void place_all_pages_adaptive(MemoryMap &segments, double ratio) {
  for (auto &segment : segments) {
    if (segment.length() > 1ULL << 20) {
      place_pages_adaptive(segment, ratio);
    }
  }
}

void place_all_pages_adaptive(double ratio) {
  LDEBUGF("place_pages with local ratio %lf", ratio);
  MemoryMap &segments = MemoryMap::getInstance();
  place_all_pages_adaptive(segments, ratio);
}

void place_all_pages(double ratio) {
  LDEBUGF("place_pages with local ratio %lf", ratio);
  MemoryMap &segments = MemoryMap::getInstance();
  place_all_pages(segments, ratio);
}

}  // namespace unstickymem
