#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

static bool
is_valid_addr (uint32_t *pd, const void *vaddr)
{
  uint32_t *pde;

  ASSERT (pd != NULL);
  ASSERT (is_user_vaddr (vaddr));

  pde = pd + pd_no (vaddr);
  if (*pde == 0) return false;
  return true;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void *
get_referenced_argument (uint32_t *pagedir, void *esp, int offset)
{
  void *arg = *((int *) esp + offset);
  if (!is_valid_addr (pagedir, arg)) thread_exit ();
  return arg;
}

static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = (void *) f->esp;
  struct thread *cur = thread_current ();
  if (!is_valid_addr (cur->pagedir, esp)) {
    exit (-1);
  }

  int syscall_num = *(int *) esp;
  // printf ("system call: %d\n", syscall_num);

  switch (syscall_num) {
    case SYS_OPEN:
      {
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        f->eax = open (file);
        return;
      }
    case SYS_READ:
      {
        int fd = *((int *) esp + 1);
        void *buffer = get_referenced_argument (cur->pagedir, esp, 2);
        unsigned sz = *((int *) esp + 3);
        f->eax = read (fd, buffer, sz);
        return;
      }
    case SYS_WRITE:
      {
        int fd = *((int *) esp + 1);
        const void *buffer = (const void *) get_referenced_argument (cur->pagedir, esp, 2);
        unsigned sz = *((int *) esp + 3);
        f->eax = write (fd, buffer, sz);
        return;
      }
    case SYS_CLOSE:
      {
        int fd = *((int *) esp + 1);
        close (fd);
        return;
      }
    case SYS_EXEC:
      {
        const char *cmdline = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        pid_t pid = exec (cmdline);
        f->eax = pid;
        return;
      }
    case SYS_EXIT:
      {
        int status = *((int *) esp + 1);
        exit (status);
      }
    case SYS_WAIT:
      {
        pid_t pid = *((int *) esp + 1);
        int status = wait (pid);
        f->eax = status;
        return;
      }
    case SYS_CREATE:
      {
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        unsigned size = *((int *) esp + 2);
        f->eax = create (file, size);
        return;
      }
    case SYS_REMOVE:
      {
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        f->eax = remove (file);
        return;
      }
    case SYS_HALT:
      {
      }
    case SYS_FILESIZE:
      {
      }
    case SYS_SEEK:
      {
      }
    case SYS_TELL:
      {
      }
    default:
      break;
  }
}

int
open (const char *file)
{
  if (thread_current ()->open_fds > MAX_OPEN_FD_THREAD) return -1;
  int fd = allocate_fd ();
  if (fd == -1) {
    return -1;
  }

  struct file_desc *fdsc = get_file_descriptor (fd);
  struct file *t_file = filesys_open (file);
  fdsc->fd = fd;
  fdsc->pid = thread_current ()->pid;
  strlcpy (&fdsc->filename, file, 50);
  fdsc->t_file = t_file;
  return fd;
}

int
read (int fd, void *buffer, unsigned size)
{
  struct file_desc *fdsc = get_file_descriptor (fd);
  struct file *t_file = fdsc->t_file;
  return file_read (t_file, buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  // printf("Called write: fd - %d, size: %d, buffer: %s\n", fd, size, buffer);
  if (fd == 1) {
    putbuf (buffer, size);
  } else {
    struct file_desc *fdsc = get_file_descriptor (fd);
    struct file *t_file = fdsc->t_file;
    return file_write (t_file, buffer, size);
  }
  return size;
}

void
close (int fd)
{
  free_fd (fd);
}

void
exit (int status)
{
  // printf("%s: exit syscall(%d)\n", thread_current ()->name, status);
  thread_current ()->exit_status = status;
  thread_exit ();
  NOT_REACHED ();
}

int
wait (pid_t pid)
{
  int exit_status;
  for (; ;) {
    exit_status = fetch_child_exit_status (pid);
    if (exit_status != -2) return exit_status;
  }
  NOT_REACHED ();
}

bool
create (const char *file, unsigned initial_size)
{
  return filesys_create (file, initial_size);
}

bool
remove (const char *file)
{
  return filesys_remove (file);
}

void
halt (void)
{
}

pid_t
exec (const char *cmdline)
{
  struct thread *cur = thread_current ();
  if (cur->child_threads == MAX_CHILDREN) return -1;

  sema_init (&cur->child_sema, 0);
  tid_t tid = process_execute (cmdline);
  if (tid != TID_ERROR) {
    sema_down (&cur->child_sema);
    struct thread *child = get_thread_by_pid (tid);
    if (child != NULL) return child->pid;
  }

  return -1;
}

int
filesize (int fd)
{
  return 0;
}

void
seek (int fd, unsigned position)
{
}

unsigned
tell (int fd)
{
  return 0;
}
