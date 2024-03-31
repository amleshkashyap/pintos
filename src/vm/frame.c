#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include "vm/frame.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"
#include "threads/thread.h"

static size_t total_user_pages;
static void *user_pool_base;

/* each entry of array stores the address of a user frame - 1 frame per page */
static uint32_t *framelist;

void
init_frame_table (void)
{
  user_pool_base = get_userpool_base ();
  total_user_pages = get_user_pages ();
  /* acquires kernel memory - non-pageable as of now */
  framelist = malloc (total_user_pages * sizeof (uint32_t *));
  return;
}

/* get the slot number by counting offset for paddr from base of user_pool
 * NOTE: this is for speeding up the CRUD operations, doesn't help with eviction */
size_t
paddr_to_slot (void *paddr)
{
  return (size_t) ((paddr - user_pool_base)/PGSIZE);
}

bool
clear_frame (void *address)
{
  size_t slot = paddr_to_slot (address);
  struct frame *frm = *(framelist + slot);
  *(framelist + slot) = 0;
  free (frm);
  return true;
}

void
map_frame (void *address, void *pte)
{
  /* get a frame from kernel pool - address of this frame will be stored in frame table */
  struct frame *nframe = malloc (sizeof (struct frame));
  nframe->address = address;
  nframe->pte = pte;
  nframe->pid = thread_current ()->pid;

  size_t slot = paddr_to_slot (address);
  *(framelist + slot) = nframe;
}

size_t
evict_page (void)
{
  /* evicts the first non-dirty frame for simplicity. needs to acquire locks
   * TODO: check if it's a accessed page, lock the movement */
  int slot = 0;
  struct frame *frm;
  for (int i = 0; i < total_user_pages; i++) {
    frm = *(framelist + i);
    if (!pte_is_dirty (frm->pte)) {
      slot = i;
      *(framelist + i) = 0;
      pagedir_clear_page (get_thread_by_pid (frm->pid)->pagedir, frm->pte);
      free (frm);
      break;
    }
  }
  return slot;
}
