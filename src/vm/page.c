#include <stdlib.h>
#include <stdbool.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"
#include "vm/swap.h"

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
write_file_to_vaddr (mapid_t mapping, enum vaddr_map_type mtype, uint32_t *addr, int pages, int fd)
{
  /* handle freeing of pages if all pages can't be acquired, and then free the map */
  void *page;
  for (int i = 0; i < 1; i++) {
    page = get_user_page (true);
    if (page == NULL) return false;
  }

  pagedir_set_page (thread_current ()->pagedir, addr, page, true);

  read (fd, page, PGSIZE);
  set_vaddr_map (mapping, MAP_USER_FILES, addr, pages, fd);
  return true;
}

void *
bring_from_swap (pid_t pid, uint32_t *vaddr)
{
  void *kpage = get_user_page (true);
  if (kpage == NULL) {
    return false;
  }

  if (get_from_swap (pid, vaddr, kpage) == false) {
    return false;
  }

  return true;
}
