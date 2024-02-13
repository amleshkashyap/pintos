## Data Structures
  * Stack Pointers
    - In threads
    - In interrupt frames
    - In TSS

  * Virtual Memory - Kernel vs User
    - The interrupt frame for user process is contructed in the kernel
    - In the interrupt frame, the stack pointer must point to an address according to the virtual memory for the user

  * Elf32\_Addr - 
  * Elf32\_Word
  * Elf32\_Off
  * Elf32\_Half

  * Executable Header (Elf32\_Ehdr)
  * Program Header (Elf32\_Phdr)

  * Interrupt

## Functions
  * process\_execute
    - Takes a filename as the argument, makes a copy of the file in a new page.
    - Creates a new thread with start\_process and the above copy of file as argument - Frees the new page if there was an error

  * start\_process
    - Takes the filename as an argument
    - Allocates and initializes new intr\_frame structure
    - Sets all the segment registers and calls the load () method with the eip and esp of the new interrupt frame.
    - If load fails, frees the page and exits.
    - Starts the user process by calling assembly commands that simulate return from an interrupt - since syscalls will work as interrupts?

  * load (char \*file\_name, void (\*\*eip) (void), void \*\*esp)
    - Allocates and activates page directory.
    - Opens the given file and verifies that its an ELF object (by loading and comparing the fields in ELF header, if present)
    - Iterates through the Program Header table, looking for all the loadable segments.
      - If it doesn't find any, exits with error.
      - If it finds an entry of type PT\_SHLIB, exits with error
    - It validates all the loadable segments by comparing some of the attributes - if validation fails for any of them, exits with error.
    - After validating a segment, it loads the segment (ie, add it to process' address space) - exits with error if it fails.
    - Sets up the stack by mapping a zeroed page at the top of user virtual memory.
    - Stores the entry point of the executable in eip and stack pointer in esp.
    - Related functions - validate\_segment (), load\_segment (), setup\_stack (), install\_page ()

  * load\_segment (struct file \*file, off\_t ofs, uint8\_t \*upage, uint32\_t read\_bytes, uint32\_t zero\_bytes, bool writable)
    - Loads a segment starting at offset ofs in file, at address upage.
    - read\_bytes in upage are read from file starting at offset ofs
    - zero\_bytes at upage + read\_bytes must be zeroed.
    - Initialized pages are writable by user process if the param is true, else read-only.
    - Returns false if memory allocation or disk read error occurs.

  * setup\_stack (void \*\*esp)
    - Gets a zeroed out page from allocator.
    - Adds a mapping from user virtual address (say upage) to kernel virtual address (say kpage) in the page table. upage must not be
      already mapped and kpage is to be obtained from user pool. Page should ideally be writable by user.

  * filesys\* and file\* methods - For file related system calls, these can be used without any modification, they handle the accounting
    for newly created files as well as number of file write denials. Mapping of the file created by filesys to fd has to be handled via the
    syscalls.

## Workflows
  * Initialization
    - In Make.vars, USERPROG is defined
    - This initializes task state segment and global descriptor tables
    - Also initializes the exception and syscall modules
    - Instead of executing the run\_test () (defined in tests/threads/tests.c), task execution is done via
      process\_wait (process\_execute (task)) (defined in userprog/process.c). process\_wait does nothing initially.

  * Process Execution
    - Gets a page from kernel space (flag = 0), copies the given filename, creates a thread and returns at completion.
      - If an error is signalled from the created thread, frees the allocated page.
      - Copying of filename is only done for 1 page size -> hence the restriction of 4KB for arguments.
    - Created thread executes start\_process method, which internally calls the load () method
      - load () method does all the actual work of execution

  * System Calls
    - A user process is same as a thread -> however, once they make a system call, the thread switches to the kernel mode where everything
      is accessible. Since user programs have their own virtual memory, going from 0 to PHYS\_BASE, in kernel mode, it should ensure that
      it doesn't allow itself to read any data from the kernel space, except for performing the accounting activities (eg, getting
      page table information for validation, etc).
    - More security check can be enforced here to block user programs from harming other threads as in kernel mode, it can effect them - for
      now though, a child thread changes the data of its parent thread (parent reads the data of child thread).
    - Once the boundaries are checked, there's no other protective mechanisms.

## Synchronization
  * A semaphore is present for every thread which waits for exec'd child to finish loading.
  * In process\_wait, an infinite for loop runs and waits for a given tid to return a NULL value, meaning the thread is removed from the
    system (ie, killed), hence the executor can stop waiting - needs to be fixed to be same as wait (as it has to return exit status) - for
    now though, it's only used by kernel thread and hence works fine.
  * wait syscall is similar, but it exits once the child's exit\_status becomes different from -2 (the initial value).

