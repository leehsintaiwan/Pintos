#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

// #include "lib/user/syscall.h"
#include "lib/kernel/list.h"

/* Process identifier. */
typedef int pid_t;
#define PID_ERROR ((pid_t) -1)

void syscall_init (void);

/* Lock to be used when accessing the file system. */
struct lock filesys_lock;

/* Struct for file descriptors. */
struct fd
{
    int id;
    pid_t process;
    struct file *file;
    struct list_elem elem;
};

#endif /* userprog/syscall.h */
