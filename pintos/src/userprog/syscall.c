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
static int _read(int fd, void *buffer, unsigned size);
static int _write(int fd, const void *buffer, off_t size);
static void _seek(int fd, unsigned position, struct intr_frame *f UNUSED); 
static void _tell(int fd, struct intr_frame *f UNUSED); 
static void _close(int fd);
static int get_file_descriptor(struct file * file);
static int get_new_file_descriptor(struct file * file);
off_t get_file_position(struct file* file);
static struct file * search_file (struct thread* this, int fd);
struct lock lockFile;

void
syscall_init (void)  
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&lockFile);
}

//To validate a pointer is pointing to a legal user mem address before dereferencing it
static int
safe_esp (const void *pointer)
{
  struct thread *current = thread_current();

  if (pointer == NULL)
    _exit(-1);
  
  if (!is_user_vaddr (pointer) ||
      pagedir_get_page (current->pagedir, pointer) == NULL)
      _exit(-1);
  else
      return 1;
  
  return 0;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  // printf ("It's all good system call!\n");

  int *pointer = f->esp;
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
      
      case SYS_EXIT:
        if( !safe_esp(pointer + 1) )
          return;

        status = *( pointer + 1 );
        f->eax = status;
        _exit( status );
      break;
      
      case SYS_EXEC:                   /* Start another process. */
        if( !safe_esp(pointer + 1) )
          return;

        cmd_line =  (char*)*( pointer + 1);
        _exec(cmd_line, f);
      break;

      case SYS_WAIT:                   /* Wait for a child process to die. */
        if( !safe_esp(pointer + 1) )
          return;

        pid =  *( pointer + 1);
        _wait(pid, f); 
      break;

      case SYS_CREATE:

      if( !safe_esp(pointer + 1) || !safe_esp(pointer + 2) )
          return;

      lock_acquire(&lockFile);  
      
      file =  (char *) *( pointer + 1 );
      initial_size = *( pointer + 2 );
      _create (file, initial_size, f);
      
      lock_release(&lockFile);  
      break;                   
      
      case SYS_REMOVE:
      if( !safe_esp(pointer + 1) )
          return;
      
      lock_acquire(&lockFile); 

      file =  (char*)*(pointer + 1);
      _remove (file,f);

      lock_release(&lockFile);
      break;
      
      case SYS_OPEN:
      if( !safe_esp(pointer + 1) )
          return;

      lock_acquire(&lockFile); 
      
      file =  (char*)*(pointer + 1);
      _open (file,f);

      lock_release(&lockFile);
      break;

      case SYS_FILESIZE:
      if( !safe_esp(pointer + 1) )
          return;
      
      lock_acquire(&lockFile); 

      file =  (char*)*(pointer + 1);
      _filesize (file,f);
      
      lock_release(&lockFile);
      break;                  

      case SYS_READ:
      if( !safe_esp(pointer + 1) || !safe_esp(pointer + 2) || !safe_esp(pointer + 3))
          return;

      fd = *( pointer + 1 );
      f->eax = _read(fd, (void*)*(pointer + 2), *(pointer + 3));
      break;                   
      
      case SYS_WRITE:
      if( !safe_esp(pointer + 1) || !safe_esp(pointer + 2) || !safe_esp(pointer + 3))
          return;

      lock_acquire(&lockFile); 
      
      fd = *( pointer + 1 );
      size = *( pointer + 3 );
      f->eax = _write(fd, (void*)*(pointer + 2), size);

      lock_release(&lockFile);
      break;
      
      case SYS_SEEK:
      if( !safe_esp(pointer + 1) || !safe_esp(pointer + 2) )
          return;

      lock_acquire(&lockFile); 

      fd = *( pointer + 1 );
      position = *( pointer + 2 );
      _seek(fd, position,f);

      lock_release(&lockFile);                    
      break;

      case SYS_TELL:
      if( !safe_esp(pointer + 1) )
          return;

      fd = *( pointer + 1 );
      _tell(fd, f); 
      break;                   

      case SYS_CLOSE:
      if( !safe_esp(pointer + 1) )
          return;

      lock_acquire(&lockFile); 

      fd = *( pointer + 1 );
      _close(fd);

      lock_release(&lockFile);
      break;

      default:
      break;
    }
  }

  // thread_exit ();
}


static void _halt(void) 
{
  //Terminates Pintos by calling shutdown_power_off() in devices/shutdown.h
  //should be seldom used: lose some info about possible deadlock situtations
  shutdown_power_off();
}

