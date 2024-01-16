## 32-bit Registers (switch.S)
  * ebx - base register (bx), used for indexed addressing
  * ebp - base pointer (bp), reference pointer to the parameters passed in subroutine
    - ss + bp -> location of parameter
    - di/si + bp -> special addressing
  * esi - source index (si), for string operations
  * edi - destination index (di), for string operations
  * eax - accumulator (ax), stores operands in arithmetic instructions
  * ecx - counter (cx), stores loop count in iterative operations
  * edx - data register (dx), stores operands for mult/div operations with large values
  * esp - stack pointer (sp), provides offset for program stack
    - ss + sp -> location of data/address in program stack
  * eip - instruction pointer (ip), offset of next instruction to be executed
    - cs + ip -> complete address of current instruction in code segment

  * cs - stores the starting address of code segment (instructions to be executed)
  * ds - stores the starting address of data segment (data, constants, work areas)
  * ss - stores the starting address of stack segment (data and return address for procedures)

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
  * initial\_thread
  * idle\_thread - 
  * tid\_lock - 
  * kernel\_thread\_frame - return address (eip), function to execute, arguments (aux)
  * list\_elem - struct containing previous and next elements of the list in which the element belongs


## Functions
  * NOTE: calling printf() in the interrupt handler causes too much output from interrupts and other output is lost
    - Reduce the frequency of interrupts or print periodically

  * NOTE: Pintos runs only 1 thread at any time - hence if any pintos method is running, calling methods like current\_thread() and
    thread\_yield() refers to the execution of these methods for the thread being called upon.
    - Eg, a sleep() method which performs thread\_yield() means the sleeping thread is supposed to not do anything and simply yield the CPU
      for scheduling - at the end of thread\_yield(), current thread remains the same, suggesting the actual switch happens later.
    - When a method is not called from a thread, then it has to be the first kernel thread with id-1.
    - Add debug statements to see which thread is current and which thread is running the schedule method during returning back CPU (yield)

  * thread\_yield - finds the current thread using running\_thread method which uses assembly code to identify running thread information
    directly from the hardware.
    - Disables interrupts
    - If the thread is not idle already, puts it at the back of ready list, and marks the thread is ready
    - Calls the schedule method which might schedule it again
    - Sets interrupt back to the previous state
    - Method can be called by any thread (eg, via timer\_sleep)

  * next\_thread\_to\_run - fetches the first thread from the ready list and schedules it. If none are present, then idle\_thread is
    scheduled. idle\_thread simply runs an infinite loop at the beginning of while, it marks itself as blocked and calls schedule()
    method.
    - This method can be updated to implement the sleep() method -> mark the sleeping thread as blocked and update a struct member to
      when it's supposed to wake up - when this method is run, it'll ignore all blocked threads whose wakeup time is in future and push
      them back to the ready list - if all threads in the ready list are sleeping, it'll return the idle\_thread.
    - Other way I could think of is to maintain another list of blocked threads - at every timer interrupt, all the threads in that list
      are evaluated for being qualified to return back to ready list - it would have the same complexity but would require introducing
      and maintaining another data structure.
