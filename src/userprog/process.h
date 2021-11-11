#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Max size for command-line arguments 
   that the pintos utility can pass to the kernel. */
#define MAX_ARGS_SIZE 128

/* Max number of arguments passed. */
#define MAX_ARGS_AMOUNT 15

typedef int pid_t;

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process {
  pid_t pid;
  int exit_status;
  struct list child_process_list;
  struct list_elem child_process_elem;
  struct semaphore wait_child;
  bool parent_died;
}

#endif /* userprog/process.h */
