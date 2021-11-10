#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* Max size for command-line arguments 
   that the pintos utility can pass to the kernel. */
#define MAX_ARGS_SIZE 128

/* Max number of arguments passed. */
#define MAX_ARGS_AMOUNT 15


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
