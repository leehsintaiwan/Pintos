#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdint.h>
#include <inttypes.h>

void swap_init (void);
void swap_read (uint32_t swap_index, void *page);
uint32_t swap_write (void *page);
void free_swap (uint32_t swap_index);

static struct swap_table 
{
    // TO-DO
};

struct swap_slot 
{
    // TO-DO
};

#endif