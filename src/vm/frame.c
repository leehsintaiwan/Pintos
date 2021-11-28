#include "frame.h"

static struct frame_table frames;
unsigned counter;

void init_frames()
{
  hash_init(&frames->table, hash_func, hash_less, NULL);
  counter = 0;
}

unsigned hash_func(const struct hash_elem *elem, void *aux UNUSED)
{
  struct frame *frame = hash_entry(elem, struct frame, hash_elem);
  return frame->index;
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux)
{
  struct frame *frame_a = hash_entry(a, struct frame, hash_elem);
  struct frame *frame_b = hash_entry(b, struct frame, hash_elem);
  return frame_a->index < frame_b->index;
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

  new_frame->index = counter;
  counter++;

  hash_insert(&frames->table, &new_frame->hash_elem);

  return &new_frame;
}
