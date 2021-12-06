#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <hash.h>
#include "lib/kernel/hash.h"
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
  EXECFILE /* In filesys/executable. */
};


struct supp_page_table {
    /* Map from page to the supplemental page table entry. */
    struct hash page_table;
};

/* A page is an entry of the supp_page_table. */
struct page {
    void *address; /* Virtual address of the page. */
    void *faddress; /* Frame address of the page (location in the kernel). */
    enum page_loc page_from;


    bool dirty_bit;

    // When page loc is SWAP
    uint32_t swap_index;

    // When page loc is FILE
    struct file_struct *file_info;

    struct hash_elem elem; 
};

struct file_struct {
  struct file *file; /* Executable file. */
  int32_t file_start_byte; /*  Offset in the file to start read. */
  size_t file_read_bytes; /* Number of bytes to read. */
  size_t file_zero_bytes; /* Number of trailing 0 bytes. */
  bool file_writeable;  /* Is file writeable (based on segment being read). */
};


struct supp_page_table *init_supp_page_table (void);
void destroy_supp_pt (struct supp_page_table *supp_page_table);
bool add_frame_supp_pt (struct supp_page_table *supp_page_table, void *addr, void *faddr);
bool add_zero_supp_pt (struct supp_page_table *supp_page_table, void *addr);
bool set_swap_supp_pt (struct supp_page_table *supp_page_table, void *page_addr, uint32_t swap_index);
bool add_file_supp_pt (struct supp_page_table *supp_page_table, void *addr,
    struct file *file, int32_t start_byte, uint32_t read_bytes, uint32_t zero_bytes, bool writeable);
struct page *find_page (struct supp_page_table *supp_page_table, void *page);
bool load_page (struct supp_page_table *supp_page_table, uint32_t *pagedir, void *address);
bool add_supp_pt (struct supp_page_table *supp_page_table, void *addr, void *faddr, enum page_loc from, struct file_struct *file_info);
bool unmap_supp_pt(struct supp_page_table *supp_page_table, uint32_t *pagedir,
    void *addr, struct file *f, uint32_t offset, size_t bytes);




#endif