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
// static void _exit (int status);
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

//To validate a pointer is pointing to a legal 
//user mem address before dereferencing it
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
  unsigned size;
  unsigned position, initial_size;

  if( safe_esp(pointer) )
  {
    sys_num = *((int*)pointer); 

    switch(sys_num)
    {
      //kim driving
      case SYS_HALT:
        _halt();                            /* Halt the operating system. */
      break;                   
      
      //prithvi driving
      case SYS_EXIT:
        if( !safe_esp(pointer + 1) )
          return;

        status = *( pointer + 1 );
        f->eax = status;
        _exit( status );
      break;
      
      //eros driving
      case SYS_EXEC:                   /* Start another process. */
        if( !safe_esp(pointer + 1) ){
          return;
        }

        cmd_line =  (char*)*( pointer + 1);
        _exec(cmd_line, f);
      break;

      //abraham driving
      case SYS_WAIT:                   /* Wait for a child process to die. */
        if( !safe_esp(pointer + 1) )
          return;

        pid =  *( pointer + 1);
        _wait(pid, f); 
      break;

      //prithvi driving
      case SYS_CREATE:

      /* all these locks are to make sure we are safely accessing the pointers
      Kim driving */
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
      
      //Eros driving
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

      //Prithvi driving
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

      //Kim driving
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

      //Abraham driving
      lock_acquire(&lockFile); 

      fd = *( pointer + 1 );
      _close(fd);

      lock_release(&lockFile);
      break;

      default:
      break;
    }
  }
}


static void _halt(void) 
{
  //Terminates Pintos by calling shutdown_power_off() in devices/shutdown.h
  //should be seldom used: lose some info about possible deadlock situtations
  shutdown_power_off();
}
/*
Terminates the current user program, returning status to the kernel. 
If the process's parent waits for it (see below), this is the status that will 
be returned. Conventionally, a status of 0 indicates success and nonzero values 
indicate errors.
*/
void _exit(int status)
{
    //Eros driving
  struct thread *current = thread_current();
  
  current->exit_status = status;
    
  sema_up( &(current->wait_sema_child) );
    
  current->wait_status = 0;
  
  printf ("%s: exit(%d)\n", current->name, current->exit_status);
  
  thread_exit();
}

/*
Reads size bytes from the file open as fd into buffer. Returns the number of 
bytes actually read (0 at end of file), or -1 if the file could not be read 
(due to a condition other than end of file). fd 0 reads from the keyboard using 
input_getc().
*/
static int _read(int fd, void *buffer, unsigned size)
{
    //Abraham driving

  struct thread * current = thread_current();
  struct file * file;

  //checking for bad pointer
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
  else if(fd > 1)
  {
    int len;

    file = search_file(current, fd);

    if(file)
      len = file_read (file, buffer, size);

    return len;
  }

  return -1;
}

/*
Writes size bytes from buffer to the open file fd. Returns the number of bytes 
actually written, which may be less than size if some bytes could not be written.
Writing past end-of-file would normally extend the file, but file growth is not 
implemented by the basic file system. The expected behavior is to write as many 
bytes as possible up to end-of-file and return the actual number written, or 0 
if no bytes could be written at all.

fd 1 writes to the console. Your code to write to the console should write all 
of buffer in one call to putbuf(), at least as long as size is not bigger than a
few hundred bytes. (It is reasonable to break up larger buffers.) Otherwise, 
lines of text output by different processes may end up interleaved on the 
console, confusing both human readers and our grading scripts.
*/
static int _write(int fd, const void *buffer, off_t size)
{
  //Prithvi driving
  struct thread * current = thread_current();
  struct file * file;
  int max_buffer = 100;

  //check std input
  if (fd == STDIN_FILENO)
    return -1;

  //check for bad pointer
  if (!safe_esp(buffer + size))
    return -1;

  if ( !safe_esp(buffer) )
    return -1;

  if ( fd == 1 ){   
    putbuf(buffer, size); 
    return 0;
  }
  else
  {

    file = search_file(current, fd);

    if(file)
    {
      size = file_write (file, buffer, size);
    }

    return size;
  }

}

