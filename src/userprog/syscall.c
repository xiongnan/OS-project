#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/vaddr.h"
#include "userprog/pagedir.h"
#include "threads/malloc.h"

struct file_descriptor
{
  int fd_num;
  tid_t owner;
  struct file *file_struct;
  struct list_elem elem;
};

struct list open_files;


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

static  bool valid_pointer(const void * ptr);
static struct file_descriptor *get_open_file (int);
static void close_open_file (int);
static int allocate_fd (void);

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
  struct child * this_child;
  struct thread * cur = thread_current ();
  printf ("%s: exit(%d)\n", cur->name, status);
  struct thread * parent = find_thread (cur->parent_tid);
  if (parent != NULL) {
    struct list_elem * e;
    for (e = list_begin (&parent->children); e != list_end (&parent->children); e = list_next (e)) {
      
      this_child = list_entry (e, struct child, elem_child_status);
      
      if (this_child->child_id == cur->tid)
      {
        lock_acquire (&parent->lock_child);
        this_child->exited = true;
        this_child->exit_status = status;
        lock_release (&parent->lock_child);
      }
      
    }
    
  }
  thread_exit ();
  
}

pid_t
exec (const char *cmd_line)
{
  /* a thread's id. When there is a user process within a kernel thread, we
   * use one-to-one mapping from tid to pid, which means pid = tid
   */
  tid_t tid;
  struct thread *cur;
  
  /* check if the user pinter is valid */
  if (!valid_pointer (cmd_line))
  {
    exit (-1);
  }
  
  cur = thread_current ();
  cur->child_load_status = 0;
  tid = process_execute (cmd_line);
  lock_acquire(&cur->lock_child);
  while (cur->child_load_status == 0)
    cond_wait(&cur->cond_child, &cur->lock_child);
  if (cur->child_load_status == -1)
    tid = -1;
  lock_release(&cur->lock_child);
  return tid;
}


int
wait (pid_t pid)
{
  return process_wait(pid);
}

bool
create (const char *file_name, unsigned size)
{
  bool status;
  
  if (!valid_pointer (file_name))
    exit (-1);
  
  lock_acquire (&fs_lock);
  status = filesys_create(file_name, size);
  lock_release (&fs_lock);
  return status;
}


bool
remove (const char *file_name)
{
  bool status;
  if (!valid_pointer (file_name))
    exit (-1);
  
  lock_acquire (&fs_lock);
  status = filesys_remove (file_name);
  lock_release (&fs_lock);
  return status;
}

int
open (const char *file_name)
{
  struct file *f;
  struct file_descriptor *fd;
  int status = -1;
  
  if (!is_valid_ptr (file_name))
    exit (-1);
  
  lock_acquire (&fs_lock);
  
  f = filesys_open (file_name);
  if (f != NULL)
  {
    fd = calloc (1, sizeof *fd);
    fd->fd_num = allocate_fd ();
    fd->owner = thread_current ()->tid;
    fd->file_struct = f;
    list_push_back (&open_files, &fd->elem);
    status = fd->fd_num;
  }
  lock_release (&fs_lock);
  return status;
}

