#include "page.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"


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


/* Add page with location FRAME to supplemental page table. */
bool add_frame_supp_pt (struct supp_page_table *supp_page_table, void *addr)
{
  struct page *page = (struct page *) malloc(sizeof(struct page));

  // Set page attributes.
  page->address = addr;
  page->page_from = FRAME;
  page->dirty_bit = false;

  struct hash_elem *prev_elem = hash_insert (&supp_page_table->page_table, &page->elem);
  // Return true if successful and false if the page already exists (unsuccessful).
  if (!prev_elem) {
    return true;
  }
  else 
  {
    free (page);
    return false;
  }
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