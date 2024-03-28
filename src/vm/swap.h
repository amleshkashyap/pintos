#ifndef VM_SWAP_H
#define VM_SWAP_H

/* each swap is for 1 page = (PGSIZE/BLOCK_SECTOR_SIZE) sectors */
struct swap {
  int pid;
  uint32_t *vaddr;
  int start_sector;
};

#endif /* vm/swap.h */
