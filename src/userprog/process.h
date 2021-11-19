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
  pid_t pid;                            /* PID number */
  int exit_status;                      /* Exit status, -1 if killed by kernel */
  bool exited;                          /* Indicates if process has exited */
  struct list child_process_list;       /* Contains the list of child process */
  struct list_elem child_process_elem;  /* list elem for assigning to parent process */
  struct semaphore *wait_child;         /* Semaphore for waiting for this process to exit */
  bool parent_died;                     /* Indicates if parent had exited */
  bool is_root;
};

#endif /* userprog/process.h */
