## I32-bit Register Related Structures
  * struct intr\_frame (all members are int32 corresponding to 32-bit registers unless specified)
    - edi - destination index (di), for string operations
    - esi - source index (si), for string operations
    - ebp - base pointer (bp), reference pointer to the parameters passed in subroutine
      - ss + bp -> location of parameter
      - di/si + bp -> special addressing
    - esp\_dummy - Not used
    - ebx - base register (bx), used for indexed addressing
    - edx - data register (dx), stores operands for mult/div operations with large values
    - ecx - counter (cx), stores loop count in iterative operations
    - eax - accumulator (ax), stores operands in arithmetic instructions
    - (int16) gs
    - (int16) fs
    - (int16) es
    - (int16) ds - stores the starting address of data segment (data, constants, work areas)
    - vec\_no - interrupt vector number
    - error\_code - 
    - (void \*) frame\_pointer
    - (void \*) eip (void) - instruction pointer (ip), offset of next instruction to be executed
      - cs + ip -> complete address of current instruction in code segment
    - (int16) cs - stores the starting address of code segment (instructions to be executed)
    - eflags
    - (void \*) esp - stack pointer (sp), provides offset for program stack
      - ss + sp -> location of data/address in program stack
    - (int16) ss - stores the starting address of stack segment (data and return address for procedures)

  * struct switch\_threads\_frame
    - edi
    - esi
    - ebp
    - ebx
    - (void \*) eip (void)
    - struct thread \*cur
    - struct thread \*next

  * struct switch\_entry\_frame
    - (void \*) eip (void)

  * struct kernel\_thread\_frame
    - (void \*) eip - Return address (for?)
    - (thread\_func \*) function - Function to be executed
    - (void \*) aux - Auxiliary data for the function

  * struct tss (all members are int16, unless specified)
    - back\_link
    - (void \*) esp0
    - ss0
    - (void \*) esp1
    - ss1
    - (void \*) esp2
    - ss2
    - (int32) cr3
    - (void \*) eip (void)
    - (int32) - eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi
    - es, cs, ss, ds, fs, gs
    - ldt
    - trace
    - bitmap

  * Control Registers - overflow (OF, 11), direction (DF, 10), interrupt (IF, 9), trap (TF, 8), sign (SF, 7), zero (ZF, 6),
    auxiliary carry(AF, 4), parity (PF, 2), carry (CF, 0)
    - 2nd element is bit position in a 16-bit flag register

## Instructions (switch.S)
  * mov
  * movl - 
  * pushl - push source operand (32-bits) to the stack
  * popl - 

## Data Structures
  * ready\_list - list of threads ready to run, in THREAD\_READY state - retrieve/push via 'elem'
  * all\_list - all processes - added when first scheduled, removed when they exit - retrieve/push via 'allelem'
  * initial\_thread - reference to the main thread with tid 1
  * idle\_thread - reference to the idle thread with tid 2
  * tid\_lock - 
  * kernel\_thread\_frame - return address (eip), function to execute, arguments (aux)
  * list\_elem - struct containing previous and next elements of the list in which the element belongs
  * semaphore - consists of value and list of waiters
  * lock - consists of pointer to holder thread and semaphore whose value is initialized to 1


