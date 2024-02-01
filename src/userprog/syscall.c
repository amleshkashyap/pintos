#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
int write (int, void *, unsigned);
void exit (int);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f) 
{
  struct thread *cur = thread_current ();
  void *esp = pagedir_get_page (cur->pagedir, (void *) f->esp);
  int syscall_num = *(int *) esp;
  printf ("system call!\n");
  // printf("call_num: %d, tid: %d, stack ptr: %x, esp: %x\n", syscall_num, cur->tid, cur->stack, f->esp);
  if (syscall_num == 9) {
    int fd = *((int *) esp + 1);
    void *buffer = *((int *) esp + 2);
    unsigned sz = *((int *) esp + 3);
    write (fd, buffer, sz);
  } else if (syscall_num == 1) {
    int status = *((int *) esp + 1); 
    exit (status);
  }

  // thread_exit ();
}

int
write (int fd, void *buffer, unsigned size)
{
  printf("Called write: fd - %d, size: %d, buffer: %s\n", fd, size, buffer);
  return size;
}

void
exit (int status)
{
  printf("Exit status: %d\n", status);
  thread_exit ();
  NOT_REACHED ();
}
