#include "lib/debug.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "threads/malloc.h"
#include "userprog/syscall.h"
#include "threads/vaddr.h"
#include <string.h>
#include "userprog/pagedir.h"
#include "filesys/file.h"
#include <stdio.h>
#include "threads/thread.h"
#include "vm/swap.h"

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
  
  // Initialise the supplemental page table.
  hash_init (&supp_page_table->page_table, supp_hash_func, supp_hash_less, NULL);

  return supp_page_table;
}

/* Destroy supplemental page table. */
void destroy_supp_pt (struct supp_page_table *supp_page_table)
{
  hash_destroy (&supp_page_table->page_table, supp_destroy_func);
  free (supp_page_table);
}

/* Add a page to the supp_page_table with it's specified page_loc and file_info if page is from an EXECFILE. */ 
bool add_supp_pt (struct supp_page_table *supp_page_table, void *addr, void *faddr, enum page_loc from, struct file_struct *file_info)
{
  struct page *page = (struct page *) malloc(sizeof(struct page));
  if (!page)
  {
    exit_exception ();
  }

  // Set page struct members.
  page->address = addr;
  page->faddress = faddr;
  page->page_from = from;
  page->dirty_bit = false;
  page->file_info = file_info;

  struct hash_elem *old = hash_insert (&supp_page_table->page_table, &page->elem);
  // Return true and replace old page if the page already exists.
  if (old) 
  {
    struct page *old_page = hash_entry (old, struct page, elem);

    page->file_info->file_writeable = old_page->file_info->file_writeable || page->file_info->file_writeable;
    if (old_page->page_from == FRAME)
    {
      destroy_frame (old_page->faddress, true);
    }
    else if (old_page->page_from == EXECFILE)
    {
      free (old_page->file_info);
    }
    pagedir_clear_page (thread_current()->pagedir, old_page->address);
    hash_replace (&supp_page_table->page_table, &page->elem);
  }
  return true;
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


/* Add page with location of EXECFILE. */
bool add_file_supp_pt (struct supp_page_table *supp_page_table, void *addr,
    struct file *file, int32_t start_byte, uint32_t read_bytes, uint32_t zero_bytes, bool writeable)
{
  struct file_struct *file_info = (struct file_struct *) malloc(sizeof(struct file_struct));
  file_info->file = file;
  file_info->file_start_byte = start_byte;
  file_info->file_read_bytes = read_bytes;
  file_info->file_zero_bytes = zero_bytes;
  file_info->file_writeable = writeable;
  return add_supp_pt (supp_page_table, addr, NULL, EXECFILE, file_info);
}

/* Finds the page in the supp_page_table.
   If found it returns the page address, else returns NULL. */
struct page *find_page (struct supp_page_table *supp_page_table, void *page)
{
  // Create a page with the same hash key
  struct page temp_page;
  temp_page.address = page;

  struct hash_elem *e = hash_find (&supp_page_table->page_table, &temp_page.elem);
  if (!e) 
  {
    return NULL;
  }

  return hash_entry (e, struct page, elem);
}

/* Load page back on frame (into the memory). */
bool load_page (struct page *page, uint32_t *pagedir, void *address)
{
  // If the page has already been loaded it will be on FRAME
  // Else the page will be put in a new frame
  if(page->page_from == FRAME) 
  {  
    return true;
  }

  // Our attempt at sharing read-only files
  /*
  if(page->page_from == EXECFILE && !page->file_info->file_writeable)
  {
    struct hash_iterator frame_iterator;
    hash_first (&frame_iterator, &frame_table);

    while (hash_next (&frame_iterator))
    {
      // Obtain next frame from frame table
      struct frame *frame = hash_entry (hash_cur (&frame_iterator), struct frame, hash_elem);

      // If file in frame matches our file in page, share file
      if (frame->file_info->file == page->file_info->file)
        {
          frame->num_shared_pages++;
          page->faddress = frame->frame_address;
          return true;
        }
    }
  }
  */

  void *frame_page = get_new_frame(PAL_USER, address);
  if(!frame_page) 
  {
    return false;
  }

  // Fetch the data into the frame
  bool writeable = true;
  switch (page->page_from)
  {
    case ZERO:
      memset (frame_page, 0, PGSIZE);
      break;

    case SWAP:
      swap_read (page->swap_index, frame_page);
      break;

    case EXECFILE:
      file_seek (page->file_info->file, page->file_info->file_start_byte);

      size_t bytes_read = file_read (page->file_info->file, frame_page, page->file_info->file_read_bytes);
      if (bytes_read != page->file_info->file_read_bytes)
      {
        destroy_frame (frame_page, true);
        return false;
      }
      // The rest of the bytes are set to 0.
      memset (frame_page + bytes_read, 0, page->file_info->file_zero_bytes);

      writeable = page->file_info->file_writeable;
      break;

    default:
      PANIC ("Page type invalid or does not exist.");
      return false;
  }

  // Point the page table entry for the faulting virtual address to the physical page.
  if(!pagedir_set_page (pagedir, address, frame_page, writeable)) 
  {
    destroy_frame (frame_page, true);
    return false;
  }

  page->faddress = frame_page;
  page->page_from = FRAME;
  pagedir_set_dirty (pagedir, frame_page, false);

  return true;
}


bool unmap_supp_pt(struct supp_page_table *supp_page_table, uint32_t *pagedir,
    void *addr, struct file *f, uint32_t offset, size_t bytes)
{
  struct page *page = find_page (supp_page_table, addr);
  if (page == NULL) 
  {
    PANIC ("munmap - page is missing");
  }

  if (page->page_from == FRAME) 
  {
    set_used (page->faddress, true);
  }

  switch (page->page_from)
  {
    case FRAME:
    {
      // if address or mapped frame is dirty, write to file
      bool is_dirty = page->dirty_bit;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, page->address);
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, page->faddress);
      if (is_dirty) 
      {
        file_write_at (f, page->address, bytes, offset);
      }

      // destroy frame and clear page mapping
      destroy_frame (page->faddress, true);
      pagedir_clear_page (pagedir, page->address);
    }
      break;

    case SWAP:
    {
      bool is_dirty = page->dirty_bit;
      is_dirty = is_dirty || pagedir_is_dirty(pagedir, page->address);
      if (is_dirty) 
      {
        // load from swap and write back to file
        void *temp_page = palloc_get_page(0);
        swap_read (page->swap_index, temp_page);
        file_write_at (f, temp_page, PGSIZE, offset);
        palloc_free_page (temp_page);
      }
      else 
      {
        free_swap (page->swap_index);
      }
    }
      break;

    case EXECFILE:
      break;

    default:
      PANIC ("unreachable state");
  }

  // remove supplementary page table so unmapped memory is unreachable
  hash_delete(&supp_page_table->page_table, &page->elem);
  return true;
}

/* Helper functions for the supplemental page table hash map. */

static unsigned supp_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct page *page = hash_entry (elem, struct page, elem);
  return hash_int((int) page->address);
}

static bool supp_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct page *page_a = hash_entry (a, struct page, elem);
  struct page *page_b = hash_entry (b, struct page, elem);
  return page_a->address < page_b->address;
}

static void supp_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  struct page *page = hash_entry (e, struct page, elem);

  // Check the page_from and free based on the location
  if (page->page_from == FRAME)
  {
    destroy_frame (page->faddress, false);
  }
  else if (page->page_from == EXECFILE)
  {
    free (page->file_info);
  }
  else if (page->page_from == SWAP) 
  {
    free_swap (page->swap_index);
  }
  free (page);
}
