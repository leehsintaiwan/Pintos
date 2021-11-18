#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

/* Max size for rest of memory struct 
   that the pintos utility can pass to the kernel. */
#define MAX_REMAINING_SIZE 148

/* Max number of arguments passed. */
#define MAX_ARGS_AMOUNT 32

typedef uint32_t pid_t;

tid_t process_execute (const char *cmd_line);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

struct process {
  pid_t pid;
  int exit_status;
  bool exited;
  struct list child_process_list;
  struct list_elem child_process_elem;
  struct semaphore *wait_child;
  bool parent_died;
};

#endif /* userprog/process.h */
