#include "vm/frame.h"
#include "vm/swap.h"
#include "lib/debug.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "userprog/pagedir.h"

static hash_hash_func frame_hash_func;
static hash_less_func frame_hash_less;
static struct frame *lookup_frame(void *frame_address);
static struct frame *evict_frame(uint32_t *pagedir);

// Frame table stored as a hash table
static struct hash frame_table;

// Frame lock to be acquried when accessing frame table to avoid race conditions
static struct lock frame_lock;

// Frame list for looping through frames in round of robin to find next victim
static struct list frame_list;
static struct list_elem *frame_pointer;

// Initialise frame table
void init_frames(void)
{
  lock_init(&frame_lock);
  hash_init(&frame_table, frame_hash_func, frame_hash_less, NULL);
  list_init(&frame_list);
  frame_pointer = NULL;
}

// Get new frame by calling palloc, and add frame to frame table
void *get_new_frame(enum palloc_flags flag, void *page_address)
{
  lock_acquire(&frame_lock);
  printf("get new frame\n");

  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->page_address = page_address;

  void *frame_address = palloc_get_page(PAL_USER | flag);

  if (frame_address == NULL)
  {
    printf("ft full\n");
    struct frame *evicted_frame = evict_frame(thread_current()->pagedir);
    pagedir_clear_page(evicted_frame->thread->pagedir, evicted_frame->page_address);

    bool is_dirty = pagedir_is_dirty(evicted_frame->thread->pagedir, evicted_frame->page_address) 
      || pagedir_is_dirty(evicted_frame->thread->pagedir, evicted_frame->frame_address);

    uint32_t index = swap_write(evicted_frame->frame_address);
    struct supp_page_table *supt = evicted_frame->thread->supp_page_table;
    struct page *page = find_page(supt, evicted_frame->page_address);
    page->page_from = SWAP;
    page->faddress = NULL;
    page->swap_index = index;
    page->dirty_bit = page->dirty_bit || is_dirty;

    destroy_frame(evicted_frame->frame_address, true);
    frame_address = palloc_get_page(PAL_USER | flag);
  }

  new_frame->frame_address = frame_address;
  new_frame->thread = thread_current();
  new_frame->used = true;
  hash_insert(&frame_table, &new_frame->hash_elem);
  list_push_back(&frame_list, &new_frame->list_elem);
  
  lock_release(&frame_lock);
  return frame_address;
}

/* Destroy a specified frame in frame table. */
void destroy_frame (void *frame_address, bool palloc_free)
{
  // lock_acquire (&frame_lock);

  struct frame *frame = lookup_frame (frame_address);
  hash_delete (&frame_table, &frame->hash_elem);
  list_remove(&frame->list_elem);
  if (palloc_free)
  {
    palloc_free_page (frame_address);
  }
  free (frame);

  // lock_release (&frame_lock);
}


/* Helper functions for frame table. */

void set_used (void *frame_address, bool new_used)
{
  lock_acquire (&frame_lock);
  lookup_frame (frame_address)->used = new_used;
  lock_release (&frame_lock);
}

// Choose a frame to evict when frame table is full using the second chance algorithm.
static struct frame *evict_frame(uint32_t *pagedir)
{
  size_t size = list_size(&frame_list);
  printf("size %d\n", size);
  if (frame_pointer == NULL)
  {
    frame_pointer = list_begin(&frame_list);
  }
  for (uint32_t i = 0; i < 2 * size; i++)
  {
    struct frame *frame = list_entry(frame_pointer, struct frame, list_elem);

    if (frame_pointer == list_end(&frame_list))
    {
      printf("beginning\n");
      frame_pointer = list_begin(&frame_list);
    }
    else
    {
      printf("get next\n");
      frame_pointer = list_next(frame_pointer);
    }

    if (frame->used)
    {
      printf("frame used\n");
      continue;
    }
    if (!pagedir_is_accessed(pagedir, frame->page_address))
    {
      printf("return frame\n");
      return frame;
    }
    printf("set accessed\n");
    pagedir_set_accessed(pagedir, frame->page_address, false);
  }

  return NULL;
}

// Lookup a frame in the hash table via its frame_address
static struct frame *lookup_frame(void *frame_address)
{
  struct frame search_frame;
  search_frame.frame_address = frame_address;
  struct hash_elem *frame_elem = hash_find(&frame_table, &search_frame.hash_elem);
  return hash_entry(frame_elem, struct frame, hash_elem);
}

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame *frame = hash_entry(elem, struct frame, hash_elem);
  return hash_bytes(&frame->frame_address, sizeof(frame->frame_address));
}

static bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
  struct frame *frame_a = hash_entry(a, struct frame, hash_elem);
  struct frame *frame_b = hash_entry(b, struct frame, hash_elem);
  return frame_a->frame_address < frame_b->frame_address;
}