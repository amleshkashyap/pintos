## Data Structures

## Functions
  * ptov - Returns the (kernel) virtual address at which the given physical address is mapped.
  * vtop - Returns the physical address at which the given (kernel) virtual address is mapped.
    - This and above method is used directly for kernel address translations since kernel addresses are 1:1 mapped.
    - Once the page to frame translation is done in kernel address space, user addresses can be validated to be below PHYS\_BASE.

  * pd\_no - Obtains the index for a PDE from a given virtual address - result is passed to the below method to get the page table address.
  * pde\_get\_pt - Gets the page table address for a given virtual address.
  * pde\_create - Gets a PDE that points to a given page table.
  * pagedir\_create - Creates a new page directory that has mappings for kernel virtual addresses (init\_page\_dir). This method is used
    to create and set a new page directory for a thread.

  * pt\_no - Obtains the index for a page table from a given virtual address - result is passed to the below method to get the physical
    base address of the actual frame.
  * pte\_get\_page - Returns the 20 bits of physical base address to be added to 12 bits of page offset to obtain the frame

  * palloc\_get\_multiple
    - Gets page\_cnt number of pages. Acquires the lock and flips the bitmask first, and releases the lock. Then returns the required pages.
    - Checks for PAL\_USER and PAL\_ZERO to fetch pages from user/kernel page pool, and set them to zero respectively.
  * palloc\_get\_page - Calls the above method to acquire 4kB of memory.
    - Usually, structures like a page table and page directory are fit in 4kB
  * lookup\_page
    - Checks if a page to frame mapping for a given virtual address exists -> it may allocate the page if it doesn't exist using the above
      method.
    - Its workflow is given below.
  * pagedir\_get\_page - Calls the above method to get the PTE for a vaddr, and adds the offset to it to get the physical address for vaddr.
    - Existence of a PTE (via lookup\_page) ensures that 4kB of memory must be present from the base - it just adds the offset.

  * bool pagedir\_set\_page (uint32\_t \*pd, void \*upage, void \*kpage, bool writable)
    - Gets the PTE for a vaddr given by `upage` - if page table is not present, it'll be created (given in Workflow).
    - A newly created page table will lead to a PTE (wrt vaddr) that is set to all zero's - PTE needs to be filled with values so that next
      time when the vaddr is encountered, its actual physical memory location can be found - actual memory location is `kpage` which is
      set to the address of the PTE - this kpage should contain physical memory allocated from user pool.
    - Whether the page can be written is set by the last argument.


## Workflows
  * Example of writing to a physical memory and accessing it via virtual memory address (see `setup_stack` method)
    - Obtain 4kB of physical memory from user memory pool (`palloc_get_page` with `PAL_USER` flag)
    - Create a PTE for this 4kB of memory so that this memory can be accessed using virtual addressing (`pagedir_set_page`)
    - Setup the stack pointer (`esp`) to the top of the virtual memory (`PHYS_BASE`)
    - Start writing to the stack pointer - when reading the address, it'll show the virtual address, but the data gets written to the 4kB of
      memory allocated from user pool and it's identified from the vaddr - keep pushing down the virtual address space (till page boundary
      is encountered). Set the `esp` to this location so that it can move back up to access the pushed data.
    - Note that currently, the stack space for pintos is just 4kB/1 page.

  * Finding the frame/physical address for a given page/virtual address (vaddr) - `lookup_page` and `pagedir_get_page`
    ```
    (vaddr)
    -> get the PDE for vaddr from process' pagedir (pd_no)
      -> if PDE is NULL, return NULL as the virtual address holds no data as of now.
         -> get the address of page table for the PDE (pde_get_pt)
            -> get the PTE for vaddr from above page table (pt_no)
               -> If the above PTE holds a value of zero (or has PTE_P = 0) then return NULL as the PTE is not set yet, vaddr holds no data
                  -> get the base physical address (pte_get_page) and add offset obtained from vaddr (pg_ofs)
    ```

  * Obtaining 4kB of physical memory for the user so as to write data to some virtual memory
    - Say the user program is writing some data to memory, but it finds out that the current page is about to run out of space - hence,
      it must obtain additional page - whether it should be immediately after the current page boundary or not is upto user.
    - The PTE created for this additional page will depend on what is the start virtual address to which the user wants to write to - hence,
      this workflow also must start with a vaddr, similar to above workflow.

    ```
    (vaddr, kpage)
    -> get the PDE for vaddr from process' pagedir (pd_no)
      -> if PDE is null, allocate 4kB for a page table, and create a PDE for this table (pde_create)
         -> get the address of page table for the PDE (pde_get_pt)
            -> get the PTE for vaddr from above page table (pt_no)
               -> Ensure that the above PTE holds a value of zero (ie, not written already)
                  -> Set the PTE obtained above to the address of given 4kB page (kpage)
    ```

  * All the memory allocations done in kernel mode (eg, frame allocations during system call) during the execution of a user program via
    malloc will fetch the space from kernel pool and no paging is in place.
    - However, memory allocations are also done using palloc\_get\_page in kernel mode (during system calls) in which case above is not true.

## Frame And Page Table Discussion
  * New Page
    - If user pool has pages left, then palloc will return a page, else it'll return NULL.
    - If PTE stores frame address, it has to be written.
    - If PTE stores frame slot number, it has to be written.

  * Fetch A Page In Physical Memory
    - If PTE stores the frame address, then it's a one step operation.
    - If PTE stores the frame slot number, then it must be used to prepare the actual FTE address (it maybe across pages), verify the pid
      and then return the address.

  * Write To Page In Physical Memory
    - If PTE stores the frame address, then one step operation.

  * Free A Page In Physical Memory

  * Lookup (For Fetch/Write) A Page Being Swapped In/Out

