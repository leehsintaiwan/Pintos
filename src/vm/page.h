#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <hash.h>



/* The different locations/states
   the page can be in. */
enum page_loc {
  FRAME, /* In memory. */
  ZERO, /* Zeros. */
  SWAP, /* In swap slot. */
  FILE /* In filesys or executable. */
};


struct supp_page_table {
    /* Map from page to the supplemental page table entry. */
    struct hash page_table;
};

/* A page is an entry of the supp_page_table. */
struct page {
    void *address; /* Virtual address of the page. */

    enum page_loc page_from;

    bool dirty_bit;



    bool read_only;
    struct hash_elem elem; 
};


struct supp_page_table *init_supp_page_table (void);
void destroy_supp_pt (struct supp_page_table *supp_page_table);
bool add_frame_supp_pt (struct supp_page_table *supp_page_table, void *addr);



#endif