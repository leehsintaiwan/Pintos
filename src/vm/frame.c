#include "frame.h"

static struct frame_table frames;

void init_frames()
{
  hash_init(&frames->table, hash_func, hash_less, NULL);
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