#ifndef FRAME_H
#define FRAME_H

struct frame_table
{
  struct hash table;
};

struct frame
{
  unsigned index;
  void *page_address;
  void *frame_address;
  struct hash_elem hash_elem;
};

#endif /* vm/frame.h */