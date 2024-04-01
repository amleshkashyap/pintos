#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/fixed-point.h"
#include "threads/synch.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
typedef int pid_t;
typedef int mapid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

#define TNAME_MAX 32
#define MAX_CHILDREN 10
#define MAX_PRIORITY_DONATION 8
#define MAX_OPEN_FD 10
#define MAX_VADDR_MAPS 10
#define INITIAL_FD 2                   /* 0 and 1 are reserved values for stdin/stdout */

/* TODO: this is not handled cleanly, eg, for exec, child doesn't set this before doing a sema_up */
struct children {
  pid_t pid;
  int exit_status;
};

enum vaddr_map_type {
  MAP_LOAD_PAGES,
  MAP_STACK_PAGES,
  MAP_USER_FILES
};

struct vaddr_map {
  enum vaddr_map_type mtype;
  uint32_t *svaddr;
  uint32_t *evaddr;
  int fd;                          /* set to -1 for non-file mappings */
};

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[TNAME_MAX];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */

    /* for tracking sleeping threads */
    int64_t wakeup_at;
    bool sleeping;

    /* for priority donation */
    int actual_priority;
    int donated_priority[MAX_PRIORITY_DONATION];
    tid_t donated_to[MAX_PRIORITY_DONATION];
    struct lock *donated_for;
    int donations_made;
    int donations_held;

    /* for mlfqs */
    int nice;
    fxpoint recent_cpu;

    /* user programs */
    bool user_thread;
    pid_t pid;
    pid_t parent_pid;
    int exit_status;

    /* for quick retrieval */
    int child_threads;
    struct children t_children[MAX_CHILDREN];

    /* stores struct file which is opened during load, closed in thread_exit () */
    struct file *exfile;

    /* fds are per process */
    struct file* file_descriptors[MAX_OPEN_FD];
    int open_fds;

    /* parent does a sema_down and waits for exec'd child to complete load and do a sema_up */
    struct semaphore child_sema;

    /* all mappings */
    uint32_t *code_segment;
    uint32_t *data_segment;
    struct vaddr_map* vaddr_mappings[MAX_VADDR_MAPS];
    int active_vaddr_maps;
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

/* priority scheduling and donation */
void priority_schedule (struct thread *, struct thread *);
void donate_priority (struct thread *, struct thread *, struct lock *);
void reset_donated_priority (struct thread *);

void print_all_priorities (void);

/* sleep without busy waiting */
void thread_make_sleep (int64_t);
void thread_wakeup (struct thread *);

void clean_orphan_threads (void);

/* utility */
uint64_t total_ticks (void);
struct thread * get_thread_by_tid (int);
struct thread * get_thread_by_pid (pid_t);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void thread_set_load_avg (void);
void thread_set_recent_cpu (struct thread *t);
void thread_update_all_priorities (void);
void thread_recent_cpu_tick (void);
void thread_update_all_recent_cpu (void);

int all_ready_threads (void);

/* for syscalls */
int fetch_child_exit_status (pid_t);
void update_exit_status_for_parent (void);

/* for file syscalls */
bool is_valid_fd (int);
int allocate_fd (void);
void free_fd (int);
struct file * get_file (int);
void set_file (int, struct file*);

/* for mmap */
bool is_mappable_vaddr (void *);
mapid_t allocate_vaddr_mapid (void);
void free_vaddr_map (mapid_t);
void set_vaddr_map (mapid_t, enum vaddr_map_type, uint32_t *, int, int);

#endif /* threads/thread.h */
