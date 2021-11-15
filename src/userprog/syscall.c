#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
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

/* Helper functions. */
static struct fd *find_fd (struct thread *t, int fd_id);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool is_string_valid (char *str);
static bool is_valid_address (const void *addr);


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
  shutdown_power_off();
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

/* Creates a new file called file initially initial size bytes in size. 
   Returns true if successful, false otherwise. */
static bool create (const char *file, unsigned initial_size) 
{
  if (!is_string_valid(file))
  {
    return false;
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_create(file, initial_size);
  lock_release (&filesys_lock);
  return success;
}

/* Deletes the file called file. 
   Returns true if successful, false otherwise. */
static bool remove (const char *file)
{
  if (!is_string_valid(file))
  {
    return false;
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_remove(file);
  lock_release (&filesys_lock);
  return success;
}

/* Opens the file called file. 
   Returns a nonnegative integer handle called a “file descriptor” (fd),
   or -1 if the file could not be opened. */
static int open (const char *file)
{
  if (!is_string_valid(file))
  {
    return -1;
  }
  
  lock_acquire (&filesys_lock);
  
  /* Attempt to open file. */
  struct file *open_file = filesys_open (file);
  if (!open_file)
  {
    lock_release (&filesys_lock);
    return -1;
  }

  /* Allocate memory. */
  struct fd *file_desc = palloc_get_page (0);
  if (!file_desc)
  {
    lock_release (&filesys_lock);
    return -1;
  }

  /* Assign file info to file descriptor. */
  file_desc->file = open_file;
  file_desc->process = thread_current()->tid;

  struct list* fd_list = &thread_current()->open_fd;
  if (list_empty(fd_list)) {
    file_desc->id = 2;
  }
  else {
    file_desc->id = (list_entry(list_back(fd_list), struct fd, elem)->id) + 1;
  }
  list_push_back(fd_list, &(file_desc->elem));

  lock_release (&filesys_lock);
  return file_desc->id;

}

/* Returns the size, in bytes, of the file open as fd. */
static int filesize (int fd)
{
  int size = -1;
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  if (file_desc)
  {
    size = file_length (file_desc->file);
  }
  lock_release (&filesys_lock);
  return size;
}

// Reads size bytes from file fd  into buffer
static int read (int fd, const void *buffer, unsigned size)
{

  if (!is_valid_address(buffer)) {
    return -1;
  }

  if (fd == STDIN_FILENO) {
    return input_getc();
  }

  return size;
}

// Writes size bytes from buffer into file fd
static int write (int fd, const void *buffer, unsigned size)
{

  if (!is_valid_address(buffer)) {
    return -1;
  }

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

/* Closes file descriptor fd. 
   Exiting or terminating a process implicitly closes all its open file
   descriptors, as if by calling this function for each one. */
static void close (int fd)
{
  lock_acquire (&filesys_lock);

  struct fd *file_desc = find_fd (thread_current(), fd);
  if (file_desc && thread_current()->tid == file_desc->process)
  {
    list_remove (&file_desc->elem);
    file_close (file_desc->file);
    palloc_free_page (file_desc);
  }

  lock_release (&filesys_lock);
}

/* Helper Functions */

/* Find the file descriptor in thread t using the 
   given file descriptor id. */
static struct fd *find_fd (struct thread *t, int fd_id)
{
  ASSERT (t);

  if (fd_id < 2) {
    return NULL;
  }

	for (struct list_elem *e = list_begin (&t->open_fd); 
       e != list_end (&t->open_fd);
	     e = list_next (e))
	{
		struct fd *file_desc = list_entry (e, struct fd, elem);
		if (file_desc->id == fd_id)
			return file_desc;
	}
	return NULL;
}


/* Memory Access Helpers */

/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
  /* Check user address is below PHYS_BASE. */
  if (!is_user_vaddr (uaddr))
  {
    return -1;
  }

  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:" : "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  /* Check user address is below PHYS_BASE. */
  if (!is_user_vaddr (udst))
  {
    return false;
  }

  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:" : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

/* Check that a string is valid. */
static bool is_string_valid (char *str)
{
  int character = get_user ((uint8_t *) str);
  if (character == -1)
  {
    return false
  }
  int i = 1;
  while (character != "\0")
  {
    character = get_user ((uint8_t *) (str + i));
    if (character == -1)
    {
      return false
    }
    i++;
  }
  return true;
}


/* Simple address validity check. 
   Use other helpers for more efficient checking. */
static bool is_valid_address (const void *addr)
{
 return is_user_vaddr (addr) && (pagedir_get_page (thread_current()->pagedir, addr) != NULL);
}
