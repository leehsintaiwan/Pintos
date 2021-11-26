#ifndef FRAME_H
#define FRAME_H

struct frame_table
{
  struct hash table;
};

struct frame
{
  void *page;
  struct hash_elem hash_elem;
};

#endif /* vm/frame.h */