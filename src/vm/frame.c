#include "frame.h"


static hash_hash_func frame_hash_func;
static hash_less_func frame_hash_less;
static hash_action_func frame_destroy_func;

static struct frame_table frames;

void init_frames()
{
  hash_init(&frames.table, frame_hash_func, frame_hash_less, NULL);
}

/* Destroy frame table. */
void destroy_frame_table (struct frames *supp_page_table)
{
  // hash_destroy (&frames.table, frame_destroy_func);
  free (frames);
}


struct frame *get_new_frame(void *page_address)
{
  struct frame *new_frame = malloc(sizeof(struct frame));
  new_frame->page_address = page_address;

  void *frame_address = palloc_get_page(PAL_USER);

  if (frame_address == NULL) {
    PANIC("no more free pages");
  }

  new_frame->frame_address = frame_address;

  hash_insert(&frames.table, &new_frame->hash_elem);

  return &new_frame;
}

struct frame *lookup_frame(void *frame_address) {
  struct frame search_frame;
  search_frame.frame_address = frame_address;

  struct hash_elem *frame_elem = hash_find(&frames.table, &search_frame.hash_elem);
  
  return hash_entry(frame_elem, struct frame, hash_elem);
}


/* Helper functions for frame table. */

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