## Functions
  * NOTE: calling printf() in the interrupt handler causes too much output from interrupts and other output is lost
    - Reduce the frequency of interrupts or print periodically

  * NOTE: Pintos runs only 1 thread at any time - hence if any pintos method is running, calling methods like current\_thread() and
    thread\_yield() refers to the execution of these methods for the thread being called upon.
    - Eg, a sleep() method which performs thread\_yield() means the sleeping thread is supposed to not do anything and simply yield the CPU
      for scheduling - at the end of thread\_yield(), current thread remains the same, suggesting the actual switch happens later.
    - When a method is not called from a thread, then it has to be the first kernel thread with tid 1.
    - When there's no thread in the ready list -> when will this happen? initial\_thread is killed at the end and will always be present
      in the ready list.
    - Add debug statements to see which thread is current and which thread is running the schedule method during returning back CPU (yield)

  * schedule - This method expects that the current running thread is not in RUNNING state anymore (which is done by thread\_yield,
    thread\_block and thread\_exit methods - marks them READY, BLOCKED and DYING).
    - It will find the next thread to be scheduled via next\_thread\_to\_run method - earlier, this method will simply return the first
      thread in ready list and if the list is empty, it'll return the idle thread (which is initialised in main and does nothing but runs
      an infinite loop where it calls the schedule method again.
    - All the scheduling related code has to be tied up with finding the next thread to schedule.
    - After finding the next thread, it will switch the threads, set the static thread\_tick variable to 0 (this variable is used to
      perform thread switching based on timer interrupts, every 4 ticks ideally) and mark the selected thread as RUNNING.
    - This method should be called with interrupts switched off as it needs to be sequential till completion - when any function is called
      within this (eg, next\_thread\_to\_run, it needn't disable the interrupts, but good to assert as is done in tail).

    - thread\_block -> ensures current thread is set to BLOCKED state, execution is not within external interrupt and interrupts are off
      - sema\_down -> called by lock\_acquire and cond\_wait
      - idle
      - wait (interrupt queue)
    - thread\_exit -> ensures current thread is set to DYING state, execution is not within external interrupt and interrupts are off
      - kernel\_thread
      - main - called only once
    - thread\_yield -> ensures current thread is set to READY state, execution is not within external interrupt and interrupts are off
      - intr\_handler -> called during any interrupt (primary: timer interrupts)
      - O: timer\_sleep
      - N: donate\_priority -> called only by lock\_acquire
      - N: priority\_schedule -> called only by thread\_create
      - N: thread\_make\_sleep -> called only by timer\_sleep
      - N: thread\_set\_priority -> not called in code
      - N: lock\_release -> called by - (1) allocate\_tid, (2) cond\_wait, (3) palloc\_get\_multiple, malloc, free, (4) getc, putc

    - Most relevant scheduling workflows
      - timer\_interrupt -> intr\_handler -> thread\_yield -> schedule -- round robin CPU scheduling
      - functions (same as lock\_release)/user programs -> lock\_acquire -> sema\_down -> thread\_block -> schedule
 
      - N: functions/user programs -> lock\_acquire -> donate\_priority -> thread\_yield -> schedule -- schedule after priority donation
      - N: functions/user programs -> lock\_release -> thread\_yield -> schedule -- revoke priority after releasing lock
      - N: thread\_create -> priority\_schedule -> thread\_yield -> schedule -- schedule high priority thread after creation

      - N: user programs -> timer\_sleep -> thread\_make\_sleep -> thread\_yield -> schedule -- unschedule a sleeping thread
      - N: user programs -> thread\_set\_priority (for running thread) -> thread\_yield -> schedule -- schedule after priority reduction

  * thread\_yield - finds the current thread using running\_thread method which uses assembly code to identify running thread information
    directly from the hardware.
    - Disables interrupts
    - If the thread is not idle already, puts it at the back of ready list, marks the thread as ready and calls schedule()
    - Sets interrupt back to the previous state
    - Method can be called by any thread (eg, via timer\_sleep)

  * idle - This method is called only once by the idle thread - when it first gets the CPU, it sets a variable idle\_thread (during kernel
    initialization) which is used later. After that, it runs an infinite loop - at the beginning of the loop, it calls the thread\_block
    method which will perform scheduling. idle\_thread has tid 2.
    - There's a similar method which sets a variable initial\_thread which is for the main thread with tid 1.

  * next\_thread\_to\_run - fetches the first thread from the ready list and schedules it. If none are present, then idle\_thread is
    scheduled. It's modified to do the scheduling in 2 ways -
    - If idle\_thread has the context, then it must yield to the highest priority thread which is not sleeping.
      - If no non-sleeping thread is found, then schedule idle\_thread again
    - Otherwise, find a non-sleeping thread with a higher priority
      - If no such thread is found, schedule idle\_thread again - let's say a higher priority thread exists but its sleeping, then this
        method will schedule a lower priority thread -> this maybe incorrect. Also, if there are only lower priority threads, then the
        current thread could've been rescheduled - this is inefficient, but in the next scheduling cycle, that's remedied so not too bad.
    - The implementation for sleeping doesn't remove a thread from the ready list for now, but it ideally should for decoupling various
      additional scheduling strategies that need to be implemented.

## Workflows
  * Initialization
    - Create first thread - store as initial\_thread immediately, add to ready queue
    - Create second thread - add to ready queue -> when executed first, store as idle\_thread -> never add to ready queue again
    - Further operations in run\_actions

  * Scheduling
    - Timer Interrupt -> intr\_handler () -> timer\_interrupt () -> possibly schedule another thread (if TIME\_SLICE thread\_ticks ())
    - Sleep -> schedule another thread
    - Blocked by semaphore/lock -> schedule another thread
    - Unblocked by semaphore/lock -> add to ready queue
    - Increase/decrease priority -> possibly schedule another thread
    - Create new thread -> possibly schedule another thread based on priority
    - Update nice value -> increase/decrease priority -> possibly schedule another thread
    - When a thread has completed -> schedule another thread
    - At the time of scheduling -> wakeup all sleeping threads.
    - Counting threads -
      - increase when new thread is added to ready list, or woken up from sleep
      - reduce when a thread is blocked, made to sleep or when completed
      - never count second thread

  * Priority Donation
    - While Acquiring Lock -> check the priority of holder -> if lower, increase its priority -> if it has donated to threads, increase the
      priority of all the threads.
    - After Acquiring Lock -> check if donated priority to threads -> if yes, change their priorities
    - After Releasing Lock -> check if it has received any donations -> if yes, then yield the CPU immediately
    - After Releasing Lock -> check if there are waiting threads -> if yes, unblock the highest priority thread -> if that thread has made
      donations, then put it at the front of the ready list.
    - There's no actual recursive method for nested priority donation.