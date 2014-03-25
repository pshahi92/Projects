#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void *parseline (void *esp, const char *file_name); //parsing the command line
//might need to change return type

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  //Kim driving
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (file_name, PRI_DEFAULT, start_process, fn_copy);

  //everytime a thread is created start_process is called, which in turn calls load
  //thread create runs through start_process

  struct thread * child_thread;
  child_thread = search_thread_tid(tid);

  //Eros driving
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy);
  else{

    sema_down(&thread_current()->load_sema);
    
    if(thread_current()->success_code == 0)
      _exit(thread_current()->tid);
    
    if(child_thread)
    {
      char executable_prog[128];
      char file_name_duplicate[128];

      memcpy(file_name_duplicate, file_name, strlen(file_name) + 1);
      memcpy(executable_prog, file_name_duplicate, strlen(file_name) + 1);
      strtok_r(executable_prog, " ", &file_name_duplicate);

    if(filesys_open(executable_prog) == NULL)
      return -1;

      list_push_back(&(thread_current()->list_childThread), &(child_thread->child_elem));
    }
    else
      return -1;
  }
  return tid;
}

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;


  success = load (file_name, &if_.eip, &if_.esp);
  
  struct thread * current = thread_current();
  //calling load from inside start_process
  //inside load the stack is set up

  /* If load failed, quit. */
  palloc_free_page (file_name);
  
  if (!success)
  {
    thread_exit ();
  }

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid UNUSED) 
{
  //Prithvi driving
  //implementing infinite loop (that waits forever)
  //Pintos will hang forever but we will get to see output from child processes
if (child_tid == TID_ERROR)
    return TID_ERROR;

  struct thread *current = thread_current ();

  /* taking the current aka parent thread */

  /* we are blocking the parent process on the child process's semaphore
   * because we are implementing the wait the parent has to do until the child
   * process terminates inside wait
   */
  struct list_elem *node = list_begin( &(current->list_childThread) );
  /* getting the front of the list for list traversal aka finding matching
   * child_tid
   */

  struct thread *child_t;
  int found = 0; //have we found the child thread?

  /* traversing our child thread list */
  while (node != list_end( &(current->list_childThread) ) ) {
    /* getting the thread from the list element*/
    child_t = list_entry (node, struct thread, child_elem);
    
    /* checking tid */
    if(child_t->tid == child_tid) 
    {
      /* found matching child_tid */
      break;
    }

    node = list_next( node );
  }

  int child_status;

  if(child_t->tid == child_tid)
  {
    /* since we have found our child thread, we wait on the semaphore to wait
     * for the child process to finish
     */
    
    //has waited =)
    if(current->wait_status){
      return -1;
    }
    //having the current parent thread wait on the child thread until
    //the child finishes execution
    sema_down( &(child_t->wait_sema_child) );
        //updating the child's exit status
    child_status = child_t->exit_status;
        //the child is now done so we can remove from the list
    list_remove( &child_t->child_elem );
  }
  else
  {
    child_status = -1;
  }


  return child_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  if(cur->executable_file)
    file_close(cur->executable_file);
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

static bool setup_stack (void **esp, const char *file_name);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  //creates stack
  struct thread *t = thread_current ();

  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  //Kim Driving
  //Grab the executable first argument
  char executable_prog[128];
  char file_name_duplicate[128];

  memcpy(file_name_duplicate, file_name, strlen(file_name) + 1);
  memcpy(executable_prog, file_name_duplicate, strlen(file_name) + 1);
  strtok_r(executable_prog, " ", &file_name_duplicate);

  /* Open executable file. */
  // file = filesys_open (file_name);
  file = filesys_open (executable_prog);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
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
  if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

  t->executable_file = file;
  file_deny_write(t->executable_file);
 
 done:
  /* We arrive here whether the load is successful or not. */
  // printf("file close before (ERROR)\n");
  if(!success)
    file_close (file);

  if(thread_current()->parent != NULL)
  {
        /* this is to solve the race condition on exec missing
    we need to wake up the parent thread which has been waiting on the
    child to finish loading*/
    thread_current()->parent->success_code = 1;
    sema_up(&thread_current()->parent->load_sema);
  }

  return success;
}

/* load() helpers. */

void *parseline (void *esp, const char *file_name)
{
  //Prithvi Driving
  char *cmd_copy; //strtok_r deletes the string so we need a copy to store file_name
  char *str_arg; //this is to check the return 
  char *saveptr; //remaining argument
  int argumentV[128]; //array to store arguments
  int numArgument = 0;

  cmd_copy = palloc_get_page(0);
  memcpy(cmd_copy, file_name, (strlen(file_name)+1));
  str_arg = strtok_r(cmd_copy, " ", &saveptr);

  if(cmd_copy != NULL)
    strlcpy(thread_current()->name, str_arg, strlen(str_arg) + 1 );

  // printf("Print this out \n");
  // printf("Other argument:  _----->            %s\n", str_arg);

  while( str_arg != NULL ) //strrok_r returns a null when we reach the end of the cmdline argument
  {
    esp -= (strlen(str_arg) + 1 ); //decrementing the esp pointer by the lenght of the argument we received in cmdline
    //strlen will not account for the lenght of the space so we have to decrement once more
    //now we have to store the data and the address inside a data structure, we will use an array
    
    //Kim driving
    memcpy(esp, str_arg, (strlen(str_arg) + 1) ); //push into stack
    // printf("THis is current             -> %s\n", esp);

    argumentV[numArgument] = esp; //get the address of argument
    numArgument++; //number of argument

    str_arg = strtok_r(NULL, " ", &saveptr); // Do not need to grab the cmd_copy again
    // printf("Other argument i :  _----->            %s\n", str_arg, numArgument);
  }

  //Kim driving
  //push word align
  uint8_t memPadding = ((unsigned int ) esp ) % 4;
  esp -= memPadding;

  // printf("\nTrying to get word_align value:            %s\n", esp);

  //push a zero
  esp -= 4;
  *(int*)esp = 0;

  // printf("\nTrying to push dummy into esp:            %s\n\n\n", esp);
  
  int i = numArgument -1 ;
  
  while (i >= 0){
    esp-= 4;
    *(int*)esp = argumentV[i]; 
    // memcpy(esp, &argumentV[i], 4);
    
    // printf("\nPush in backward esp:            %p     %x \n\n\n", esp, argumentV[i]);
    //push argv
    if( i == 0 ){
      // printf("we are here eeeee \n");
      esp-= 4;
      
      // printf("I want this address so much           %p\n", (esp+4));
      *(int*)esp = esp + 4;
    }

    i--;
  }

  //push argc
  esp-= 4;
  memcpy(esp, &numArgument, 4);

  //push retrun address
  esp-= 4;
  *(int*)esp = 0;

  //pintos --filesys-size=2 -p ../../examples/abin/ls -a abin/ls -- -f -q run 'abin/ls -l foo bar'
  // hex_dump(esp, esp, PHYS_BASE - esp, 1);

  return esp;
}

static bool install_page (void *upage, void *kpage, bool writable);

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

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *kpage = palloc_get_page (PAL_USER);
      if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (kpage);
          return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          palloc_free_page (kpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, const char *file_name) 
{
  //we have to pass the cmdline all the way into setup_stack and parse here!!

  uint8_t *kpage;
  bool success = false;

  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
        *esp = PHYS_BASE; //Prithvi driving
        *esp = parseline(*esp, file_name);
      }
      else
        palloc_free_page (kpage);
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
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}