/*
Opens the file called file. Returns a nonnegative integer handle called a 
"file descriptor" (fd) or -1 if the file could not be opened.
File descriptors numbered 0 and 1 are reserved for the console: 
fd 0 (STDIN_FILENO) is standard input, fd 1 (STDOUT_FILENO) is standard output. 
The open system call will never return either of these file descriptors, which 
are valid as system call arguments only as explicitly described below.

Each process has an independent set of file descriptors. File descriptors are 
not inherited by child processes.

When a single file is opened more than once, whether by a single process or 
different processes, each open returns a new file descriptor. Different file 
descriptors for a single file are closed independently in separate calls to 
close and they do not share a file position.
*/
static void _open(const char *file, struct intr_frame *f UNUSED)
{

  if(!safe_esp(file))
    return;

  struct file * open_file; //new struct for the file we are opening
  struct file_description * fd_obj; //creating a file description object

  //Have to free
  //pallocing the page for fd obj
  fd_obj = palloc_get_page(0);
  //opening the file 
  open_file = filesys_open (file);
  
  if(open_file) //checking the file is not null
  {
    struct thread * current = thread_current();
    fd_obj->assigned_fd = current->file_descriptor_num ++;
    //setting assigned_fd of fd_obj struct
    fd_obj->open_file = open_file;
    //this is the open file we want
    f->eax = fd_obj->assigned_fd;
    //updated eax register
    list_push_back (&current->list_openfile, &fd_obj->elem_openfile);
    //putting onto list  

  }
  else
    f->eax = -1;
}
/*
Returns the size, in bytes, of the file open as fd.
*/
static void _filesize(int fd, struct intr_frame *f UNUSED) 
{
  //Abraham driving
  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);

  f->eax = file_length (file); 
}
/*
Runs the executable whose name is given in cmd_line, passing any given arguments
, and returns the new process's program id (pid). Must return pid -1, 
which otherwise should not be a valid pid, if the program cannot load or run 
for any reason. Thus, the parent process cannot return from the exec until it 
knows whether the child process successfully loaded its executable. You must 
use appropriate synchronization to ensure this.
*/
static void _exec(const char *cmd_line, struct intr_frame *f UNUSED)
{
    //Abraham driving
  f->eax = process_execute(cmd_line);
}

/* Waits for a child process pid and retrieves the child's exit status.
 */
static void _wait(pid_t pid, struct intr_frame *f UNUSED)
{
      //Eros driving

   f->eax = process_wait(pid);
}
/*
Creates a new file called file initially initial_size bytes in size. Returns 
true if successful, false otherwise. Creating a new file does not open it: 
opening the new file is a separate operation which would require a open system 
call.
*/
static void _create(const char *file, unsigned initial_size, struct intr_frame *f UNUSED)
{
  //Eros driving
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
  //kim driving
  if(!safe_esp(file))
    return;

  f->eax = filesys_remove(file); 
}

/*
Changes the next byte to be read or written in open file fd to position, 
expressed in bytes from the beginning of the file. (Thus, a position of 0 is the
file's start.)
A seek past the current end of a file is not an error. A later read obtains 0 
bytes, indicating end of file. A later write extends the file, filling any 
unwritten gap with zeros. (However, in Pintos, files will have a fixed length 
until project 4 is complete, so writes past end of file will return an error.) 
These semantics are implemented in the file system and do not require any 
special effort in system call implementation.
*/
static void _seek(int fd, unsigned position, struct intr_frame *f UNUSED) 
{
    //Eros driving
  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);
  
  file_seek (file, position);
}
/*
Returns the position of the next byte to be read or written in open file fd, 
expressed in bytes from the beginning of the file.
*/
static void _tell(int fd, struct intr_frame *f UNUSED) 
{
    //Eros driving

  struct thread * current = thread_current();
  struct file * file;
  file = search_file(current, fd);

  f->eax = file_tell (file);
}
/*
Closes file descriptor fd. Exiting or terminating a process implicitly closes 
all its open file descriptors, as if by calling this function for each one.
*/
static void _close(int fd)
{
    //Eros driving

  struct thread * current = thread_current();
  struct list_elem * node ;
  struct file_description * fd_obj;

  
  for ( node = list_begin( &(current->list_openfile) );
        node != list_end (&(current->list_openfile) ); 
        node = list_next(node) )
  {
      fd_obj = list_entry (node, struct file_description, elem_openfile);

       if(fd_obj->assigned_fd == fd){
          file_close(fd_obj->open_file);
          list_remove (node);
          palloc_free_page (fd_obj);

          return;
       }
  }

}

/* This method is the search method we use to search for our file in file list
need to use this because we use a list data structure rather than an array
*/
static struct file* search_file (struct thread * this, int fd)
{
  //Kim driving

  struct file_description * fd_obj;
  struct list_elem * node ;

  for ( node = list_begin( &(this->list_openfile) );
        node != list_end (&(this->list_openfile) ); 
        node = list_next(node) )
  {
      fd_obj = list_entry (node, struct file_description, elem_openfile);

       if(fd_obj->assigned_fd == fd)
          return fd_obj->open_file;
  }
  
  return NULL;
}
