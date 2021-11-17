#include "userprog/syscall.h"
#include <syscall-nr.h>
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include <inttypes.h>
#include "lib/stdint.h"
#include "process.h"
#include "devices/shutdown.h"
#include "lib/stdio.h"
#include "threads/palloc.h"
#include "devices/input.h"
#include "threads/vaddr.h"

#define NUM_OF_SYSCALLS 13

static void syscall_handler (struct intr_frame *);

/* System calls. */
static void halt (int32_t *args, struct intr_frame *f);
static void exit (int32_t *args, struct intr_frame *f);
static void exec (int32_t *args, struct intr_frame *f);
static void wait (int32_t *args, struct intr_frame *f);
static void create (int32_t *args, struct intr_frame *f);
static void remove (int32_t *args, struct intr_frame *f);
static void open (int32_t *args, struct intr_frame *f);
static void filesize (int32_t *args, struct intr_frame *f);
static void read (int32_t *args, struct intr_frame *f);
static void write (int32_t *args, struct intr_frame *f);
static void seek (int32_t *args, struct intr_frame *f);
static void tell (int32_t *args, struct intr_frame *f);
static void close (int32_t *args, struct intr_frame *f);

/* Helper functions. */
static struct fd *find_fd (struct thread *t, int fd_id);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool is_string_valid (char *str);
static bool is_buffer_valid (void *addr, int size);
static int copy_bytes (void *source, void *dest, size_t size);

static void (*syscall_function[NUM_OF_SYSCALLS])(int32_t *, struct intr_frame *);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
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
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t syscall_no = *(uint32_t *) f->esp;

  // Add function to get up to argument
  // Store args in arg 1, arg 2 and arg 3
  int32_t *args = get_arguments(&f->esp);
  syscall_function[syscall_no](args, f);

  // Ensure to add check if return is -1
  // Check if the current thread is holding the filesys_lock
  // and if it is then release it
  thread_exit();
}

// Stores the return value in EAX on frame
static void return_frame(struct intr_frame *f, int value)
{
  f->eax = value;
}

 // Terminates Pintos
static void halt (int32_t *args UNUSED, struct intr_frame *f UNUSED) 
{
  shutdown_power_off();
}

// Terminates the current user program
static void exit (int32_t *args, struct intr_frame *f UNUSED) 
{
  int status = args[0];
  struct thread *current = thread_current();
  current->process->exit_status = status;
  printf ("%s: exit(%d)\n", current->name, status);
  thread_exit();
}

// Run executable
static void exec (int32_t *args, struct intr_frame *f) 
{
  const char *file = (char *) args[0];
  tid_t thread_id = process_execute(file);
  return_frame(f, thread_id);
}

// Wait for child process
static void wait (int32_t *args, struct intr_frame *f) 
{
  pid_t pid = args[0];
  return_frame(f, process_wait(pid));
}

/* Creates a new file called file initially initial size bytes in size. 
   Returns true if successful, false otherwise. */
