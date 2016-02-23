#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

static void halt (void) NO_RETURN;
static void exit (int status) NO_RETURN;
static pid_t exec (const char *file);
static int wait (pid_t);
static bool create (const char *file, unsigned initial_size);
static bool remove (const char *file);
static int open (const char *file);
static int filesize (int fd);
static int read (int fd, void *buffer, unsigned length);
static int write (int fd, const void *buffer, unsigned length);
static void seek (int fd, unsigned position);
static unsigned tell (int fd);
static void close (int fd);


void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f)
{
  uint32_t * esp = f->esp;
  if (!valid_pointer(esp))
  {
    exit(-1);
  } else {
    int syscall_num = *esp;
    switch (syscall_num) {
      case SYS_HALT:
        halt ();
        break;
      case SYS_EXIT:
        exit (*(esp + 1));
        break;
      case SYS_EXEC:
        f->eax = exec ((char *) *(esp + 1));
        break;
      case SYS_WAIT:
        f->eax = wait (*(esp + 1));
        break;
      case SYS_CREATE:
        f->eax = create ((char *) *(esp + 1), *(esp + 2));
        break;
      case SYS_REMOVE:
        f->eax = remove ((char *) *(esp + 1));
        break;
      case SYS_OPEN:
        f->eax = open ((char *) *(esp + 1));
        break;
      case SYS_FILESIZE:
        f->eax = filesize (*(esp + 1));
        break;
      case SYS_READ:
        f->eax = read (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        break;
      case SYS_WRITE:
        f->eax = write (*(esp + 1), (void *) *(esp + 2), *(esp + 3));
        break;
      case SYS_SEEK:
        seek (*(esp + 1), *(esp + 2));
        break;
      case SYS_TELL:
        f->eax = tell (*(esp + 1));
        break;
      case SYS_CLOSE:
        close (*(esp + 1));
        break;
      default:
        break;
    }
  }
  
  
  //printf ("system call!\n");
  //thread_exit ();
}


void
halt (void)
{
  shutdown_power_off ();
}


void
exit (int status)
{
  struct child_status *child;
  struct thread * cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  struct thread * parent = find_thread (cur->parent_tid);
  if (parent != NULL) {
    for (struct list_elem * e = list_begin (&parent->children); e != list_end (&parent->children); e = list_next (e)) {
      
      child = list_entry (e, struct child, elem_child_status);
      
      if (child->child_id == cur->tid)
      {
        lock_acquire (&parent->lock_child);
        child->exited = true;
        child->child_exit_status = status;
        lock_release (&parent->lock_child);
      }
      
    }
    
  }
  thread_exit ();
  
}

pid_t
exec (const char *cmd_line)
{
  
}


int
wait (pid_t pid)
{

}


bool
valid_pointer(const void * ptr)
{
  if (uvaddr == NULL || !is_user_vaddr (ptr)) {
    return false;
  }
  struct thread * cur = thread_current ();
  void * res = pagedir_get_page (cur->pagedir, usr_ptr);
  return res != NULL;
}