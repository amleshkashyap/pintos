#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "threads/synch.h"
#include "devices/block.h"

/* NOTE: since swap tables must store virtual address which is process specific, it is not
 *   possible to have a direct mapping to some swap slot - however, an efficient alternative to
 *   linear search can be to hash to certain index based on the vaddr and then search linearly
 * NOTE: swap is a global datastore without a direct mapping, hence a lock is required (unlike fd or frame tables) */

static struct block *swapblock;
static uint32_t *swaplist;
static size_t swap_sectors;
static size_t swap_pages;
static size_t sectors_per_page;
static size_t allocated_slots;
static struct lock swaplock;

/* if block is larger than page, then 1 page/block 
 * if pagesize % blocksize = 0 , then no wasted space
 * else, wasted space per page, and possibly at the end of swap */
void
init_swap_table (void)
{
  swapblock = block_get_role (BLOCK_SWAP);
  if (swapblock == NULL) {
    printf("No swap device: panic?\n");
    return;
  }
  swap_sectors = block_size (swapblock);

  if (BLOCK_SECTOR_SIZE >= PGSIZE) {
    sectors_per_page = 1;
    swap_pages = swap_sectors;
  } else {
    sectors_per_page = PGSIZE/BLOCK_SECTOR_SIZE;
    if (PGSIZE % BLOCK_SECTOR_SIZE != 0) sectors_per_page += 1;
    swap_pages = swap_sectors/sectors_per_page;
  }

  printf ("Swap sectors are: %d, pages allowed in swap: %d\n", swap_sectors, swap_pages);
  swaplist = malloc (swap_pages * sizeof (struct swap));
  lock_init (&swaplock);
}

/* assumes an unchangeable swap */
size_t
slot_to_sector (int slot)
{
  return (slot * sectors_per_page);
}

int
find_in_swap (pid_t pid, uint32_t *vaddr)
{
  struct swap *nswap;
  int slot = -1;
  bool found;

  for (int i = 0; i < swap_pages; i++) {
    nswap = *(swaplist + i);
    if (nswap->pid == pid && nswap->vaddr == vaddr) {
      slot = i;
      break;
    }
  }

  return slot;
}

bool
get_from_swap (pid_t pid, uint32_t *vaddr, void *buffer)
{
  int slot = find_in_swap (pid, vaddr);
  if (slot == -1) return false;

  size_t start_sector = slot_to_sector (slot);
  int counter = 0; 
  /* starts reading page from swap sector, write it to vaddr */
  // TODO: allocate vaddr with lock
  for (int i = start_sector; i < start_sector + sectors_per_page; i++) {
    block_read (swapblock, i, buffer + counter * BLOCK_SECTOR_SIZE);
  }

  /* cleanup swapslot */
  free_swapslot (slot);

  return true;
}

int
get_swapslot (void)
{
  if (allocated_slots >= swap_pages) {
    /* TODO: cause a page fault */
    printf("swapblock is full\n");
    return -1;
  }

  int slot = -1;

  lock_acquire (&swaplock);
  for (int i = 0; i < swap_pages; i++) {
    if (*(swaplist + i) == 0) {
      allocated_slots++;
      slot = i;
      break;
    }
  }

  lock_release (&swaplock);
  return slot;
}

/* no lock is required */
void
free_swapslot (int slot)
{
  struct swap *nswap = *(swaplist + slot);
  *(swaplist + slot) = 0;
  allocated_slots--;
  free (nswap);
}

void
map_and_write_to_swapslot (int slot, pid_t pid, uint32_t *vaddr)
{
  // printf("mapping and write to swapslot: %d, pid: %d, addr: %p\n", slot, pid, vaddr);
  struct swap *nswap = malloc (sizeof (struct swap));
  nswap->pid = pid;
  nswap->vaddr = vaddr;
  *(swaplist + slot) = nswap;

  size_t start_sector = slot_to_sector (slot);
  int counter = 0;

  struct thread *t = get_thread_by_pid (pid);
  void *page = pagedir_get_page (t->pagedir, vaddr);

  for (int i = start_sector; i < start_sector + sectors_per_page; i++) {
    /* write vaddr to sector 0 (for 512 bytes), vaddr + 512 to sector 1, etc*/
    block_write (swapblock, i, page + counter * BLOCK_SECTOR_SIZE);
    counter += 1;
  }
}
