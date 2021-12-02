#include "vm/frame.h"
#include "lib/debug.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"

static hash_hash_func frame_hash_func;
static hash_less_func frame_hash_less;
static hash_action_func frame_destroy_func;
static struct frame *lookup_frame(void *frame_address);

static struct frame_table frames;

// Initialise frame table
void init_frames(void)
{
  lock_init(&frames.lock);
  hash_init(&frames.table, frame_hash_func, frame_hash_less, NULL);
}

// Get new frame by calling palloc, and add frame to frame table
void *get_new_frame(enum palloc_flags flag, void *page_address)
{
  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->page_address = page_address;

  void *frame_address = palloc_get_page(PAL_USER | flag);

  if (frame_address == NULL) {
    PANIC("no more free pages");
    return NULL;
  }

  new_frame->frame_address = frame_address;

  lock_acquire(&frames.lock);
  hash_insert(&frames.table, &new_frame->hash_elem);
  lock_release(&frames.lock);

  return frame_address;
}

/* Destroy a specified frame in frame table. */
void destroy_frame (void *frame_address)
{
  struct frame *frame = lookup_frame (frame_address);
  lock_acquire (&frames.lock);
  hash_delete (&frames.table, &frame->hash_elem);
  lock_release (&frames.lock);
  palloc_free_page (frame_address);
  free (frame);
}


/* Helper functions for frame table. */

static struct frame *lookup_frame(void *frame_address) {
  struct frame search_frame;
  search_frame.frame_address = frame_address;

  lock_acquire(&frames.lock);
  struct hash_elem *frame_elem = hash_find(&frames.table, &search_frame.hash_elem);
  lock_release(&frames.lock);

  return hash_entry(frame_elem, struct frame, hash_elem);
}

static unsigned frame_hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame *frame = hash_entry(elem, struct frame, hash_elem);
  return hash_bytes(&frame->frame_address, sizeof(frame->frame_address));
}

static bool frame_hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  struct frame *frame_a = hash_entry(a, struct frame, hash_elem);
  struct frame *frame_b = hash_entry(b, struct frame, hash_elem);
  return frame_a->frame_address < frame_b->frame_address;
}

static void frame_destroy_func (struct hash_elem *e, void *aux UNUSED)
{
  struct frame *entry = hash_entry (e, struct frame, hash_elem);
  free (entry);
}