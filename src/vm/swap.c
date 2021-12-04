#include "vm/swap.h"
#include "devices/block.h"
#include <bitmap.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

static struct block *block_swap;
static struct bitmap *swap_bitmap; // bitmap for checking availiable swap slots

// Initialise the swap table
void swap_init (void) 
{
    block_swap = block_get_role(BLOCK_SWAP);
    size_t swap_size = block_size(block_swap) / SECTORS_PER_PAGE;
    swap_bitmap = bitmap_create(swap_size);
    bitmap_set_all(swap_bitmap, true);
}

// Read the content from the swap index, and store into "page"
void swap_read (uint32_t swap_index, void *page) 
{

}

// Write the content of "page" into swap table
uint32_t swap_write (void *page) 
{

}

// Free a swap entry
void free_swap (uint32_t swap_index) 
{

}

