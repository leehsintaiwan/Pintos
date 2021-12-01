#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "threads/palloc.h"


struct frame_table
{
  struct lock lock;
  struct hash table;
};

struct frame
{
  void *frame_address;
  void *page_address;
  struct hash_elem hash_elem;
};

void init_frames();
void *get_new_frame(enum palloc_flags flag, void *page_address);
void destroy_frame (void *frame_address);

#endif /* vm/frame.h */