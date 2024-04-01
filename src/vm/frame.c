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

/* each entry of array stores the address of a user frame - 1 frame per page
 * NOTE: there's no management for frame count, etc - if palloc was able to give a page,
 *   then physical memory is available - and physical memory is a deterministic entity
 *   hence a direct mapping to slot number is possible and utilised */
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
  /* evicts the first non-dirty frame for simplicity - causes pagefault if swap is empty
   * needs to acquire locks
   * TODO: if a page is selected for eviction, then it might become dirty/accessed while being written to swap
   *   for such a case, either disable interrupts while writing to swap, or invalidate immediately */
  int slot = -1;
  struct frame *frm;
  for (int i = 0; i < total_user_pages; i++) {
    frm = *(framelist + i);
    if (!pte_is_dirty (frm->pte) && !pte_is_accessed (frm->pte)) {
      slot = i;
      int swapslot = get_swapslot ();
      if (swapslot != -1) {
        map_and_write_to_swapslot (swapslot, frm->pid, frm->pte);
        *(framelist + i) = 0;
        pagedir_clear_page (get_thread_by_pid (frm->pid)->pagedir, frm->pte);
        free (frm);
        break;
      } else {
        /* swap is full */
        // PANIC ();
      }
    }
  }

  /* TODO: if all pages are accessed and dirty, let the page fault to continue? */
  if (slot == -1) {
    // PANIC ();
  }

  return slot;
}
