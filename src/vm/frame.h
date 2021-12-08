#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"

struct frame
{
  void *frame_address;         /* Frame address allocated using palloc */
  void *page_address;          /* User page address that's using this frame */
  struct hash_elem hash_elem;  /* Hash elem used in hash table */
  struct list_elem list_elem;  /* List elem used in frame_list */
  struct thread *thread;       /* Stores the thread that owns this frame */
};

void init_frames(void);
void *get_new_frame(enum palloc_flags flag, void *page_address);
void destroy_frame (void *frame_address);

#endif /* vm/frame.h */