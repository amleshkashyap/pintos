#ifndef VM_PAGE_H
#define VM_PAGE_H

/* TODO: how can one write a tool to allow configuring this? */
#define MAX_STACK_PAGES 32
#define INCR_VADDR(vaddr, i) (void *) vaddr + i * PGSIZE    /* increment as a 1-byte pointer instead of 4-byte uint32_t */

#include <stdbool.h>
#include "threads/thread.h"

void * get_user_page (bool);
void free_user_page (void *);
bool is_stack_vaddr (void *);
bool write_file_to_vaddr (mapid_t, enum vaddr_map_type, uint32_t *, int, int);
void write_back_to_file (mapid_t mapping);
void clear_vaddr_map_and_pte (mapid_t mapping);

bool allocate_next_stack_page (void);

/* for mmap */
bool is_mappable_vaddr (void *);
mapid_t allocate_vaddr_mapid (void);
void free_vaddr_map (mapid_t);
void set_vaddr_map (mapid_t, enum vaddr_map_type, uint32_t *, int, int);

#endif /* vm/page.h */
