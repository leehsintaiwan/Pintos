#include "devices/input.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/stdint.h"
#include "lib/stdio.h"
#include "process.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include <inttypes.h>
#include <stdio.h>
#include <syscall-nr.h>
#include "lib/kernel/stdio.h"

#define NUM_OF_SYSCALLS 13

static void syscall_handler (struct intr_frame *);

/* System calls. */
static void halt (struct intr_frame *f);
static void exit (struct intr_frame *f);
static void exec (struct intr_frame *f);
static void wait (struct intr_frame *f);
static void create (struct intr_frame *f);
static void remove (struct intr_frame *f);
static void open (struct intr_frame *f);
static void filesize (struct intr_frame *f);
static void read (struct intr_frame *f);
static void write (struct intr_frame *f);
static void seek (struct intr_frame *f);
static void tell (struct intr_frame *f);
static void close (struct intr_frame *f);
static void mmap (struct intr_frame *f);
static void munmap (struct intr_frame *f);

/* Helper functions. */
static struct fd *find_fd (struct thread *t, int fd_id);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static bool is_string_valid (char *str);
static bool is_buffer_valid (void *addr, int size);
static char *get_address (void *addr);
static uint32_t get_num (void *addr);


static void (*syscall_function[NUM_OF_SYSCALLS])(struct intr_frame *);


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
  syscall_function[SYS_MMAP] = &mmap;
  syscall_function[SYS_MUNMAP] = &munmap;
  lock_init (&filesys_lock);
}

static void
syscall_handler (struct intr_frame *f) 
{
  uint32_t syscall_no = *(uint32_t *) f->esp;

  syscall_function[syscall_no](f);
}

// Stores the return value in EAX on frame
static void return_frame(struct intr_frame *f, int value)
{
  f->eax = value;
}

 // Terminates Pintos
static void halt (struct intr_frame *f UNUSED) 
{
  shutdown_power_off();
}

// Terminates the current user program
static void exit (struct intr_frame *f) 
{
  int status = get_num (f->esp + 4);
  close_all();
  struct thread *current = thread_current();
  current->process->exit_status = status;
  printf ("%s: exit(%d)\n", current->name, status);
  thread_exit();
}

// Terminates the current user program due to exception
void exit_exception (void)
{
  close_all();
  struct thread *current = thread_current();
  current->process->exit_status = -1;
  printf ("%s: exit(%d)\n", current->name, -1);
  thread_exit();
}

// Run executable
static void exec (struct intr_frame *f) 
{
  char *file = get_address (f->esp + 4);
  tid_t thread_id = process_execute(file);
  return_frame(f, thread_id);
}

// Wait for child process
static void wait (struct intr_frame *f) 
{
  pid_t pid = get_num (f->esp + 4);
  return_frame(f, process_wait(pid));
}

/* Creates a new file called file initially initial size bytes in size. 
   Returns true if successful, false otherwise. */
