#include <stdlib.h>
#include <stdbool.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"
/* there's no compilation errors for unknown function calls, and a proper runtime error is not present */
#include "userprog/pagedir.h"

void *
get_user_page (bool zero)
{
  enum palloc_flags flag = zero ? (PAL_USER | PAL_ZERO) : PAL_USER;
  uint8_t *kpage = palloc_get_page (flag);
  if (kpage == NULL) {
    /* evict a page from the framelist, free it from userpool and retry palloc method
     * alternatively, the evicted page is not freed from userpool and slot number can be used directly */
    evict_page ();
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
write_file_to_vaddr (mapid_t mapping, enum vaddr_map_type mtype, uint32_t *addr, int filesize, int fd)
{
  /* handle freeing of pages if all pages can't be acquired, and then free the map */
  uint32_t *page;
  struct thread *cur = thread_current ();
  int pages = filesize / PGSIZE + 1;
  for (int i = 0; i < pages; i++) {
    page = get_user_page (false);
    if (page == NULL) return false;
    pagedir_set_page (cur->pagedir, addr + i * (PGSIZE/4), page, true);
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
    pagedir_set_dirty (cur->pagedir, addr + i * PGSIZE, false);
  }
  return true;
}

void
clear_vaddr_map_and_pte (mapid_t mapping)
{
  struct thread *cur = thread_current ();
  /* it's a thread method - TODO: place the methods as required */
  struct vaddr_map *vmap = cur->vaddr_mappings[mapping];
  int pages = (vmap->evaddr - vmap->svaddr) / PGSIZE;
  for (int i = 0; i < pages; i++) {
    pagedir_clear_page (cur->pagedir, vmap->svaddr + i * (PGSIZE/4));
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

void *
bring_from_swap (pid_t pid, uint32_t *vaddr)
{
  void *kpage = get_user_page (false);
  if (kpage == NULL) {
    return false;
  }

  if (get_from_swap (pid, vaddr, kpage) == false) {
    return false;
  }

  return true;
}
