#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <inttypes.h>
#include <stdint.h>
#include "process.h"

static void syscall_handler (struct intr_frame *);

/* System calls. */
static void halt (void);
static void exit (int status);
static pid_t exec (const char *file);
static int wait (pid_t pid);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, const void *buffer, unsigned size);
static int write (int fd, const void *buffer, unsigned size);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t systemCallNo = *(uint32_t*) f->esp;
  //printf ("system call number: %"PRIU32"\n", systemCallNo);
  thread_exit ();
}

 // Terminates Pintos
static void halt (void) 
{

}

// Terminates the current user program
static void exit (int status) 
{
  struct thread *current = thread_current();
  current->process->exit_status = status;
  printf ("%s: exit(%d)\n", current->name, status);
  thread_exit();
}

// Run executable
static pid_t exec (const char *file) 
{
  return -1;
}

// Wait for child process
static int wait (pid_t pid) 
{
  return process_wait(pid);
}

// Creates new file with size initial_size
static bool create (const char *file, unsigned initial_size) 
{
  return false;
}

// Removes file
static bool remove (const char *file)
{
  return false;
}

// Opens file, returns -1 if file could not be opened, otherwise returns fd
static int open (const char *file)
{
  return 0;
}

// Returns filesize of file
static int filesize (int fd)
{
  return 0;
}

// Reads size bytes from file fd  into buffer
static int read (int fd, const void *buffer, unsigned size)
{
  return 0;
}

// Writes size bytes from buffer into file fd
static int write (int fd, const void *buffer, unsigned size)
{
  int fileSize = filesize(fd);
  unsigned sizeToWrite = (size > fileSize) ? fileSize : size;

  if (fd == STDOUT_FILENO) {
    putbuf(buffer, sizeToWrite);
    return sizeToWrite;
  }

  return 0;
}

// Changes next byte to be read or written in fd to position
static void seek (int fd, unsigned position)
{}

// Returns position of next byte to be written or read in fd
static unsigned tell (int fd)
{
  return 0;
}

// Closes fd
static void close (int fd)
{}

static bool is_valid_address (const void *addr)
{
 return is_user_vaddr (addr) && (pagedir_get_page (thread_current()->pagedir, addr) != NULL);
}