static void create (struct intr_frame *f) 
{
  const char *file = get_address (f->esp + 4);
  unsigned initial_size = get_num (f->esp + 8);
  if (!is_string_valid((char *) file))
  {
    return_frame(f, false);
    return;
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_create(file, initial_size);
  lock_release (&filesys_lock);
  return_frame(f, success);
}

/* Deletes the file called file. 
   Returns true if successful, false otherwise. */
static void remove (struct intr_frame *f)
{
  const char *file = get_address (f->esp + 4);
  if (!is_string_valid((char *) file))
  {
    return_frame(f, false);
    return;
  }

  bool success;
  lock_acquire (&filesys_lock);
  success = filesys_remove(file);
  lock_release (&filesys_lock);
  return_frame(f, success);
}

/* Opens the file called file. 
   Returns a nonnegative integer handle called a "file descriptor" (fd),
   or -1 if the file could not be opened. */
static void open (struct intr_frame *f)
{
  const char *file = get_address (f->esp + 4);
  if (!is_string_valid((char *) file))
  {
    return_frame(f, -1);
    return;
  }
  
  lock_acquire (&filesys_lock);
  
  /* Attempt to open file. */
  struct file *open_file = filesys_open (file);
  if (!open_file)
  {
    lock_release (&filesys_lock);
    return_frame(f, -1);
    return;
  }

  /* Allocate memory. */
  struct fd *file_desc = malloc(sizeof(struct fd));
  if (!file_desc)
  {
    lock_release (&filesys_lock);
    return_frame(f, -1);
    return;
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
static void filesize (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
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
static void read (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
  const void *buffer = get_address (f->esp + 8);
  unsigned size = get_num (f->esp + 12);
  if (!is_buffer_valid((void *) buffer, size)) 
  {
    exit_exception ();
    return;
  }

  int num_bytes;

  if (fd == STDOUT_FILENO)
  {
    exit_exception();
    return;
  }
    

  if (fd == STDIN_FILENO) 
  {
    /* Must fill buffer from STDIN. */
    for (unsigned i = 0; i < size; i++) 
    {
      if (!put_user((void *) (buffer + i), input_getc())) 
      {
        return_frame(f, -1);
        return;
      }
    }
    num_bytes = size;
  } 
  else
  {
    lock_acquire (&filesys_lock);
    num_bytes = -1;
    struct fd *file_desc = find_fd (thread_current(), fd);
    if (!file_desc) 
    {
      lock_release (&filesys_lock);
      exit_exception ();
      return;
    }
    
    num_bytes = file_read (file_desc->file, (void *) buffer, size);

    lock_release (&filesys_lock);
  }

  return_frame(f, num_bytes);
}

/* Writes size bytes from buffer to the open file fd. 
   Returns the number of bytes actually written, which may 
   be less than size if some bytes could not be written. */
static void write (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
  const void *buffer = get_address (f->esp + 8);
  unsigned size = get_num (f->esp + 12);
  if (!is_buffer_valid((void *) buffer, size)) 
  {
    exit_exception ();
    return;
  }

  if (fd == STDIN_FILENO)
  {
    exit_exception();
    return;
  }

  /* Write to console. */
  if (fd == STDOUT_FILENO) 
  {
    putbuf(buffer, size);
    return_frame(f, size);
    return;
  }

  /* Write to file. */
  lock_acquire(&filesys_lock);
  struct fd *file_desc = find_fd(thread_current(), fd);

  if (!file_desc) 
  {
    lock_release(&filesys_lock);
    exit_exception ();
    return;
  }
  int bytes_written = file_write(file_desc->file, buffer, size);
  lock_release(&filesys_lock);
  return_frame(f, bytes_written);
}

/* Changes the next byte to be read or written in open file fd 
   to position, expressed in bytes from the beginning of the file. */
static void seek (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
  unsigned position = get_num (f->esp + 8);
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  if (file_desc)
  {
    file_seek (file_desc->file, position);
  }

  lock_release (&filesys_lock);
}

/* Returns the position of the next byte to be read or written 
   in open file fd, expressed in bytes from the beginning of the file. */
static void tell (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);
  unsigned position = -1;

  if (file_desc)
  {
    position = file_tell (file_desc->file);
  }

  lock_release (&filesys_lock);
  return_frame(f, position);
}

/* Closes file descriptor fd. 
   Exiting or terminating a process implicitly closes all its open file
   descriptors, as if by calling this function for each one. */
static void close (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);
  lock_acquire (&filesys_lock);

  struct fd *file_desc = find_fd (thread_current(), fd);

  if (file_desc && thread_current()->tid == (tid_t) file_desc->process)
  {
    list_remove (&file_desc->elem);
    file_close (file_desc->file);
    free (file_desc);
  }

  lock_release (&filesys_lock);
}

/* System Calls for memory mapping*/

/* Maps file open as fd into process' virtual address space*/
static void mmap (struct intr_frame *f)
{
  int fd = get_num (f->esp + 4);

  /* Call to mmap fails if fd is 0 or 1 */
  if (fd <= 1)
  {
    return_frame (f, -1);
    return;
  }

  void *addr = get_address (f->esp + 8);

  /* Call to mmap fails if addr is 0 */
  if (addr == 0)
  {
    return_frame (f, -1);
    return;
  }

  lock_acquire (&filesys_lock);
  struct fd *file_desc = find_fd (thread_current(), fd);

  if (!file_desc)
  {
    lock_release (&filesys_lock);
    return_frame (f, -1);
    return;
  }

  /* Call to mmap may fail if file has length of zero bytes */
  int size = file_length (file_desc->file);
  if (size == 0)
  {
    lock_release (&filesys_lock);
    return_frame (f, -1);
    return;
  }

  /* Check if the range of pages overlaps any existing set of mapped pages */
  int32_t offset;
  for (offset = 0; offset < size; offset += PGSIZE)
  {
    void *map_addr = addr + offset;

    if (find_page (thread_current()->supp_page_table, map_addr))
    {
      lock_release (&filesys_lock);
      return_frame (f, -1);
      return;
    }

  }

  for (offset = 0; offset < size; offset += PGSIZE)
  {
    void *map_addr = addr + offset;

    uint32_t read_bytes = offset + PGSIZE < size ? PGSIZE : size - offset;
    uint32_t zero_bytes = PGSIZE - read_bytes;

    add_file_supp_pt (thread_current()->supp_page_table, map_addr, f, offset, read_bytes, zero_bytes, true);
  }

  /* ASSIGN MAPPING ID HERE */
  mapid_t mapping_id;
  if (!list_empty(thread_current()->mmap_list))
  {
    mapping_id = list_entry(list_back(thread_current()->mmap_list), struct md, elem)->id + 1;
  }
  else
  {
    mapping_id = 1;
  }

  struct md *mmap_desc = (struct md*) malloc(sizeof(struct md));
  mmap_desc->id = mapping_id;
  mmap_desc->file = f;
  mmap_desc->addr = addr;
  mmap_desc->size = size;
  list_push_back(thread_current()->mmap_list, &mmap_desc->elem);

  lock_release (&filesys_lock);
  return_frame (f, mapping_id);
}

static void munmap (struct intr_frame *f)
{
  mapid_t mapping_id = get_num (f->esp + 4);

}

/* Helper Functions */

/* Function to close all files open by current thread. */
void close_all(void)
{
	if (!lock_held_by_current_thread (&filesys_lock))
  {
		lock_acquire (&filesys_lock);
  }
  struct list_elem *e;
  struct thread *t = thread_current();
  while (!list_empty (&t->open_fd))
	{
    e = list_begin(&t->open_fd);
		int fd = list_entry (e, struct fd, elem)->id;
		struct fd *file_desc = find_fd (t, fd);
		if (file_desc)
    {
      list_remove (&file_desc->elem);
      file_close (file_desc->file);
      free (file_desc);
    }
	}
	lock_release (&filesys_lock);
}

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

/* Find the map descriptor in thread t using the 
   given mapping id. */
static struct md *find_md (struct thread *t, mapid_t mapping_id)
{
  ASSERT (t);

	for (struct list_elem *e = list_begin (&t->mmap_list); 
       e != list_end (&t->mmap_list);
	     e = list_next (e))
	{
		struct md *mmap_desc = list_entry (e, struct md, elem);
		if (mmap_desc->id == mapping_id)
			return mmap_desc;
	}

	return NULL;
}

/* Verify user pointer, then dereference it. */
static void verify_pointer (void *addr)
{
  if (get_user ((uint8_t *) addr) == -1) 
  {
    exit_exception ();
  }
}

static char *get_address (void *addr)
{
  verify_pointer (addr);
  return *((char **) addr);
}

static uint32_t get_num (void *addr)
{
  verify_pointer (addr);
  return *((uint32_t *) addr);
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
  while (character != '\0')
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
    if (get_user ((uint8_t *) (buffer + i)) == -1)
    {
      return false;
    }
  }

  return true;
}

