#include <stdlib.h>
#include <stdbool.h>
#include "threads/palloc.h"
#include "vm/frame.h"
#include "vm/page.h"

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