static void _exit(int status)
{
  // printf("***************\n");
  // printf("entered exit\n");
  struct thread *current = thread_current();
  struct thread *child_t;

  struct list_elem *mov;

  /* traversing our child thread list */
  for( mov = list_begin( &(current->list_childThread) );
       mov != list_end( &(current->list_childThread) );
       mov = list_next( mov ) )
  {
    /* getting the thread from the list element*/
    child_t = list_entry (mov, struct thread, child_elem);
    
    /* checking tid */
    if(child_t->tid == child_t->parent->tid)
      break;
  }

  // //Child exited
  // child_t->terminate_status = 1;

  //Child exit status

  ASSERT (child_t != NULL)

  current->exit_status = status;
  list_remove( &current->child_elem );

  //Sema up
  sema_up( &(current->parent->wait_sema_child) );
  sema_down(  &(current->parent->wait_sema_zombie) );


  //Close the file
  struct list_elem *node;
  struct list_elem *ahead;

  for (node = list_begin (&current->list_openfile);
       node != list_end (&current->list_openfile);
       node = list_next (node))
    {
      struct file_description * fd_node = list_entry (node, struct file_description, elem_openfile);

      _close (fd_node->assigned_fd);
    }

  printf ("%s: exit(%d)\n", current->name, current->exit_status);
  
  
  sema_up( &(current->parent->wait_sema_child) );
  
  thread_exit();
}


static int _read(int fd, void *buffer, unsigned size)
{
  struct thread * current = thread_current();
  struct file * file;

  if (!safe_esp(buffer))
    return -1;

  if (!safe_esp(buffer + size))
    return -1;

  if (fd == 0)
  {

    unsigned i = 0;
    uint8_t *b = buffer;
    
    for (; i < size; ++i)
      b[i] = (char) input_getc ();

    return size;
  }
  else
  {
    file = search_file(current, fd);
    return file_read (file, buffer, size);
  }

}


static int _write(int fd, const void *buffer, off_t size)
{
  // struct file * file = current->open_files[fd];
  struct thread * current = thread_current();
  struct file * file;
  int max_buffer = 100;

  //check std input
  if (fd == STDIN_FILENO)
    return -1;

  if (!safe_esp(buffer + size))
    return -1;

  if ( !safe_esp(buffer) )
    return -1;

  if ( fd == 1 ){
      
      putbuf(buffer, size);
      
      // if (size < max_buffer)
      // {
      //   return size;
      // }
      // else
      // {
      //   int offset = 0;
        
      //   // while (size > max_buffer)
      //   // {
      //   //   putbuf (buffer +, max_buffer);
      //   //   offset += max_buffer;
      //   //   size -= max_buffer;
      //   // }

      //   putbuf (buffer, size);
      //   offset += size;
      //   return offset;
      // }

  }
  else{

    file = search_file(current, fd);
    
    if(file)
      file_write (file, buffer, size);

    return size;
  }

}


static void _open(const char *file, struct intr_frame *f UNUSED)
{

  if(!safe_esp(file))
    return;

  struct file * open_file;
  struct file_description * fd_obj;
  // fd_obj = (struct file_description*) malloc (sizeof(struct file_description));
  fd_obj = palloc_get_page(0);

  open_file = filesys_open (file);
  
  if(open_file){
    struct thread * current = thread_current();


    fd_obj->assigned_fd = current->file_descriptor_num ++;

    fd_obj->open_file = open_file;
    
    f->eax = fd_obj->assigned_fd;

    list_push_back (&current->list_openfile, &fd_obj->elem_openfile);
  }
  else
    f->eax = -1;
}

static void _filesize(int fd, struct intr_frame *f UNUSED) 
{

  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);

  f->eax = file_length (file); 
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

  if(!safe_esp(file))
    return -1;

  f->eax = filesys_create(file, initial_size); 
}
/* Deletes the file called file. Returns true if successful, false 
 * otherwise. A file may be removed regardless of whether it is open or 
 * closed, and removing an open file does not close it. See Removing an 
 * Open File, for details.
 */
static void _remove(const char *file, struct intr_frame *f UNUSED)
{
  if(!safe_esp(file))
    return;

  f->eax = filesys_remove(file); 
}

static void _seek(int fd, unsigned position, struct intr_frame *f UNUSED) 
{
  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);
  
  file_seek (file, position);
}

static void _tell(int fd, struct intr_frame *f UNUSED) 
{
  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);

  f->eax = file_tell (file);
}

static void _close(int fd)
{
  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);
  
  if(file)
    file_close (file);
}

static struct file* search_file (struct thread * this, int fd){

  struct file_description * fd_obj;
  struct list_elem * node ;

  for ( node = list_begin( &(this->list_openfile) );
        node != list_end (&(this->list_openfile) ); 
        node = list_next(node) )
  {
       // file = list_entry (node, struct thread, elem_openfile);
      fd_obj = list_entry (node, struct file_description, elem_openfile);

       if(fd_obj->assigned_fd == fd)
          return fd_obj->open_file;
  }
  
  return NULL;
}