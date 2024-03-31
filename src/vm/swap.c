#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "vm/swap.h"
#include "threads/malloc.h"
#include "threads/pte.h"
#include "devices/block.h"

static struct block *swapblock;
static uint32_t *swaplist;
static size_t swap_sectors;
static size_t swap_pages;
static size_t blocks_per_page;

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
    blocks_per_page = 1;
    swap_pages = swap_sectors;
  } else {
    blocks_per_page = PGSIZE/BLOCK_SECTOR_SIZE;
    if (PGSIZE % BLOCK_SECTOR_SIZE != 0) blocks_per_page += 1;
    swap_pages = swap_sectors/blocks_per_page;
  }

  printf ("Swap sectors are: %d, pages allowed in swap: %d\n", swap_sectors, swap_pages);
  swaplist = malloc (swap_pages * sizeof (struct swap));
}

/* assumes an unchangeable swap */
size_t
slot_to_sector (int slot)
{
  return (slot * blocks_per_page);
}

void *
get_from_swap (int pid, uint32_t *vaddr)
{
  struct swap *nswap;
  size_t start_sector;
  bool found;

  for (int i = 0; i < swap_pages; i++) {
    nswap = *(swaplist + i);
    if (nswap->pid == pid && nswap->vaddr == vaddr) {
      start_sector = slot_to_sector (i);
      found = true;
      break;
    }
  }

  if (!found) return NULL;
  /* start reading the page from swap blocks to a frame, clean the swap slot, acquire lock as needed */
  return NULL;
}

size_t
get_swapslot (void)
{
  for (int i = 0; i < swap_pages; i++) {
    if (*(swaplist + i) == 0) return i;
  }
  return NULL;
}

void
map_to_swapslot (int slot, int pid, uint32_t *vaddr)
{
  struct swap *nswap = malloc (sizeof (struct swap));
  nswap->pid = pid;
  nswap->vaddr = vaddr;
  *(swaplist + slot) = nswap;
}
