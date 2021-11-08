#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process {
  tid_t tid;
  int exit_status;
  struct list child_process_list;
  struct list_elem child_process_elem;
  struct semaphore wait_child;
  bool parent_died;
}

#endif /* userprog/process.h */
