#ifndef VM_SWAP_H
#define VM_SWAP_H

#include "threads/thread.h"

/* each swap is for 1 page = (PGSIZE/BLOCK_SECTOR_SIZE) sectors */
struct swap {
  int pid;
  uint32_t *vaddr;
};

void init_swap_table (void);
int get_swapslot (void);
bool get_from_swap (pid_t, uint32_t *, void *);
int find_in_swap (pid_t, uint32_t *);
void map_and_write_to_swapslot (int, pid_t, uint32_t *);
#endif /* vm/swap.h */
