#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

/* Lock to be used when accessing the file system. */
struct lock filesys_lock;

#endif /* userprog/syscall.h */
