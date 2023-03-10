#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/string.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/tss.h"
#include "vm/frame.h"
#include "vm/page.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>

static thread_func start_process NO_RETURN;
static bool load (const char *file_name, char *args, void (**eip) (void), void **esp);
static bool push_arguments (void **esp, const char *file_name, char *args);
static void init_process(struct process *parent);
static struct process *get_child_process(struct list *child_list, pid_t child_pid);
static void notify_child_process(struct list *child_list);
static void free_process(struct process *process);

struct process_info
{
  char *process_name;
  char *process_args;
  struct process *parent;
  struct semaphore *wait_load;
  bool load_success;
};

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *cmd_line) 
{
  char *cl_copy;
  tid_t tid;
  
  /* Make a copy of CMD_LINE.
     Otherwise there's a race between the caller and load(). */
  cl_copy = palloc_get_page (0);
  if (cl_copy == NULL)
    return TID_ERROR;
  strlcpy (cl_copy, cmd_line, PGSIZE);

  if (thread_current()->process == NULL) 
    init_process(NULL);
  
  struct process_info process_info;
  // process_info.command_line = cl_copy;
  process_info.parent = thread_current()->process;
  
  char *null_pointer;
  char *prog_name = strtok_r(cl_copy, " ", &null_pointer);
  process_info.process_name = prog_name;
  process_info.process_args = null_pointer;
  process_info.wait_load = malloc(sizeof(struct semaphore));
  sema_init(process_info.wait_load, 0);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (prog_name, PRI_DEFAULT, start_process, &process_info);
  if (tid == TID_ERROR) {
    palloc_free_page (cl_copy); 
    free(process_info.wait_load);
    return tid;
  }
  
  sema_down(process_info.wait_load);
  free(process_info.wait_load);
  if (!process_info.load_success)
    return TID_ERROR;
  return tid;
}

