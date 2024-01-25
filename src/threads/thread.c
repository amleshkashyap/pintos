#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static struct list multilevel_lists[PRI_MAX+1];

/* initialized to 0 */
static int ready_threads = 0;
static fxpoint load_average = 0;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (struct thread *);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);

  if (thread_mlfqs) {
    for (int i = 0; i <= PRI_MAX; i++) {
      list_init (&multilevel_lists[i]);
    }
  }

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
  ready_threads = 1;
  printf("First thread: %s, %d\n", initial_thread->name, initial_thread->tid);
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  /* this is a hack to be compatible with priority scheduling - idle thread will never be scheduled otherwise */
  thread_create ("idle", PRI_MAX, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  // printf("Thread tick for: %d at tick: %d\n", t->tid, timer_ticks ());

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();
  // printf("Thread with name, tid, priority %s, %d, %d\n", t->name, tid, t->priority);
  struct thread *cur = thread_current ();
  if (thread_mlfqs && tid > 2) {
    t->nice = cur->nice;
    // thread is yielded at the bottom
    t->priority = calculate_priority (0, t->nice);
    t->actual_priority = t->priority;
  }

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /* Add to run queue. */
  thread_unblock (t);

  /* Don't yield current thread during init
   * Assumes idle thread always has tid = 2 */
  if (t->tid > 2) {
    if (!thread_mlfqs) {
      priority_schedule (cur, t);
    } else {
      priority_schedule (cur, t);
    }
  }

  return tid;
}

/* Caller must ensure that mlfqs is disabled */
void
reset_donated_priority (struct thread *cur)
{
  ASSERT (!thread_mlfqs);
  int donations_made = cur->donations_made;
  if (donations_made <= 0) {
    return;
  }

  struct thread *t;
  tid_t tid;
  for (int i = 0; i < donations_made; i++) {
    tid = cur->donated_to[i];
    t = get_thread_by_tid (tid);
    t->donations_held--;
    if (t->donations_held <= 0) {
      t->priority = t->actual_priority;
    } else {
      t->priority = cur->donated_priority[i];
    }
    cur->donated_priority[i] = -1;
    cur->donated_to[i] = -1;
  }

  cur->donations_made = 0;
  cur->donated_for = NULL;
}

/*
 * This has to be called with interrupts switched off
 * Caller must ensure that mlfqs is disabled
 */
void
donate_priority (struct thread *cur, struct thread *holder, struct lock *l)
{
  ASSERT (intr_get_level () == INTR_OFF);

  int cur_priority = cur->priority;
  int holder_priority = holder->priority;

  if (cur_priority <= holder_priority) {
    return;
  }

  ASSERT (cur->donations_made == 0);
  ASSERT (holder->donations_made <= 7);

  cur->donated_for = l;
  cur->donations_made++;
  cur->donated_to[0] = holder->tid;
  cur->donated_priority[0] = holder_priority;

  holder->priority = cur_priority;
  holder->donations_held += 1;

  // printf("yielding from %d to %d, ticks: %d, priorities c: %d, old: %d, new: %d, at: %d\n", cur->tid, holder->tid, thread_ticks, cur_priority, holder_priority, holder->priority, timer_ticks ());

  if (holder->donations_made <= 0) {
    thread_yield();
  } else {
    struct thread *t;
    tid_t tid;

    ASSERT (intr_get_level () == INTR_OFF);
    for (int i = 0; i < holder->donations_made; i++) {
      tid = holder->donated_to[i];
      t = get_thread_by_tid (tid);
      ASSERT (is_thread (t));

      cur->donations_made++;
      cur->donated_to[i+1] = tid;
      cur->donated_priority[i+1] = t->priority;

      t->priority = cur_priority;
      t->donations_held++;
    }

    thread_yield();
  }
}

void
priority_schedule (struct thread *cur, struct thread *t)
{
  enum intr_level old_level;
  old_level = intr_disable ();
  // printf("cur tid, prio: %d, %d, t tid, prio: %d, %d\n", cur->tid, cur->priority, t->tid, t->priority);
  if (cur->priority < t->priority) {
    thread_yield ();
  }
  intr_set_level (old_level);
}

