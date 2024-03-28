#include <stdbool.h>
#include <stdint.h>
#include "threads/palloc.h"

static uint32_t *swaplist;
static size_t swapslots;

void
init_swap_table (void)
{
  /* assumes that this much space will be available in filesystem */
  swapslots = get_user_pages () / 2;
  swaplist = malloc (swapslots * sizeof (struct swap));
}
