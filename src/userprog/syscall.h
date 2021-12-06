#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "lib/kernel/list.h"
#include "threads/thread.h"

/* Process identifier. */
typedef uint32_t pid_t;
#define PID_ERROR ((pid_t) -1)

void syscall_init (void);
void exit_exception (void);
void close_all (void);
bool munmap (mapid_t mapping_id);


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