void
thread_make_sleep (int64_t new_wakeup_at)
{
  enum intr_level old_level = intr_disable ();
  struct thread *cur = thread_current ();
  cur->wakeup_at = new_wakeup_at;
  cur->sleeping = true;
  ready_threads--;
  intr_set_level (old_level);
  thread_yield ();
}

void
thread_wakeup (struct thread *t)
{
  ASSERT (intr_get_level () == INTR_OFF);
  t->wakeup_at = -1;
  t->sleeping = false;
  ready_threads++;
}

static void
wakeup_threads (void)
{
  ASSERT (intr_get_level () == INTR_OFF);
  struct list_elem *it;
  struct thread *t;
  for (it = list_begin (&all_list); it != list_end (&all_list); it = list_next (it)) {
    t = list_entry (it, struct thread, allelem);
    if (t->sleeping && t->wakeup_at <= timer_ticks ()) thread_wakeup (t);
  }
}

struct thread *
get_thread_by_tid (int tid)
{
  struct list_elem *e;
  struct thread *t;
  for(e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    t = list_entry (e, struct thread, allelem);
    if (t->tid == tid) {
      return t;
    }
  }
  return NULL;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  struct thread *cur = thread_current ();
  cur->status = THREAD_BLOCKED;
  if (cur->tid != 2) ready_threads--;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);

  if (t->tid != 2) ready_threads++;
  if (thread_mlfqs) {
    list_push_back (&multilevel_lists[t->priority], &t->elem);
  } else {
    list_push_back (&ready_list, &t->elem);
  }
  t->status = THREAD_READY;
  intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (t != NULL);
  /* check when does a thread get corrupted - usually at the end of program */
  ASSERT (t->magic == THREAD_MAGIC);
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  ready_threads--;
  schedule ();
  NOT_REACHED ();
}

