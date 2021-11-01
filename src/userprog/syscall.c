#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t systemCallNo = *(uint32_t*) intr_frame->esp;
  printf ("system call number: %"PRIU32"\n", systemCallNo);
  thread_exit ();
}
 // Terminates Pintos
void halt (void) {

}

// Terminates the current user program
void exit (int status) {
  printf ("%s: exit(%d)\n", status);
}

// Run executable
pid_t exec (const char *file) {
  return -1;
}

// Wait for child process
int wait (pid_t pid) {
  return -1;
}

// Creates new file with size initial_size
bool create (const char *file, unsigned initial_size) 
{
  return false;
}

// Removes file
bool remove (const char *file)
{
  return false;
}

// Opens file, returns -1 if file could not be opened, otherwise returns fd
int open (const char *file)
{
  return 0;
}

// Returns filsize of file
int filesize (int fd)
{
  return 0;
}

// Reads size bytes from file fd  into buffer
int read (int fd, const void *buffer, unsigned size)
{
  return 0;
}

// Writes size bytes from buffer into file fd
int write (int fd, const void *buffer, unsigned size)
{
  return 0;
}

// Changes next byte to be read or written in fd to position
void seek (int fd, unsigned position)
{}

// Returns position of next byte to be written or read in fd
unsigned tell (int fd)
{
  return 0;
}

// Closes fd
void close (int fd)
{}