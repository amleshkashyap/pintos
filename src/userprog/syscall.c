#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "filesys/filesys.h"

static void syscall_handler (struct intr_frame *);

static bool
is_valid_addr (uint32_t *pd, const void *vaddr, size_t size)
{
  uint32_t *pde;
  const void *vaddr2 = vaddr + size;

  if (pd == NULL || !is_user_vaddr (vaddr) || !is_user_vaddr (vaddr2)) return false;

  if (pagedir_get_page (pd, vaddr) == NULL) return false;
  if (pagedir_get_page (pd, vaddr2) == NULL) return false;
  return true;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

int
get_argument (uint32_t *pagedir, void *esp, int offset, size_t size)
{
  int *arg = (int *) esp + offset;
  if (!is_valid_addr (pagedir, arg, size)) thread_exit ();
  return (int) *arg;
}

/* it's also possible to check if a given buffer actually contains upto size bytes? */
void *
get_referenced_argument (uint32_t *pagedir, void *esp, int offset)
{
  void *arg = get_argument (pagedir, esp, offset, sizeof (void **));
  if (!is_valid_addr (pagedir, arg, sizeof (void *))) thread_exit ();
  return arg;
}

static void
syscall_handler (struct intr_frame *f) 
{
  void *esp = (void *) f->esp;
  struct thread *cur = thread_current ();
  int syscall_num = get_argument (cur->pagedir, esp, 0, sizeof (int));

  // printf ("system call: %d\n", syscall_num);

  switch (syscall_num) {
    case SYS_OPEN:
      {
        // printf("Syscall open: %d\n", cur->tid);
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        int fd = open (file);
        // printf("Opened file: %s, tname: %s, tid: %d, fd: %d\n", file, cur->name, cur->tid, fd);
        ASSERT (fd >= INITIAL_FD || fd == -1);
        f->eax = fd;
        return;
      }
    case SYS_READ:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        // printf("process: %d, name: %s, reading fd: %d\n", cur->pid, cur->name, fd);
        if (fd == 1 || !is_valid_fd (fd)) {
          f->eax = 0;
          return;
        }
        void *buffer = get_referenced_argument (cur->pagedir, esp, 2);
        unsigned sz = get_argument (cur->pagedir, esp, 3, sizeof (unsigned));
        f->eax = read (fd, buffer, sz);
        return;
      }
    case SYS_WRITE:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        if (fd == 0 || !is_valid_fd (fd)) {
          f->eax = 0;
          return;
        }
        const void *buffer = (const void *) get_referenced_argument (cur->pagedir, esp, 2);
        unsigned sz = get_argument (cur->pagedir, esp, 3, sizeof (unsigned));
        f->eax = write (fd, buffer, sz);
        return;
      }
    case SYS_CLOSE:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        if (fd == 0 || fd == 1 || !is_valid_fd (fd)) {
          return;
        }
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
        int status = get_argument (cur->pagedir, esp, 1, sizeof (int));
        exit (status);
      }
    case SYS_WAIT:
      {
        pid_t pid = get_argument (cur->pagedir, esp, 1, sizeof (pid_t));
        int status = wait (pid);
        f->eax = status;
        return;
      }
    case SYS_CREATE:
      {
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        unsigned size = get_argument (cur->pagedir, esp, 2, sizeof (unsigned));
        f->eax = create (file, size);
        return;
      }
    case SYS_REMOVE:
      {
        const char *file = (const char *) get_referenced_argument (cur->pagedir, esp, 1);
        // printf(" --- Removing file: %s, tname: %s, tid: %d\n", file, cur->name, cur->tid);
        f->eax = remove (file);
        return;
      }
    case SYS_HALT:
      {
        halt ();
      }
    case SYS_FILESIZE:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        if (!is_valid_fd (fd)) {
          f->eax = 0;
          return;
        }
        f->eax = filesize (fd);
        return;
      }
    case SYS_SEEK:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        if (!is_valid_fd (fd)) return;
        unsigned pos = get_argument (cur->pagedir, esp, 2, sizeof (unsigned));
        seek (fd, pos);
        return;
      }
    case SYS_TELL:
      {
        int fd = get_argument (cur->pagedir, esp, 1, sizeof (int));
        if (!is_valid_fd (fd)) {
          f->eax = 0;
          return;
        }
        f->eax = tell (fd);
        return;
      }
    default:
      break;
  }
}

int
open (const char *file)
{
  int fd = allocate_fd ();
  if (fd == -1) {
    return -1;
  }

  struct file *t_file = filesys_open (file);
  if (t_file == NULL) return -1;
  set_file (fd, t_file);
  return fd;
}

int
read (int fd, void *buffer, unsigned size)
{
  return file_read (get_file (fd), buffer, size);
}

int
write (int fd, const void *buffer, unsigned size)
{
  // printf("Called write: fd - %d, size: %d, buffer: %s\n", fd, size, buffer);
  if (fd == 1) {
    putbuf (buffer, size);
  } else {
    return file_write (get_file (fd), buffer, size);
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
  shutdown_power_off ();
}

pid_t
exec (const char *cmdline)
{
  struct thread *cur = thread_current ();
  // printf("Trying to execute cmdline: %s, cur thread: %s-%d, cur children: %d\n", cmdline, cur->name, cur->tid, cur->child_threads);
  if (cur->child_threads == MAX_CHILDREN) return -1;

  tid_t tid = process_execute (cmdline);
  if (tid != TID_ERROR) {
    // printf("About to down the sema, cmdline: %s, tname: %s, sema: %d\n", cmdline, cur->name, cur->child_sema.value);
    sema_down (&cur->child_sema);
    struct thread *child = get_thread_by_pid (tid);
    if (child != NULL && child->exit_status == -2) return child->pid;
  }

  return -1;
}

int
filesize (int fd)
{
  return file_length (get_file (fd));
}

void
seek (int fd, unsigned position)
{
  file_seek (get_file (fd), position);
}

unsigned
tell (int fd)
{
  return file_tell (get_file (fd));
}