uint64_t
total_ticks (void)
{
  return (idle_ticks + kernel_ticks + user_ticks + thread_ticks);
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) {
    if (thread_mlfqs) {
      int priority = cur->priority;
      list_push_back (&multilevel_lists[priority], &cur->elem);
    } else {
      list_push_back (&ready_list, &cur->elem);
    }
  }
  cur->status = THREAD_READY;
  schedule ();
  // printf("yield() for %d, at: %d, wake: %lld\n", cur->tid, timer_ticks (), cur->wakeup_at);
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  struct thread *cur = thread_current();
  if (thread_mlfqs) {
    // current thread is not in ready list
    cur->priority = calculate_priority (cur->recent_cpu, cur->nice);
    return;
  }

  int old_priority = cur->priority;
  // If thread has a donated priority, it shouldn't be able to lower its priority right now
  if (cur->priority != cur->actual_priority && new_priority < cur->priority) {
    cur->actual_priority = new_priority;
    return;
  }

  cur->priority = new_priority;
  cur->actual_priority = new_priority;
  /* if priority is increased, then don't check if scheduling is needed */
  if (new_priority >= old_priority) {
    return;
  }
  enum intr_level old_level;
  old_level = intr_disable ();
  struct list_elem *e;
  bool yield = false;
  struct thread *t;

  for (e = list_begin (&ready_list); e != list_end (&ready_list); e = list_next (e)) {
    t = list_entry (e, struct thread, elem);
    if (t->priority > new_priority && t->status == THREAD_READY && t->sleeping == false) {
        yield = true;
        break;
    }
  } 
  intr_set_level (old_level);
  // yield the thread, method disables the interrupt - should this be inside the above block?
  if (yield == true) {
    thread_yield ();
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* To update the priorities, start with highest priority threads
 * This ensures round robin orders are maintained
 * Additional variables can be used to prevent recomputation
*/
void
thread_update_all_priorities (void)
{
  enum intr_level old_level = intr_disable ();
  struct list_elem *it;
  struct list *tlist;
  struct thread *t;
  for (int i = PRI_MAX; i >= 0; i--) {
    tlist = &multilevel_lists[i];
    if (list_size (tlist) > 0) {
      for (it = list_begin (tlist); it != list_end (tlist); it = list_next (it)) {
        t = list_entry (it, struct thread, elem);
        int old_priority = t->priority;
        t->priority = calculate_priority (t->recent_cpu, t->nice);
        if (t->priority != old_priority) {
          struct list_elem *t_it = list_prev (it);
          list_remove (&t->elem);
          list_push_back (&multilevel_lists[t->priority], &t->elem);
          it = t_it;
	}
      }
    }
  }

  for (it = list_begin (&all_list); it != list_end (&all_list); it = list_next (it)) {
    t = list_entry (it, struct thread, allelem);
    if (t == idle_thread || t->status == THREAD_READY) continue;
    t->priority = calculate_priority (t->recent_cpu, t->nice);
  }
  intr_set_level (old_level);
}

void
print_all_priorities (void)
{
  ASSERT (intr_get_level () == INTR_OFF);
  if (thread_current () == idle_thread) return;
  struct list_elem *it;
  struct thread *t;
  printf("Printing thread state at: %lld\n", timer_ticks ());
  for (it = list_begin (&all_list); it != list_end (&all_list); it = list_next (it)) {
    t = list_entry (it, struct thread, allelem);
    printf("  tid: %d, prio: %d, nice: %d, rec: %lld\n", t->tid, t->priority, t->nice, t->recent_cpu);
  }
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int new_nice) 
{
  enum intr_level old_level = intr_disable ();
  struct thread *cur = thread_current ();
  cur->nice = new_nice;
  int old_priority = cur->priority;
  // current thread not in ready list
  cur->priority = calculate_priority (cur->recent_cpu, cur->nice);
  intr_set_level (old_level);
  if (cur->priority < old_priority) {
    thread_yield ();
  }
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current ()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  fxpoint temp = mult_fxpoint_int (load_average, 100);  
  return fxtoi_nearest (temp); // mult_fxpoint_int (load_average, 100));
}

void
thread_set_load_avg (void)
{
  load_average = calculate_load_avg (load_average, ready_threads);
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return fxtoi_nearest (mult_fxpoint_int (thread_current ()->recent_cpu, 100));
}

/* Setting of recent cpu doesn't lead to a change in priority
 * no shuffling required - hence, iterate over the alllist
*/ 
void thread_update_all_recent_cpu (void)
{
  ASSERT (intr_get_level () == INTR_OFF);
  struct list_elem *it;
  struct thread *t;
  for (it = list_begin (&all_list); it != list_end (&all_list); it = list_next (it)) {
    t = list_entry (it, struct thread, allelem);
    if (t == idle_thread) continue;
    thread_set_recent_cpu (t);
  }
}

void
thread_recent_cpu_tick (void)
{
  struct thread *cur = thread_current ();
  if (cur != idle_thread) cur->recent_cpu = add_fxpoint (cur->recent_cpu, F_FXPOINT);
}

void
thread_set_recent_cpu (struct thread *t)
{
  t->recent_cpu = calculate_recent_cpu (t->recent_cpu, load_average, t->nice);
}

int
all_ready_threads (void)
{
  return ready_threads;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  idle_thread->priority = PRI_MIN;
  idle_thread->actual_priority = PRI_MIN;
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      // printf("Idle at: %d\n", timer_ticks ());
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->magic = THREAD_MAGIC;
  t->wakeup_at = 0;
  t->sleeping = false;

  if (!thread_mlfqs) {
    t->priority = priority;
    t->actual_priority = priority;
    t->donations_held = 0;
    t->donations_made = 0;
  } else {
    t->nice = 0;
    t->recent_cpu = 0;
    t->priority = PRI_MAX;
    t->actual_priority = PRI_MAX;
  }

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* This method is same as the find_next_thread, but starts to check for available
 * threads in the multilevel list with highest priority. For mlfqs, two methods
 * are not required because this one finds the next available thread of highest priority
 * by default (round robin with highest priority)
*/
static struct thread *
find_next_thread_mlfqs (struct thread *cur UNUSED)
{
  enum intr_level old_level = intr_disable ();
  bool found = false;

  struct thread *next;
  struct list_elem *it;
  struct list_elem *t_max;
  struct list *tlist;

  for (int i = PRI_MAX; i >= 0; i--) {
    tlist = &multilevel_lists[i];
    if (list_size (tlist) > 0) {
      for (it = list_begin (tlist); it != list_end (tlist); it = list_next (it)) {
        next = list_entry (it, struct thread, elem);
        ASSERT (next->priority == i);
        if (next->sleeping || next->status != THREAD_READY) {
          continue;
        }
        t_max = it;
        found = true;
        break;
      }
      if (found) {
        next = list_entry (t_max, struct thread, elem);
        ASSERT (is_thread (next));
        list_remove (&next->elem);
        intr_set_level (old_level);
        return next;
      }
    }
  }
  ASSERT (is_thread (idle_thread));
  intr_set_level (old_level);
  return idle_thread;
}

/* When idle thread is running, it can be because there are no threads to run
 * or all other threads are sleeping/blocked. Former case doesn't lead to execution
 * of this method - in the latter case, it's possible that multiple threads woke up
 * in this scheduling event - however, the highest priority thread must be run, not
 * the first available thread (unlike the other method).
 * NOTE: if no thread wakes up/unblocks, then idle_thread is scheduled again
*/

struct thread *
find_next_thread (struct thread *cur)
{
  enum intr_level old_level = intr_disable ();
  int max_priority = -1;
  if (!cur->sleeping) max_priority = cur->priority;

  struct thread *next;
  struct list_elem *it;
  struct list_elem *t_pre;
  struct list_elem *t_max;
  bool found_cur = false;
  bool found_pre = false;
  bool found_max = false;

  for (it = list_begin (&ready_list); it != list_end (&ready_list); it = list_next (it)) {
    next = list_entry (it, struct thread, elem);
    if (next == cur) {
      found_cur = true;
      continue;
    }
    if (next->sleeping || next->status != THREAD_READY) {
      continue;
    }

    // Find any eligible threads before the current thread with same priority
    // for round robin, if no higher priority thread is found later
    if (!found_cur && !found_pre && next->priority == cur->priority) {
      t_pre = it;
      found_pre = true;
    }

    // This takes the highest priority -> an eligible thread at any place in the
    // ready queue with a priority larger than cur
    if (next->priority > max_priority) {
      t_max = it;
      max_priority = next->priority;
      found_max = true;
    }
  }

  /* Iterates from left to right of the ready thread, comparing priority
   * and thread status. Corner cases -
   *   1. Current thread was blocked/dying - it won't be found in ready_queue
   *   2. No thread w
   * 
  */ 
  if (max_priority == -1) {
    // cur is sleeping and no thread was found in the search -> schedule idle_thread
    next = idle_thread;
  } else if (max_priority > cur->priority) {
    // a thread with priority > cur was found
    next = list_entry (t_max, struct thread, elem);
    list_remove (t_max);
  } else if (found_pre) {
    // a thread with priority = cur was found before cur, perform round-robin
    next = list_entry (t_pre, struct thread, elem);
    list_remove (t_pre);
  } else if (!cur->sleeping && found_cur) {
    next = cur;
    list_remove (&cur->elem);
  } else if (found_max) {
    next = list_entry (t_max, struct thread, elem);
    list_remove (t_max);
  } else {
    // cur was blocked or dying
    ASSERT (found_cur == false);
    ASSERT (cur->status == THREAD_BLOCKED || cur->status == THREAD_DYING);
    next = idle_thread;
  }
  intr_set_level (old_level);
  return next;
}


/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (struct thread *cur) 
{
  if (thread_mlfqs) {
    return find_next_thread_mlfqs (cur);
  }

  return find_next_thread (cur);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  // printf("  thread_schedule_tail: current: %d, previous: %d\n", cur->tid, prev->tid);
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
/* the thread which is yielding the CPU is supposed to bear the cost of scheduling next thread too */
static void
schedule (void) 
{
  // sleeping threads shouldn't be woken up at any other place
  wakeup_threads ();
  struct thread *cur = running_thread ();
  struct thread *next;
  if (ready_threads == 0 && is_thread (idle_thread)) {
    next = idle_thread;
  } else {
    next = next_thread_to_run (cur);
  }
  struct thread *prev = NULL;
  // int cur_tid = cur->tid;
  // int next_tid = next->tid;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next) {
    prev = switch_threads (cur, next);
  }
  thread_schedule_tail (prev);
  // printf("Schedule() - c: %d, n: %d, at: %d\n", cur_tid, next_tid, timer_ticks ());
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
/* offsetof is a macro defined in lib/ to fetch the offset of a struct's member */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
