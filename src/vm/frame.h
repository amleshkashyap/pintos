#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "threads/palloc.h"
#include "threads/pte.h"
#include <stdbool.h>
#include <stdint.h>
#include <list.h>

#define FRLINESIZE 4
#define FRPERPAGE PGSIZE/FRLINESIZE

// 56 bytes
struct frame {
  uint32_t *address;      // physical memory address
  uint32_t *pte;          // PTE for the frame
  int pid;                // primary holder process
  int shared;             // number of processes shared with
  int shared_pids[10];    // pid of upto 10 processes
};

size_t paddr_to_slot (void *);
void map_frame (void *, void *);
bool clear_frame (void *);
size_t evict_page (void);
void init_frame_table (void);

#endif /* vm/frame.h */