int
filesize (int fd)
{
  struct file_descriptor *fd_struct;
  int status = -1;
  lock_acquire (&fs_lock);
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_length (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  struct thread *t = thread_current ();
  
  unsigned buffer_size = size;
  void * buffer_tmp = buffer;
  
  /* check the user memory pointing by buffer are valid */
  while (buffer_tmp != NULL)
  {
    if (!is_valid_uvaddr (buffer_tmp))
      exit (-1);
    
    if (pagedir_get_page (t->pagedir, buffer_tmp) == NULL)
    {
      struct suppl_pte *spte;
      spte = get_suppl_pte (&t->suppl_page_table,
                            pg_round_down (buffer_tmp));
      if (spte != NULL && !spte->is_loaded)
        load_page (spte);
      else if (spte == NULL && buffer_tmp >= (esp - 32))
        grow_stack (buffer_tmp);
      else
        exit (-1);
    }
    
    /* Advance */
    if (buffer_size == 0)
    {
      /* terminate the checking loop */
      buffer_tmp = NULL;
    }
    else if (buffer_size > PGSIZE)
    {
      buffer_tmp += PGSIZE;
      buffer_size -= PGSIZE;
    }
    else
    {
      /* last loop */
      buffer_tmp = buffer + size - 1;
      buffer_size = 0;
    }
  }
  
  lock_acquire (&fs_lock);
  if (fd == STDOUT_FILENO)
    status = -1;
  else if (fd == STDIN_FILENO)
  {
    uint8_t c;
    unsigned counter = size;
    uint8_t *buf = buffer;
    while (counter > 1 && (c = input_getc()) != 0)
    {
      *buf = c;
      buffer++;
      counter--;
    }
    *buf = 0;
    status = size - counter;
  }
  else
  {
    fd_struct = get_open_file (fd);
    if (fd_struct != NULL)
      status = file_read (fd_struct->file_struct, buffer, size);
  }
  lock_release (&fs_lock);
  return status;
}

int
write (int fd, const void *buffer, unsigned size)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  
  unsigned buffer_size = size;
  void *buffer_tmp = buffer;
  
  /* check the user memory pointing by buffer are valid */
  while (buffer_tmp != NULL)
  {
    if (!is_valid_ptr (buffer_tmp))
      exit (-1);
    
    /* Advance */
    if (buffer_size > PGSIZE)
    {
      buffer_tmp += PGSIZE;
      buffer_size -= PGSIZE;
    }
    else if (buffer_size == 0)
    {
      /* terminate the checking loop */
      buffer_tmp = NULL;
    }
    else
    {
      /* last loop */
      buffer_tmp = buffer + size - 1;
      buffer_size = 0;
    }
  }
  
  lock_acquire (&fs_lock);
  if (fd == STDIN_FILENO)
  {
    status = -1;
  }
  else if (fd == STDOUT_FILENO)
  {
    putbuf (buffer, size);;
    status = size;
  }
  else
  {
    fd_struct = get_open_file (fd);
    if (fd_struct != NULL)
      status = file_write (fd_struct->file_struct, buffer, size);
  }
  lock_release (&fs_lock);
  
  return status;
}


void
seek (int fd, unsigned position)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock);
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    file_seek (fd_struct->file_struct, position);
  lock_release (&fs_lock);
  return ;
}

unsigned
tell (int fd)
{
  struct file_descriptor *fd_struct;
  int status = 0;
  lock_acquire (&fs_lock);
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL)
    status = file_tell (fd_struct->file_struct);
  lock_release (&fs_lock);
  return status;
}

void
close (int fd)
{
  struct file_descriptor *fd_struct;
  lock_acquire (&fs_lock);
  fd_struct = get_open_file (fd);
  if (fd_struct != NULL && fd_struct->owner == thread_current ()->tid)
    close_open_file (fd);
  lock_release (&fs_lock);
  return ; 
}



int
allocate_fd ()
{
  static int fd_current = 1;
  return ++fd_current;
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

struct file_descriptor *
get_open_file (int fd)
{
  struct list_elem *e;
  struct file_descriptor *fd_struct;
  e = list_tail (&open_files);
  while ((e = list_prev (e)) != list_head (&open_files))
  {
    fd_struct = list_entry (e, struct file_descriptor, elem);
    if (fd_struct->fd_num == fd)
      return fd_struct;
  }
  return NULL;
}

void
close_open_file (int fd)
{
  struct list_elem *e;
  struct list_elem *prev;
  struct file_descriptor *fd_struct;
  e = list_end (&open_files);
  while (e != list_head (&open_files))
  {
    prev = list_prev (e);
    fd_struct = list_entry (e, struct file_descriptor, elem);
    if (fd_struct->fd_num == fd)
    {
      list_remove (e);
      file_close (fd_struct->file_struct);
      free (fd_struct);
      return ;
    }
    e = prev;
  }
  return ;
}

void
close_file_by_owner (tid_t tid)
{
  struct list_elem *e;
  struct list_elem *next;
  struct file_descriptor *fd_struct;
  e = list_begin (&open_files);
  while (e != list_tail (&open_files))
  {
    next = list_next (e);
    fd_struct = list_entry (e, struct file_descriptor, elem);
    if (fd_struct->owner == tid)
    {
      list_remove (e);
      file_close (fd_struct->file_struct);
      free (fd_struct);
    }
    e = next;
  }
}
