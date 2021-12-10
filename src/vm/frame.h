#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <hash.h>
#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "vm/page.h"

// Frame table stored as a hash table
struct hash frame_table;

// Struct for a frame, containing all necessary information about that frame
struct frame
{
  void *frame_address;              /* Frame address allocated using palloc */
  void *page_address;               /* User page address that's using this frame */
  struct hash_elem hash_elem;       /* Hash elem used in hash table */
  struct list_elem list_elem;       /* List elem used in frame_list */
  struct thread *thread;            /* Stores the thread that owns this frame */
  struct file_struct *file_info;    /* File that if read-only can be used for sharing */
  uint32_t num_shared_pages;        /* Number of pages shared between other processes */
  bool used;                        /* Indicates that a frame is being used, 
                                       to prevent it from being evicted */
};

void init_frames(void);
void *get_new_frame(enum palloc_flags flag, struct file_struct *file, void *page_address);
void destroy_frame (void *frame_address, bool palloc_free);
void set_used (void *frame_address, bool new_used);

#endif /* vm/frame.h */