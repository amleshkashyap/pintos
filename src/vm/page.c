#include <stdlib.h>
#include <stdbool.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
/* there's no compilation errors for unknown function calls, and a proper runtime error is not present */
#include "userprog/pagedir.h"

/* This module serves as an intermediary between user page accesses (pagedir.c) and user programs (process.c, thread.c)
 *   all user memory allocations (outside of basic setup) for mmap and stack increment must go via this
 *   all virtual address accesses and updates go via this module
 */

void *
get_user_page (bool zero)
{
  enum palloc_flags flag = zero ? (PAL_USER | PAL_ZERO) : PAL_USER;
  uint8_t *kpage = palloc_get_page (flag);
  if (kpage == NULL) {
    /* evict a page from the framelist, free it from userpool and retry palloc method
     * alternatively, the evicted page is not freed from userpool and slot number can be used directly */
    int slot = evict_page ();
    kpage = palloc_get_page (flag);
  }
  return kpage;
}

void
free_user_page (void *paddr)
{
  palloc_free_page (paddr);
}

bool
is_overlapping_vaddr (void *vaddr)
{
  struct thread *cur = thread_current ();
  if (cur->active_vaddr_maps > 0) {
    struct vaddr_map *vmap;
    for (int i = 0; i < MAX_VADDR_MAPS; i++) {
      if (cur->vaddr_mappings[i] != NULL) {
        vmap = cur->vaddr_mappings[i];
        if (vaddr >= vmap->svaddr && vaddr <= vmap->evaddr) return true;
      }
    }
  }
  return false;
}

/* this might be a restrictive way to handle stack space, and the error message in mmap
 *   should suggest to the user that they can't use this vaddr */
bool
is_stack_vaddr (void *vaddr)
{
  return vaddr >= (PHYS_BASE - MAX_STACK_PAGES * PGSIZE);
}

bool
is_code_segment (void *vaddr)
{
  return (vaddr >= thread_current ()->code_segment && vaddr <= thread_current ()->end_code_segment);
}

bool
is_data_segment (void *vaddr)
{
  return (vaddr >= thread_current ()->end_code_segment && vaddr <= thread_current ()->data_segment);
}

bool
is_mappable_vaddr (void *vaddr)
{
  if (vaddr == 0 || vaddr == NULL) return false;
  if ((uint32_t) vaddr % PGSIZE != 0) return false;
  if (is_stack_vaddr (vaddr)) return false;     /* trying to overwrite reserved stack pages */

  struct thread *cur = thread_current ();
  if (is_code_segment (vaddr) || is_data_segment (vaddr)) return false;
  if (is_overlapping_vaddr (vaddr)) return false;   /* any overlaps with other mappings, stack pages or load time pages */
  return true;
}

mapid_t
allocate_vaddr_mapid (void)
{
  struct thread *cur = thread_current ();
  if (cur->active_vaddr_maps == MAX_VADDR_MAPS) return -1;

  for (int i = 0; i < MAX_VADDR_MAPS; i++) {
    if (cur->vaddr_mappings[i] == NULL) {
      cur->active_vaddr_maps++;
      return i;
    }
  }

  return -1;
}

void
free_vaddr_map (mapid_t mapid)
{
  struct thread *cur = thread_current ();
  struct vaddr_map *vmap = cur->vaddr_mappings[mapid];
  cur->vaddr_mappings[mapid] = NULL;
  cur->active_vaddr_maps--;
  free (vmap);
}

void
set_vaddr_map (mapid_t mapid, enum vaddr_map_type mtype, uint32_t *vaddr, int filesize, int fd)
{
  struct vaddr_map *vmap = malloc (sizeof (struct vaddr_map));
  int pages = get_pages_for_size (filesize);
  vmap->mtype = mtype;
  vmap->svaddr = vaddr;
  vmap->evaddr = INCR_VADDR (vaddr, pages);
  vmap->fd = fd;
  vmap->filesize = filesize;
  thread_current ()->vaddr_mappings[mapid] = vmap;
}

bool
write_file_to_vaddr (mapid_t mapping, enum vaddr_map_type mtype, uint32_t *addr, int filesize, int fd)
{
  /* handle freeing of pages if all pages can't be acquired, and then free the map */
  uint32_t *page;
  struct thread *cur = thread_current ();
  int pages = get_pages_for_size (filesize);

  for (int i = 0; i < pages; i++) {
    page = get_user_page (false);
    if (page == NULL) return false;
    pagedir_set_page (cur->pagedir, INCR_VADDR (addr, i), page, true);
  }


  /* TODO: multipage mmap writes */
  /* TODO: possible circular dependency with syscall.c, also in write () below */
  page = pagedir_get_page (cur->pagedir, addr);

  /* use this later */
  // unsigned file_seek = tell (fd);
  int bytes_read = read (fd, addr, filesize);
  if (bytes_read != filesize) {
    // do something
  }
  set_vaddr_map (mapping, MAP_USER_FILES, addr, filesize, fd);

  /* mark page as not dirty */
  for (int i = 0; i < pages; i++) {
    pagedir_set_dirty (cur->pagedir, INCR_VADDR (addr, i), false);
  }
  return true;
}

void
clear_vaddr_map_and_pte (mapid_t mapping)
{
  struct thread *cur = thread_current ();
  /* it's a thread method - TODO: place the methods as required */
  struct vaddr_map *vmap = cur->vaddr_mappings[mapping];
  int pages = get_pages_for_size (vmap->filesize);
  for (int i = 0; i < pages; i++) {
    pagedir_clear_page (cur->pagedir, INCR_VADDR (vmap->svaddr, i), false);
  }
  free_vaddr_map (mapping);
}

/* merge with above method to avoid fetching the same variables */
void
write_back_to_file (mapid_t mapping)
{
  struct thread *cur = thread_current ();
  struct vaddr_map *vmap = cur->vaddr_mappings[mapping];

  if (pagedir_is_dirty (cur->pagedir, vmap->svaddr)) {
    /* TODO: handle multipage mmap writes */
    void *page = pagedir_get_page (cur->pagedir, vmap->svaddr);
    seek (vmap->fd, 0); /* TODO: should the seek be handled in read/write? */

    int bytes_written = write (vmap->fd, (const void *) vmap->svaddr, vmap->filesize);
    if (bytes_written != vmap->filesize) {
      // do something
    }
    seek (vmap->fd, 0);
  }
}

bool
allocate_next_stack_page (void)
{
  struct thread *cur = thread_current ();
  if (cur->allocated_stack_pages >= MAX_STACK_PAGES) return false;
  void *page = get_user_page (false);
  if (page == NULL) return false;
  cur->allocated_stack_pages++;
  pagedir_set_page (cur->pagedir, PHYS_BASE - (cur->allocated_stack_pages * PGSIZE), page, true);
  return true;
}

void *
bring_from_swap (pid_t pid, uint32_t *vaddr)
{
  void *kpage = get_user_page (false);
  if (kpage == NULL) {
    return false;
  }

  if (get_from_swap (pid, (uint32_t *) pg_round_down (vaddr), kpage) == false) return false;

  pagedir_set_page (thread_current ()->pagedir, pg_round_down (vaddr), kpage, true);

  return true;
}
