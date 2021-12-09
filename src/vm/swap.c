#include "vm/swap.h"
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include <bitmap.h>
#include <stdio.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *block_swap;
static struct bitmap *swap_bitmap; // Bitmap for checking availiable swap slots
static size_t swap_table_size; // The size of the swap table (number of swap slots)
// struct lock swap_lock; // Swap table lock to prevent race conditions

// Initialise the swap table
void swap_init (void) 
{
  block_swap = block_get_role(BLOCK_SWAP);
  swap_table_size = block_size(block_swap) / SECTORS_PER_PAGE;
  swap_bitmap = bitmap_create(swap_table_size);
  bitmap_set_all(swap_bitmap, true);
  // lock_init(&swap_lock);
}

// Read the content from the swap index, and store into "page"
void swap_read (uint32_t swap_index, void *page) 
{
  // Assert that the page and swap index are both valid  
  ASSERT (page >= PHYS_BASE);
  ASSERT (swap_index < swap_table_size);

  // lock_acquire(&swap_lock);

  // Assert that the slot is not empty  
  if (bitmap_test(swap_bitmap, swap_index)) 
  {
    // lock_release(&swap_lock);
    PANIC ("Attempted to read from empty swap slot");
    return;
  }

  // Read the slot at the index and store in page  
  for (int i = 0; i < SECTORS_PER_PAGE; i++) 
  {
    block_read(block_swap, swap_index * SECTORS_PER_PAGE + i, page + BLOCK_SECTOR_SIZE * i);
  }

  // Make the index availiable to write again  
  free_swap(swap_index);

  // lock_release(&swap_lock);
}

// Write the content of "page" into swap table and return the index in which it is stored
uint32_t swap_write (void *page) 
{
  // Assert that the page is valid
  ASSERT (page >= PHYS_BASE);

  // lock_acquire(&swap_lock);

  /* 
    Search for an available swap slot index to write into table,
    then flip the index to false to indicate occupation
  */
  uint32_t swap_index = bitmap_scan_and_flip(swap_bitmap, 0, 1, true);

  if (swap_index == BITMAP_ERROR) 
  {
    // lock_release(&swap_lock);
    PANIC ("Swap table full");
    return swap_index;
  }

  // Write the content into the swap slot  
  for (int i = 0; i < SECTORS_PER_PAGE; ++ i) 
  {
    block_write(block_swap, swap_index * SECTORS_PER_PAGE + i, page + BLOCK_SECTOR_SIZE * i);
  }

  // lock_release(&swap_lock);

  return swap_index;
}

// Free a swap slot
void free_swap (uint32_t swap_index) 
{
  // Assert that the swap index is valid
  ASSERT (swap_index < swap_table_size);

  // Assert that the swap slot is not empty  
  if (bitmap_test(swap_bitmap, swap_index)) 
  {
    PANIC ("Attempted to free an empty swap slot");
    return;
  }

  // Indicate availiability for the index
  bitmap_set(swap_bitmap, swap_index, true);
}

