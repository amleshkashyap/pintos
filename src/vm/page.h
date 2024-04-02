#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include "threads/thread.h"

void * get_user_page (bool);
void free_user_page (void *);
bool write_file_to_vaddr (mapid_t, enum vaddr_map_type, uint32_t *, int, int);
void write_back_to_file (mapid_t mapping);
void clear_vaddr_map_and_pte (mapid_t mapping);

#endif /* vm/page.h */