static void create (int32_t *args, struct intr_frame *f) 
{
  const char *file = (char *) args[0];
  unsigned initial_size = args[1];
  if (!is_string_valid((char *) file))
  {
    return_frame(f, false);
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_create(file, initial_size);
  lock_release (&filesys_lock);
  return_frame(f, success);
}

/* Deletes the file called file. 
   Returns true if successful, false otherwise. */
static void remove (int32_t *args, struct intr_frame *f)
{
  const char *file = (char *) args[0];
  if (!is_string_valid((char *) file))
  {
    return_frame(f, false);
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_remove(file);
  lock_release (&filesys_lock);
  return_frame(f, success);
}

/* Opens the file called file. 
   Returns a nonnegative integer handle called a “file descriptor” (fd),
   or -1 if the file could not be opened. */
static void open (int32_t *args, struct intr_frame *f)
{
  const char *file = (char *) args[0];
  if (!is_string_valid((char *) file))
  {
    return_frame(f, -1);
  }
  
  lock_acquire (&filesys_lock);
  
  /* Attempt to open file. */
  struct file *open_file = filesys_open (file);
  if (!open_file)
  {
    lock_release (&filesys_lock);
    return_frame(f, -1);
  }

  /* Allocate memory. */
  struct fd *file_desc = palloc_get_page (0);
  if (!file_desc)
  {
    lock_release (&filesys_lock);
    return_frame(f, -1);
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
  return_frame(f, file_desc->id);
}

/* Returns the size, in bytes, of the file open as fd. */
static void filesize (int32_t *args, struct intr_frame *f)
{
  int fd = args[0];
  int size = -1;
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  if (file_desc)
  {
    size = file_length (file_desc->file);
  }
  lock_release (&filesys_lock);
  return_frame(f, size);
}

/* Reads size bytes from the file open as fd into buffer. 
   Returns the number of bytes actually read (0 at end of file), 
   or -1 if the file could not be read 
   (due to a condition other than end of file). */
static void read (int32_t *args, struct intr_frame *f)
{
  int fd = args[0];
  const void *buffer = (void *) args[1];
  unsigned size = args[2];
  if (!is_buffer_valid((void *) buffer, size)) 
  {
    return_frame(f, -1);
  }

  int num_bytes;

  if (fd == STDIN_FILENO) 
  {
    /* Must fill buffer from STDIN. */
    for (unsigned i = 0; i < size; i++) 
    {
      if (!put_user((void *) (buffer + i), input_getc())) 
      {
        lock_release (&filesys_lock);
        return_frame(f, -1);
      }
    }
    num_bytes = size;
  } 
  else
  {
    lock_acquire (&filesys_lock);
    num_bytes = -1;
    struct fd *file_desc = find_fd (thread_current(), fd);
    if (file_desc) 
    {
      num_bytes = file_read (file_desc->file, (void *) buffer, size);
    }

    lock_release (&filesys_lock);
  }

  return_frame(f, num_bytes);
}

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually written, which may 
   be less than size if some bytes could not be written. */
static void write (int32_t *args, struct intr_frame *f)
{
  int fd = args[0];
  const void *buffer = (void *) args[1];
  unsigned size = args[2];
  if (!is_buffer_valid((void *) buffer, size)) 
  {
    return_frame(f, -1);
  }

  /* Write to console. */
  int fileSize = filesize(fd);
  unsigned sizeToWrite = (size > fileSize) ? fileSize : size;

  if (fd == STDOUT_FILENO) 
  {
    putbuf(buffer, sizeToWrite);
    return_frame(f, sizeToWrite);
  }

  /* Write to file. */
  lock_acquire(&filesys_lock);
  struct fd *file_desc = find_fd(thread_current(), fd);

  if (!file_desc) 
  {
    lock_release(&filesys_lock);
    return_frame(f, -1);
  }

  int bytes_written = file_write(file_desc->file, buffer, size);
  lock_release(&filesys_lock);
  return_frame(f, bytes_written);
}

/* Changes the next byte to be read or written in open file fd 
   to position, expressed in bytes from the beginning of the file. */
static void seek (int32_t *args, struct intr_frame *f UNUSED)
{
  int fd = args[0];
  unsigned position = args[1];
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
static void tell (int32_t *args, struct intr_frame *f)
{
  int fd = args[0];
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  unsigned position = -1;

  if (file_desc && file_desc->file)
  {
    position = file_tell (file_desc->file);
  }

  lock_release (&filesys_lock);
  return_frame(f, position);
}

/* Closes file descriptor fd. 
   Exiting or terminating a process implicitly closes all its open file
   descriptors, as if by calling this function for each one. */
static void close (int32_t *args, struct intr_frame *f UNUSED)
{
  int fd = args[0];
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


/* Get the arguments from the function pointer. */
static int32_t *get_arguments(void *esp)
{
  int32_t args[3];
  if (copy_bytes (esp + 4, &args[0], 4) == -1 ||
      copy_bytes (esp + 8, &args[1], 4) == -1 ||
      copy_bytes (esp + 12, &args[2], 4) == -1)
  {
    // TODO: HANDLE ERROR
  }
  return args;
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
   Returns the number of byte copied or -1 if failed. */
static int copy_bytes (void *source, void *dest, size_t size)
{
  for (unsigned i = 0; i < size; i++) 
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