## Concerns
  * Certain tests don't work with 2MB filesys hence its changed to 4MB by default.
  * Filesys open method fails to load an existing program - dir\_close is commented for it to work.
  * FDs are kept to max of 10 per process - in multi-oom, upto 126 fds are attempted for allocation.
  * FD management methods are added in threads directory, but they maybe somewhere else too.
  * A process can't exec more than 10 children, even though they've died -> it must wait for them if it wants to clean such children.
  * Memory validity checks and handling of corresponding page fault/exception should be done using the second way.

## Program Startup
  * 80x86 Calling Convention
    - Caller pushes each function argument on the stack one by one, using PUSH. Arguments are pushed in right to left order.
      - Stack grows downwards - each push decrements the stack pointer and stores the location it now points to.
    - Caller pushes the address of the next instruction (return address) on the stack and jumps to the first instruction of callee.
      - CALL instruction does the above two actions.
      - Callee executes - stack pointer is pointing to the return address, first argument is above it, second above the former, and so on.
      - Callee stores the return value, if any, into EAX.
      - Callee returns by popping return address from stack and jumping to the location it specifies, using RET instruction.
    - Caller pops the arguments off the stack.

  * Syntax
    ```
      void _start(int argc, char *argv[]) {
        // main is a function call made by the entry method _start which never returns
        // calls follow the above 80x86 convention.
        exit (main (argc, argv));
      }
    ```
  * Kernel must put the arguments for the initial function on the stack before starting the user program
    - Place the words containing an argument each, at the top of the stack in any order (as addresses are used, as in the Example).
    - Push the address of each string plus a null pointer sentinel on the stack, right to left - these are elements of argv.
    - argv[argc] must be a null pointer, mandated by C standard. argv holds the address of argv[0].
    - argv[0] is at the lowest virtual address. Push argv and argc in the order, followed by a fake return address (even though entry
      function will never return).

  * Example
    ```
      Address        Name               Data           Type
      0xbffffffc     argv[3][...]       "bar\0"        char[4]
      0xbffffff8     argv[2][...]       "foo\0"        char[4]
      0xbffffff5     argv[1][...]       "-l\0"         char[3]
      0xbfffffed     argv[0][...]       "/bin/ls\0"    char[8]
      0xbfffffec     word-align         0              uint8_t
      0xbfffffe8     argv[4]            0              char *
      0xbfffffe4     argv[3]            0xbffffffc     char *
      0xbfffffe0     argv[2]            0xbffffff8     char *
      0xbfffffdc     argv[1]            0xbffffff5     char *
      0xbfffffd8     argv[0]            0xbfffffed     char *
      0xbfffffd4     argv               0xbfffffd8     char **
      0xbfffffd0     argc               4              int
      0xbfffffcc     return address     0              (void *) ()
    ```

  * System Call Workflows
    - OS already deals with external interrupts and internal exceptions.
    - User programs can request services (system calls) from OS via exceptions.
    - int is commonly used for invoking a system call (in 80x86) - instruction is handled the same way as any other software exceptions.
      - Arguments can also be pushed to the system calls by appending to the stack.
    - The interrupt frame is passed to the syscall handler, which can access the stack via "esp" member of the structure - from the stack,
      the syscall number and its arguments can be fetched (NOTE: syscall number is at the stack pointer, followed by arguments).
    - Syscalls that return a value can do so by modifying the "eax" member of the given interrupt frame structure.