static void init_process(struct process *parent)
{
  struct process *child = malloc(sizeof(struct process));
  child->wait_child = malloc(sizeof(struct semaphore));
  child->pid = thread_current()->tid;
  child->exit_status = TID_ERROR;
  child->exited = false;
  list_init(&child->child_process_list);
  sema_init(child->wait_child, 0);

  if (parent != NULL) 
  {
    child->parent_died = false;
    list_push_back(&parent->child_process_list, &child->child_process_elem);
  } 
  else 
  {
    child->parent_died = true;
  }

  thread_current()->process = child;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *info)
{
  struct process_info *process_info = (struct process_info *) info;
  char *command_line = process_info->process_args;

  struct intr_frame if_;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  init_process(process_info->parent);
  process_info->load_success = load (process_info->process_name, command_line, &if_.eip, &if_.esp);
  sema_up(process_info->wait_load);

  /* If load failed, quit. */
  if (!process_info->load_success) 
    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

static struct process *get_child_process(struct list *child_list, pid_t child_pid)
{
  for (struct list_elem *e = list_begin (child_list); e != list_end (child_list);
        e = list_next (e))
  {
    struct process *child = list_entry(e, struct process, child_process_elem);
    if (child->pid == child_pid) {
      return child;
    }
  }
  
  return NULL;
}

static void free_process(struct process *process)
{
  list_remove(&process->child_process_elem);
  free(process->wait_child);
  free(process);
}

/* Waits for thread TID to die and returns its exit status. 
 * If it was terminated by the kernel (i.e. killed due to an exception), 
 * returns -1.  
 * If TID is invalid or if it was not a child of the calling process, or if 
 * process_wait() has already been successfully called for the given TID, 
 * returns -1 immediately, without waiting.
 * 
 * This function will be implemented in task 2.
 * For now, it does nothing. */
int
process_wait (tid_t child_tid) 
{
  struct process *parent = thread_current()->process;
  struct process *child = get_child_process(&parent->child_process_list, child_tid);

  if (!child) 
  {
    return -1;
  }

  sema_down(child->wait_child);
  int status = child->exit_status;
  free_process(child);
  return status;
}

static void notify_child_process(struct list *child_list)
{
  for (struct list_elem *e = list_begin (child_list); e != list_end (child_list);
        e = list_next (e))
  {
    struct process *child = list_entry(e, struct process, child_process_elem);
    child->parent_died = true;
    if (child->exited) {
      e = list_prev(e);
      free_process(child);
    }
  }
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;
  
  close_all();
  
  struct process *process = thread_current()->process;
  notify_child_process(&process->child_process_list);
  process->exited = true;
  sema_up(process->wait_child);
  if (process && process->parent_died)
  {
    free_process(process);
  }


  /* Unmaps mappings when process exits */

  struct list *mlist = &thread_current()->mmap_list;
  while (!list_empty(mlist))
  {
    struct list_elem *e = list_begin (mlist);
    struct md *mmap_desc = list_entry(e, struct md, elem);
    if (munmap(mmap_desc->id) == false)
    {
      PANIC ("Unmap failed");
      return;
    }
  }

  /* Allow write to executable once no longer running. */
  if (!lock_held_by_current_thread(&filesys_lock))
  {
    lock_acquire(&filesys_lock);
  }
  if (cur->executable)
  {
    file_allow_write(cur->executable);
    file_close(cur->executable);
  }
  lock_release(&filesys_lock);

  destroy_supp_pt (thread_current()->supp_page_table);
  thread_current()->supp_page_table = NULL;


  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
  {
    /* Correct ordering here is crucial.  We must set
        cur->pagedir to NULL before switching page directories,
        so that a timer interrupt can't switch back to the
        process page directory.  We must activate the base page
        directory before destroying the process's page
        directory, or our active page directory will be one
        that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  }
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, char *args, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  t->supp_page_table = init_supp_page_table();
  // printf("supp page table created\n");
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  lock_acquire (&filesys_lock);
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Deny writes to a process's executable. */
  file_deny_write(file);
  thread_current()->executable = file;

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024
      ) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  /* Add arguments to stack. */
  if (!push_arguments (esp, file_name, args))
    goto done;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  if (lock_held_by_current_thread(&filesys_lock))
    lock_release (&filesys_lock);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);


/* Push arguments onto memory stack. */
static bool
push_arguments (void **esp, const char *file_name, char *args)
{

  char *arg_addresses[MAX_ARGS_AMOUNT];

  int curr_memory = 0;
  int argc = 0;

  /* Push file name onto stack. */

  int file_name_memory = sizeof (char) * (strlen (file_name) + 1);
  if (curr_memory + file_name_memory > PGSIZE - MAX_REMAINING_SIZE || argc + 1 > MAX_ARGS_AMOUNT)
  {
    return false;
  }
  *esp -= file_name_memory;
  arg_addresses[argc] = *esp;
  argc++;

  memcpy (*esp, file_name, file_name_memory);
  curr_memory += file_name_memory;

  /* Push arguments onto stack. */

  char *save_ptr;
	for (char *token = strtok_r (args, " ", &save_ptr); token != NULL;
      token = strtok_r (NULL, " ", &save_ptr))
	{
		int token_memory = sizeof (char) * (strlen (token) + 1);

    if (curr_memory + token_memory > PGSIZE - MAX_REMAINING_SIZE || argc + 1 > MAX_ARGS_AMOUNT)
    {
      return false;
    }
    *esp -= token_memory;
    arg_addresses[argc] = *esp;
    argc++;

    memcpy (*esp, token, token_memory);
    curr_memory += token_memory;
	}


  *esp = (*esp - (uint32_t) *esp % 4) - sizeof (char *) * (argc + 1);
  
  /* Push addresses to stack. */
  for (int i = 0; i < argc + 1; i++)
	{
		if (i == argc)
		{
			memset (*esp + i * sizeof (char *), 0, sizeof (char *));
		} else {
		  memcpy (*esp + i * sizeof (char *), &arg_addresses[i], sizeof (char *));
    }
	}

  /* Push address of argv[0]. */
  char **argv = *esp;
	*esp -= sizeof (char **);
	memcpy (*esp, &argv, sizeof (char **));

  /* Push argc onto stack. */
	*esp -= 4; // 4 to maintain word alignment
	memcpy (*esp, &argc, sizeof (int));

	/* Push return address of 0 onto stack. */
	*esp -= sizeof (void (*) (void));
	memset (*esp, 0, sizeof (void (*) (void)));

	// hex_dump (*esp, *esp, PHYS_BASE - *esp, 1);

  return true;
}

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);
  // printf("load segment\n");
  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      // printf("loop check\n");
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;
      
      /* Check if virtual page already allocated */
      struct thread *t = thread_current ();
      if(pagedir_get_page (t->pagedir, upage) != NULL)
      {
        // printf("null which is correct\n");
        PANIC ("pagedir should be null");
      }
      // printf("add segment %d\n", upage);
      if (page_zero_bytes == PGSIZE) {
        add_zero_supp_pt (t->supp_page_table, upage);
      }
      else 
      {
        add_file_supp_pt (t->supp_page_table, upage, file, ofs,
            page_read_bytes, page_zero_bytes, writable);
      }
      

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
      ofs += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;

  uint8_t *upage = ((uint8_t *) PHYS_BASE) - PGSIZE;
  kpage = get_new_frame (PAL_USER | PAL_ZERO, upage);
  if (kpage != NULL) 
    {
      success = install_page (upage, kpage, true);
      if (success)
      {
        *esp = PHYS_BASE;
      }
      else
      {
        destroy_frame (kpage, true);
      }
    }
  
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  bool success = (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable)
          && add_frame_supp_pt(t->supp_page_table, upage, kpage));
  if (success)
  {
    set_used (kpage, false);
  }
  return success;
}
