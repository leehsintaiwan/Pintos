#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include <bitmap.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *block_swap;
static struct bitmap *swap_bitmap; // Bitmap for checking availiable swap slots
static size_t swap_table_size; // The size of the swap table (number of swap slots)

// Initialise the swap table
void swap_init (void) 
{
    block_swap = block_get_role(BLOCK_SWAP);
    swap_table_size = block_size(block_swap) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(swap_table_size);
    bitmap_set_all(swap_bitmap, true);
}

// Read the content from the swap index, and store into "page"
void swap_read (uint32_t swap_index, void *page) 
{
  // Assert that the page and swap index are both valid  
  ASSERT (page >= PHYS_BASE);
  ASSERT (swap_index < swap_table_size);

  // Assert that the slot is not empty  
  if (bitmap_test(swap_bitmap, swap_index)) 
  {
    printf("ERROR: Attempted to read from empty swap slot");
    return;
  }

  // Read the slot at the index and store in page  
  for (int i = 0; i < SECTORS_PER_PAGE; i++) 
  {
    block_read (block_swap, swap_index * SECTORS_PER_PAGE + i, page + BLOCK_SECTOR_SIZE * i);
  }

  // Make the index availiable to write again  
  bitmap_set(swap_bitmap, swap_index, true);
}

// Write the content of "page" into swap table
uint32_t swap_write (void *page) 
{

}

// Free a swap entry
void free_swap (uint32_t swap_index) 
{

}