## Virtual Memory Layout
  * User + Kernel
  * User virtual memory is per process.
    - When switching threads, virtual address space is also switched.
    - NOTE: floating point registers are not switched during thread switching, hence floating point operations are not supported.
    - Kernel's virtual memory starts from PHYS\_BASE, it's global, and can't be accessed by user programs - kernel can access all memory,
      except unmapped virtual addresses.
    - User stack can grow.
    - Size of uninitialised data segments can be changed using a system call - not in pintos.
    - Code segment (in pintos) starts at 128 MB (0x08048000) from the bottom.
    - Linker sets the layout of a user program in memory - conveys name and locations of various program segments.
    - Following is the output from objdump for kernel.o
      ```
        kernel.o:     file format elf32-i386

        Program Header:
            LOAD off    0x00000000 vaddr 0xc0020000 paddr 0xc001f000 align 2**12
                filesz 0x000000b4 memsz 0x000000b4 flags r--
            LOAD off    0x000000c0 vaddr 0xc00200c0 paddr 0xc00200c0 align 2**12
                filesz 0x00014f40 memsz 0x00014f40 flags r-x
            LOAD off    0x00015000 vaddr 0xc0035000 paddr 0xc0035000 align 2**12
                filesz 0x00004658 memsz 0x00006864 flags rw-
            STACK off    0x00000000 vaddr 0x00000000 paddr 0x00000000 align 2**4
                filesz 0x00000000 memsz 0x00000000 flags rwx
      ```

  * User Memory
    - Kernel accesses user memory through pointers given by user programs - however, user can pass a null pointer, an unmapped virtual
      address or a pointer to kernel virtual address space -> such requests are to be rejected, along with termination of the offending
      user program.
    - One way to do the above is to validate the pointer before dereferencing it.
    - Other way which is used in real kernels is to check if it points below kernel address space (PHYS\_BASE) before dereferencing - invalid
      pointers cause page fault which can be handled via MMU, making it faster.
    - While terminating such a process, all resources (locks, pages) must be freed.
    - When relying on page fault for termination, an error code is not returned and hence, freeing up resources is not straightforward -
      need to rely on assembly code to detect it.
    - argument passing -> memory access -> syscall infra -> exit -> wait

  * Argument Passing
    - Space separated arguments
    - Argument length - 4KB

  * Process Termination
    - Print the name of user processes when exiting.

  * Denying Writes
    - Don't allow writing to open files which are executables.
    - Can be allowed to write again once closed by everyone.

## System Calls
  * halt ()
    - Terminated pintos by calling shutdown\_power\_off -> information can be lost if terminated like this.
  * exit (status)
    - Terminates current user program and returns status to the parent thread, non-zero typically implies error.
  * pid\_t exec (const char \*cmdline)
    - Runs executable which is fetched from cmdline - args to the executable are also fetched from it.
    - Returns the pid for the new process - return -1 if program can't be run for any reason. Parent process can't exit exec until status
      is received.
  * int wait (pid\_t pid)
    - Waits for a child process with pid given in argument, and retrieves its exit status. If pid is alive and running, then its
      exit status is returned to parent. If pid was terminated by kernel, then return -1. If pid is already terminated, then its exit status,
      due to exit or kernel, should be passed to parent. Method returns -1 when -
    - pid doesn't refer to the direct child of caller -> a thread is a direct child of a process if the process received its pid value by
      calling the exec method.
    - A -> B -> C, then wait(C) by A should return -1. Also, if B dies before C, then C is not assigned a new parent.
    - wait can be called at most once (eg, like lock\_acquire) by a process for a pid.
    - Since processes can create any number of threads in any order, and wait for them in any order, there are many scenarios that need to
      be considered. Ensure pintos doesn't terminate before initial\_thread has terminated.
  * bool create (const char \*file, unsigned initial\_size)
    - Creates a new file with the given name and size, without opening it.
  * bool remove (const char \*file)
    - Removes the given file without closing it - if caller process has a file descriptor for it, it can continue to use it even
      after being removed (read/write), but file will have no name and no other processes can open it - file will continue to exist untill
      all file descriptors referring to the file are closed or system is terminated.
  * int open (const char \*file)
    - Opens the given file, returning a non-negative integer called file descriptor, or -1 if it can't be opened. fd 0 (STDIN\_FILENO)
      and fd 1 (STDOUT\_FILENO) are reserved for console input/output. Child processes don't inherit file descriptors. Every time open is
      called for a file, a different fd should be returned, and should be closed by different calls to close.
  * int filesize (int fd)
    - Returns the size of file in bytes for the file pointed to by the given fd.
  * int read (int fd, void \*buffer, unsigned size)
    - Reads size bytes into buffer from the open fd. fd 0 reads from keyboard.
    - Returns actual bytes read. Returns -1 if file couldn't be read for any reason except EOF.
  * int write (int fd, const void \*buffer, unsigned size)
    - Write size bytes from buffer into open file fd - returns actual bytes written.
    - Currently, extending the file's initial size is not implemented.
    - fd 1 writes to console - ensure buffer is broken in small pieces as required, else write at once.
  * void seek (int fd, unsigned position)
    - Changes the next byte to be read/written in fd to position bytes starting from the beginning of file.
    - Seeking past end of file is not error -> read () will return 0, and write can lead to file extension, filling gaps with zero.
  * unsigned tell (int fd)
    - Return position of next byte to be read/written in open file fd.
  * void close (int fd)
    - Closes the fd. Terminating a process should close all descriptors opened by the process.

  * System call number is stored in user stack in user virtual address space.
  * System calls should be synchronized so that any number of user processes can make them at once.
  * When system call handler gets control, system call number is in the 32-bit word at the caller's stack pointer, first argument is in the
    32-bit word at the next higher address and so on.
