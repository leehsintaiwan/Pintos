#include "lib/debug.h"
#include "page.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "vaddr.h"


static hash_hash_func supp_hash_func;
static hash_less_func supp_hash_less;
static hash_action_func supp_destroy_func;

/* Create supplemental page table */
struct supp_page_table *init_supp_page_table (void)
{
  struct supp_page_table *supp_page_table = 
    (struct supp_page_table*) malloc (sizeof (struct supp_page_table));

  // If there is no memory to allocate then fail.
  if (!supp_page_table)
  {
    exit_exception ();
  }
  
  // Initialise the supplemtnale page table.
  hash_init (&supp_page_table->page_table, supp_hash_func, supp_hash_less, NULL);
  
  return supp_page_table;
}

/* Destroy supplemental page table. */
void destroy_supp_pt (struct supp_page_table *supp_page_table)
{
  hash_destroy (&supp_page_table->page_table, supp_destroy_func);
  free (supp_page_table);
}

bool add_supp_pt (struct supp_page_table *supp_page_table, void *addr, void *faddr, enum page_loc from, struct file_struct *file_info)
{
  struct page *page = (struct page *) malloc(sizeof(struct page));
  if (!page)
  {
    exit_exception ();
  }

  // Set page attributes.
  page->address = addr;
  page->faddress = faddr;
  page->page_from = from;
  page->dirty_bit = false;
  page->file_info = file_info;

  struct hash_elem *old = hash_insert (&supp_page_table->page_table, &page->elem);
  // Return true if successful and false if the page already exists (unsuccessful).
  if (!old) {
    return true;
  }
  else 
  {
    free (page);
    return false;
  }

}

/* Add page with location FRAME to supplemental page table. */
bool add_frame_supp_pt (struct supp_page_table *supp_page_table, void *addr, void *faddr)
{
  return add_supp_pt (supp_page_table, addr, faddr, FRAME, NULL);
}

/* Add page with page_loc ZERO to supplemental page table.
   This means all the bytes in this page are set to zero. */
bool add_zero_supp_pt (struct supp_page_table *supp_page_table, void *addr)
{
  return add_supp_pt (supp_page_table, addr, NULL, ZERO, NULL);
}

/* Get a page from the supp_page_table and prepare it for swapping. */
bool set_swap_supp_pt (struct supp_page_table *supp_page_table, void *page_addr, uint32_t swap_index)
{
  struct page *page = find_page(supp_page_table, page_addr);
  if (!page)
  {
    return false;
  }
  page->faddress = NULL;
  page->page_from = SWAP;
  page->swap_index = swap_index;
  return true;
}


/* Add page with location of FILE. */
bool add_file_supp_pt (struct supp_page_table *supp_page_table, void *addr,
    struct file *file, int32_t start_byte, uint32_t read_bytes, bool writeable)
{

  struct file_struct *file_info = (struct file_struct *) malloc(sizeof(struct file_struct));
  file_info->file = file;
  file_info->file_start_byte = start_byte;
  file_info->file_read_bytes = read_bytes;
  file_info->file_writeable = writeable;

  return add_supp_pt (supp_page_table, addr, NULL, FILE, file_info);
}

/* Finds the page in the supp_page_table.
   If found it returns the page address, else returns NULL. */
struct page *find_page (struct supp_page_table *supp_page_table, void *page)
{
  // Create a page with the same hash key
  struct page temp_page;
  temp_page.address = page;

  struct hash_elem *elem = hash_find (&supp_page_table->page_table, &temp_page.elem);
  if (elem) 
  {
    return hash_entry (elem, struct page, elem);
  }
  else
  {
    return NULL;
  }
}

/* Load page back on frame (into the memory). */
bool load_page (struct supp_page_table *supp_page_table, uint32_t *pagedir, void *address)
{
  /* see also userprog/exception.c */

  struct page *page = find_page(supp_page_table, address);
  if(!page) {
    return false;
  }

  // Already loaded
  if(page->page_from == FRAME) {  
    return true;
  }

  // Get new frame for page
  void *frame_page = get_new_frame(PAL_USER, address);
  if(!frame_page) {
    return false;
  }

  // Fetch the data into the frame
  bool writeable = true;
  switch (page->page_from)
  {
  case ZERO:
    memset (frame_page, 0, PGSIZE);
    break;

  case FRAME:
    /* nothing to do */
    break;

  case SWAP:
    // Swap in: load the data from the swap disc

    // TODO: Implement swap in
    break;

  case FILE:
    // if( vm_load_page_from_filesys(spte, frame_page) == false) {
    //   vm_frame_free(frame_page);
    //   return false;
    // }

    // writeable = page->writeable;
    break;

  default:
    PANIC ("Page type does not exist.");
  }

  // 4. Point the page table entry for the faulting virtual address to the physical page.
  if(!pagedir_set_page (pagedir, upage, frame_page, writable)) {
    vm_frame_free(frame_page);
    return false;
  }

  // Make SURE to mapped kpage is stored in the SPTE.
  spte->kpage = frame_page;
  spte->status = ON_FRAME;

  pagedir_set_dirty (pagedir, frame_page, false);

  // unpin frame
  vm_frame_unpin(frame_page);

  return true;
}

/* Helper functions for the supplemental page table hash map. */

static unsigned supp_hash_func(const struct hash_elem *e, void *aux UNUSED)
{
  struct page *page = hash_entry (e, struct page, elem);
  return hash_bytes(&page->address, sizeof(page->address));
}

static bool supp_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct page *page_a = hash_entry (a, struct page, elem);
  struct page *page_b = hash_entry (b, struct page, elem);
  return page_a->address < page_b->address;
}

static void supp_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  struct supp_pt_entry *entry = hash_entry (e, struct page, elem);

  // Check the page_from and free based on the location
  
  free (entry);
}