## Synchronization

## Memory
  * Memory Pools
    - Pintos memory (~4GB) is divided in user pool and kernel pool - both with same amoutn of RAM - usually, kernel pool will have much
      less RAM allocation.
    - Exclusive pool for kernel helps maintaining the performance for critical functionality like scheduling and system calls, as user
      processes might have larger and varying memory requirements, and maybe swapped more frequently.

  * Page
    - Contiguous block of `virtual` memory of fixed size. Should be aligned (ie, start address should be divisible by the fixed size).
    - With an assumption of 4096 bytes per page, 12 bits are required for offset, and with a 32-bit addressing system, 20 bits remain
      for the total virtual pages. Also, user virtual memory must end at a fixed point (PHYS\_BASE) so that there's no conflict with the
      kernel memory.
  * Frames
    - Contiguous block of `physical` memory. Other properties, eg, frame number and offset are same as page.
    - 80x86 doesn't provide a way to directly access memory at physical address -> in pintos, kernel virtual memory is mapped to the
      physical memory, eg, first page of virtual memory mapped to first frame of physical memory, second page to second frame, etc.

  * Page Tables - these hold the mapping for a page to frame, and used to translate virtual address to physical address.
  * Swap Slots - Continuous, page size region of disk space in swap partition. Ideally, they should also be page aligned.

  * Non Pageable Memory - This set of memory is guaranteed to exist in the physical memory, and they're not permitted to be swapped out
    to the swap partition. Usually consists of critical kernel code. Below DS can reside in such memory.
    - Managing large amount of resources (eg, pages, or even fd) can be performed using data structures ranging from array, lists, hash,
      bitmaps to red-black trees.

  * Supplemental Page Table (SPT)
    - Holds additional data about each page. At least 2 uses.
    - On a page fault, kernel looks up the virtual page that faulted in the SPT to find out the data that should be there.
    - Also, at process termination, SPT is looked up for the resources to be freed.
    - Used for page faults -> they shouldn't err, and should lead to searching for it in swap space.

  * Frame Table
    - One entry for each frame that contains a user page, holding a pointer to the page.
    - When no frames are free, an existing entry (ie, frame <-> page) must be evicted - eviction policy should use this data.
    - When a frame is not free, and swap slot is also full, pintos will panic -> real OSes don't, and they use a variety of other policies
      to handle such a situation (using more of filesystem?).

  * Swap Table
    - Tracks swap slots - providing an empty slot for a page that is being removed from its frame, freeing swap slot when a page is read,
      and freeing it when the process is terminated.

  * Table Of File Mappings
    - A file can be accessed using system calls like read/write - it can also be mapped to memory too using mmap. Eg, a 4kB file can occupy
      one page of virtual memory - sequential memory accesses is equivalent to reading the bytes of file.
    - Usecases?


## Page Table
  * See

## Stack, Page Faults And Frame Tables
  * Page Fault Handling
    - Locate the page that faulted in SPT - if found, locate the data held in the page, which maybe in swap space, in the filesystem,
      possibly in another frame (if aliasing is supported) or simply a zeroed page.
    - It's possible that the vaddr is invalid - eg, it's a kernel vaddr, or is an attempt to write to a read-only page - then terminate
      the user process, freeing all its resources.
    - Obtain a frame to store the page.
    - Fetch the data into the frame from swap/filesystem, etc (nothing to be fetched in case of an aliased frame).
    - Map the PTE for faulted address to this frame.

  * Frame Table Management
    - Each entry (say Frame Table Entry [FTE]) contains a pointer to page and any other required data - it's used for implementing efficient
      page eviction policy.
    - Frames for user pages should be from user pool (as given earlier).
    - Page Eviction
      - Choose a frame to evict (ie, Page Replacement Algo, use accessed/dirty bit)
      - Remove references to the frame from any page table (eg, aliased frames will have multiple PTEs)
      - Write the page to swap/filesystem if required

  * Stack Growth
    - Distinguishing stack accesses from other accesses so that if required, additional pages can be allocated for stack.
    - User programs shouldn't write to stack below the stack pointer (`esp`) - real OSes will interrupt a process at any time to deliver
      a signal which would push data to stack (however, x86-64 System V ABI is an exception - it designates 128 bytes below the stack pointer
      as red zone which may not be modified by signal/interrupt handlers) - some scenarios (?) can cause page faults as certain instructions
      check for access permissions before adjusting stack pointers.
    - static void page\_fault (struct intr\_frame \*f)
      - At entry, the faulted address is in Control Register 2 (CR2)
      - 
    - Processor only saves the stack pointer when an exception causes a switch from user to kernel mode (eg, system call) - hence the
      interrupt frame passed to the above method won't have the correct esp if there's a page fault in kernel mode.
    - Real OSes usually have an upper limit on stack size (eg, 8MB in linux). All stack pages can be evicted (and written to swap).

## System Calls
  * mapid\_t mmap (int fd, void \*addr)
    - Maps the file given by fd into process' virtual address space. Entire file should be mapped starting from virtual address addr.
    - Pages should be loaded in a lazy fashion and evicting a page mapped by mmap should write it back to the file it was mapped from.
    - If file length is not a multiple of page size, make the remaining bytes zero in memory, and discard these zeroes when swapping out.
    - mmap fails if file is empty, addr is not aligned, mapping needs multiple pages and overlaps with other mapping, addr is 0, fd is 0/1.

  * void munmap (mapid\_t mapping)
    - Unmaps the mapping given by the mapping ID passed in arg - it should be a valid ID (same process).
