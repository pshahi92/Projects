#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"


#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "lib/user/syscall.h"
#include "devices/shutdown.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/off_t.h"

static void syscall_handler (struct intr_frame *);

static int safe_esp (const void *pointer);
static void _halt (void);
static void _exit (int status);
static void _exec (const char *cmd_line, struct intr_frame *f UNUSED);
static void _wait (pid_t pid, struct intr_frame *f UNUSED);
static void _create (const char *file, unsigned initial_size, struct intr_frame *f UNUSED);
static void _remove(const char *file, struct intr_frame *f UNUSED);
static void _open(const char *file, struct intr_frame *f UNUSED);
static void _filesize(int fd, struct intr_frame *f UNUSED); 
static void _read(int fd, void *buffer, unsigned size, struct intr_frame *f UNUSED);
static void _write(int fd, const void *buffer, off_t size, struct intr_frame *f UNUSED);
static void _seek(int fd, unsigned position, struct intr_frame *f UNUSED); 
static void _tell(int fd, struct intr_frame *f UNUSED); 
static void _close(int fd);
static int get_file_descriptor(struct file * file);
static int get_new_file_descriptor(struct file * file);
off_t get_file_position(struct file* file);
static struct file * search_file (struct thread* this, int fd);

void
syscall_init (void)  
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//To validate a pointer is pointing to a legal user mem address before dereferencing it
static int
safe_esp (const void *pointer)
{
  struct thread *current = thread_current();

  if (pointer == NULL)
    return 0; 
  
  if (!is_user_vaddr (pointer) ||
      pagedir_get_page (current->pagedir, pointer) == NULL)
      return 0;
  else
      return 1;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("It's all good system call!\n");

  void *pointer = f->esp;
  int status, pid, sys_num, fd;
  char *cmd_line, *file;
  void *buffer;
  off_t size;
  unsigned position, initial_size;

  if( safe_esp(pointer) )
  {

    sys_num = *((int*)pointer); 

    switch(sys_num)
    {
      case SYS_HALT:
        _halt();                                 /* Halt the operating system. */
      break;                   
      
      // case SYS_EXIT:
      //   status = *((int*)(pointer + 1));
      //   f->eax = status;
      //   _exit(status);
      // break;
      
      // case SYS_EXEC:                   /* Start another process. */
      //   cmd_line =  *((char*)(pointer + 1));
      //   _exec(cmd_line, f);
      // break;

      case SYS_WAIT:                   /* Wait for a child process to die. */
        pid =  *((int*)(pointer + 1));
        _wait(pid, f); 
      break;

      case SYS_CREATE:
      file =  *((char*)(pointer + 1));
      initial_size = *((unsigned*)(pointer + 2));
      _create (file, initial_size, f);
      break;                   
      
      case SYS_REMOVE:
      file =  *((char*)(pointer + 1));
      _remove (file,f);
      break;
      
      case SYS_OPEN:
      file =  *((char*)(pointer + 1));
      _open (file,f);                   
      break;

      // case SYS_FILESIZE: 
      // file =  *((char*)(pointer + 1));
      // _filesize (file,f);
      // break;                  

      // case SYS_READ:
      // fd = *((int*)(pointer + 1));
      // _read(fd, *((void **)(pointer + 2)), (pointer + 3), f);
      // break;                   
      
      // case SYS_WRITE:
      // fd = *((int*)(pointer + 1));
      // size = *((unsigned *)(pointer + 3));
      // _write(fd, *((void **)(pointer + 2)), size,f);
      // break;
      
      // case SYS_SEEK:
      // fd = *((int*)(pointer + 1));
      // position = *((unsigned *)(pointer + 2));
      // _seek(fd, position,f);                    
      // break;

      // case SYS_TELL:
      // fd = *((int*)(pointer + 1));
      // _tell(fd, f); 
      // break;                   

      // case SYS_CLOSE:
      // fd = *((int*)(pointer + 1));
      // _close(fd);                   
      // break;

      default:
      break;
    }
  }

  thread_exit ();
}


static void _halt(void) 
{
  //Terminates Pintos by calling shutdown_power_off() in devices/shutdown.h
  //should be seldom used: lose some info about possible deadlock situtations
  shutdown_power_off();
}

// static void _exit(int status)
// {
//   // printf("***************\n");
//   // printf("entered exit\n");
//   struct thread *current = thread_current();
//   struct thread *child_t;

//   struct list_elem *mov = list_begin( &(current->list_childThread) );
//   struct list_elem *tail = list_end( &(current->list_childThread) );

//   /* traversing our child thread list */
//   while( mov != tail )
//   {
//     /* getting the thread from the list element*/
//     child_t = list_entry (mov, struct thread, child_elem);
    
//     /* checking tid */
//     if(child_t->tid == child_t->parent->tid)
//       break;
    
//     mov = list_next( &(current->list_childThread) );
//   }

//   //Child exited
//   child_t->terminate_status = 1;

//   //Child exit status
//   child_t->exit_status = status;

//   //Sema up
//   sema_up( &(current->wait_sema_child) );

//   //Close the file
//   struct list_elem *node;
//   struct list_elem *ahead;

//   for (node = list_begin (&current->list_openfile);
//        node != list_end (&current->list_openfile);
//        node = ahead)
//     {
//       struct file_description * fd_node = list_entry (node, struct file_description, elem_openfile);
//       ahead = list_next (node);

//       _close (fd_node->assigned_fd);
//     }

//   // printf ("%s: exit(%d)\n", thread_current()->, thread_current()->exit_status);
  
//   thread_exit();
// }


static void _open(const char *file, struct intr_frame *f UNUSED)
{
  struct file * open_file;
  struct file_description * fd_obj;
  // fd_obj = (struct file_description*) palloc (sizeof(struct file_description));
  fd_obj = palloc_get_page(0);

  open_file = filesys_open (file);
  
  if(open_file){
    struct thread * current = thread_current();

    f->eax = open_file;
    
    current->file_descriptor_num += 1;
    fd_obj->assigned_fd = current->file_descriptor_num;
    fd_obj->open_file = open_file;
    
    list_push_back (&current->list_openfile, &fd_obj->elem_openfile);
  }
  else
    f->eax = -1;
}

static void _exec(const char *cmd_line, struct intr_frame *f UNUSED)
{
  struct thread* current = thread_current();
  
  f->eax = process_execute(cmd_line);
}

/* Waits for a child process pid and retrieves the child's exit status.
 */
static void _wait(pid_t pid, struct intr_frame *f UNUSED)
{

   f->eax = process_wait(pid);
}

static void _create(const char *file, unsigned initial_size, struct intr_frame *f UNUSED)
{
  f->eax = filesys_create(file, initial_size); 
}
/* Deletes the file called file. Returns true if successful, false 
 * otherwise. A file may be removed regardless of whether it is open or 
 * closed, and removing an open file does not close it. See Removing an 
 * Open File, for details.
 */
static void _remove(const char *file, struct intr_frame *f UNUSED)
{
  f->eax = filesys_remove(file); 
}