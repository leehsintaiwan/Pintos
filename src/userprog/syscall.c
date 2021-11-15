#include "userprog/syscall.h"
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <inttypes.h>
#include "stdint.h"
#include "process.h"
#include "devices/shutdown.h"
#include "stdio.h"

#define NUM_OF_SYSCALLS 13

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
static bool is_buffer_valid (void *addr, int size);
static int copy_bytes (void *source, void *dest, size_t size);
static bool is_valid_address (const void *addr);

static void (*syscall_function[NUM_OF_SYSCALLS])(int32_t *, struct intr_frame *);
syscall_function[SYS_HALT] = &halt;
syscall_function[SYS_EXIT] = &exit;
syscall_function[SYS_EXEC] = &exec;
syscall_function[SYS_WAIT] = &wait;
syscall_function[SYS_CREATE] = &create;
syscall_function[SYS_REMOVE] = &remove;
syscall_function[SYS_OPEN] = &open;
syscall_function[SYS_FILESIZE] = &filesize;
syscall_function[SYS_READ] = &read;
syscall_function[SYS_WRITE] = &write;
syscall_function[SYS_SEEK] = &seek;
syscall_function[SYS_TELL] = &tell;
syscall_function[SYS_CLOSE] = &close;

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t syscall_no = *(uint32_t *) f->esp;

  // Add function to get up to argument
  // Store args in arg 1, arg 2 and arg 3
  int32_t *args = get_arguments(f->esp);
  syscall_function[syscall_no](f, args);

  // Ensure to add check if return is -1
  // Check if the current thread is holding the filesys_lock
  // and if it is then release it
  thread_exit();
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
  return process_execute(file);
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
  if (list_empty(fd_list)) 
  {
    file_desc->id = 2;
  }
  else 
  {
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

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file), 
   or -1 if the file could not be read 
   (due to a condition other than end of file). */
static int read (int fd, const void *buffer, unsigned size)
{
  if (!is_buffer_valid(buffer, size)) 
  {
    return -1;
  }

  int num_bytes;

  if (fd == STDIN_FILENO) 
  {
    /* Must fill buffer from STDIN. */
    for (unsigned i = 0; i < size; i++) 
    {
      if (!put_user(buffer + i, input_getc())) 
      {
        lock_release (&filesys_lock);
        return -1;
      }
    }
    num_bytes = size;
  } 
  else
  {
    lock_acquire (&filesys_lock);
    num_bytes = -1;
    struct fd *file_desc = find_file (fd);
    if (file_desc) 
    {
      num_bytes = file_read (file_desc->file, buffer, size);
    }

    lock_release (&filesys_lock);
  }

  return num_bytes;
}

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually written, which may 
   be less than size if some bytes could not be written. */
static int write (int fd, const void *buffer, unsigned size)
{
  if (!is_buffer_valid(buffer, size)) 
  {
    return -1;
  }

  /* Write to console. */
  int fileSize = filesize(fd);
  unsigned sizeToWrite = (size > fileSize) ? fileSize : size;

  if (fd == STDOUT_FILENO) 
  {
    putbuf(buffer, sizeToWrite);
    return sizeToWrite;
  }

  /* Write to file. */
  lock_acquire(&filesys_lock);
  struct fd *file_desc = find_fd(thread_current(), fd);

  if (!file_desc) 
  {
    lock_release(&filesys_lock);
    return -1;
  }

  int bytes_written = file_write(file_desc->file, buffer, size);
  lock_release(&filesys_lock);
  return bytes_written;
}

/* Changes the next byte to be read or written in open file fd 
   to position, expressed in bytes from the beginning of the file. */
static void seek (int fd, unsigned position)
{
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);

  if (file_desc && position > 0 && file_desc->file)
  {
    file_seek (file_desc->file, position);
  }

  lock_release (&filesys_lock);
}

/* Returns the position of the next byte to be read or written 
   in open file fd, expressed in bytes from the beginning of the file. */
static unsigned tell (int fd)
{
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  unsigned position = -1;

  if (file_desc && file_desc->file)
  {
    position = file_tell (file_desc->file);
  }

  lock_release (&filesys_lock);
  return position;
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

  if (fd_id < 2) 
  {
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
static int32_t
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
    return false;
  }
  int i = 1;
  while (character != "\0")
  {
    character = get_user ((uint8_t *) (str + i));
    if (character == -1)
    {
      return false;
    }
    i++;
  }

  return true;
}

/* Check that a buffer of given size is valid. */
static bool is_buffer_valid (void *addr, int size)
{
  char *buffer = (char *) addr;
  for (int i = 0; i < size; i++)
  {
    if (get_user ((uint8_t *) (buffer + i) == -1))
    {
      return false;
    }
  }

  return true;
}


/* Copy bytes from the source into the destination address.
   Returns the number of byte copied. */
static int copy_bytes (void *source, void *dest, size_t size)
{
  for (unsigned i = 0; i < size; i) 
  {
    int32_t val = get_user (source + i);
    if (val == -1)
    {
      return -1;
    }
    // Copy the first byte only
    if (!put_user(dest + i, val & 0xFF)) 
    {
      return -1;
    }
  }
  
  return size;
}


/* Simple address validity check. 
   Use other helpers for more efficient checking. */
static bool is_valid_address (const void *addr)
{
  return is_user_vaddr (addr) && (pagedir_get_page (thread_current()->pagedir, addr) != NULL);
}
