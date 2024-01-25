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
    - Argument length

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
    - Avoid writing repeating code for implementations - fetching of arguments can be isolated.
