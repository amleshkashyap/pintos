#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  // thread_current () is serving this syscall, it doesn't need the page explicitly
  // void *esp = pagedir_get_page (thread_current ()->pagedir, (void *) f->esp);
  void *esp = (void *) f->esp;
  int syscall_num = *(int *) esp;
  printf ("system call: %d\n", syscall_num);

  switch (syscall_num) {
    case SYS_OPEN:
      {
      }
    case SYS_READ:
      {
      }
    case SYS_WRITE:
      {
        int fd = *((int *) esp + 1);
        void *buffer = *((int *) esp + 2);
        unsigned sz = *((int *) esp + 3);
        write (fd, buffer, sz);
        return;
      }
    case SYS_CLOSE:
      {
      }
    case SYS_EXEC:
      {
        const char *cmdline = *((const char *) esp + 1);
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
      }
    case SYS_CREATE:
      {
      }
    case SYS_REMOVE:
      {
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
  return 0;
}

int
read (int fd, void *buffer, unsigned size)
{
  return 0;
}

int
write (int fd, const void *buffer, unsigned size)
{
  printf("Called write: fd - %d, size: %d, buffer: %s\n", fd, size, buffer);
  return size;
}

void
close (int fd)
{
}

void
exit (int status)
{
  printf("Exit status: %d, for thread: %d\n", status, thread_current ()->tid);
  thread_current ()->exit_status = status;
  thread_exit ();
  NOT_REACHED ();
}

int
wait (pid_t pid)
{
  struct thread *cur = thread_current ();
  if (cur->child_threads < 1) {
    // throw and return
    return -1;
  }

  bool found = false;
  for (int i = 0; i < cur->child_threads; i++) {
    if (cur->children[i] == pid) {
      found = true;
      break;
    }
  }

  if (!found) {
    // throw and return
    return -1;
  }

  struct thread *t = get_exited_user_thread_by_pid (pid);
  if (t != NULL) {
    list_remove (&t->allelem);
    // eax?
    return t->exit_status;
  }

  t = get_thread_by_pid (pid);

  if (t == NULL) {
    // impossible situation
    return -1;
  }

  while (t->exit_status == -2) continue;
  return t->exit_status;
}

bool
create (const char *file, unsigned initial_size)
{
  return false;
}

bool
remove (const char *file)
{
  return false;
}

/* Others */
void
halt (void)
{
}

pid_t
exec (const char *cmdline)
{
  tid_t tid = process_execute (cmdline);
  if (tid != TID_ERROR) {
    return get_thread_by_tid (tid)->pid;
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
