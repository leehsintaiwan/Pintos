#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>


struct frame_table
{
  struct hash table;
};

struct frame
{
  void *frame_address;
  void *page_address;
  struct hash_elem hash_elem;
};

void init_frames();
void destroy_frame (void *frame_address);
void *get_new_frame(void *page_address);

#endif /* vm/frame.h */