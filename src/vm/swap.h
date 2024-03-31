#ifndef VM_SWAP_H
#define VM_SWAP_H

/* each swap is for 1 page = (PGSIZE/BLOCK_SECTOR_SIZE) sectors */
struct swap {
  int pid;
  uint32_t *vaddr;
};

void init_swap_table (void);
size_t get_swapslot (void);
void map_to_swapslot (int, int, uint32_t *);
#endif /* vm/swap.h */
