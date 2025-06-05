## Reqs
Boot 
- Invokes KernelStart() to boot the system (1)
- Set up a way to track free frames
- Set up the region 0 page table, We are given 3 values
	- first_kernel_text_page - the lowest page used by the kernel text
	- first_kernel_data_page - easy to extrapolate
	- first_kernel_brk_page - First page after the kernel heap on boot
- Set up a region 1 page table for idle (1 valid page for user stack)
- Set the page table registers
- Create a set of PTEs and write the page table base and limit registers to tell where the PTs are located in memory
- Create IVT, initialize corresponding entry with a pointer to the relevant handler function.
- Store addr in REG_VECTOR_BASE
- WriteRegister(REG VM ENABLE, 1) to enable vmem
- When func returns machines runs in user mode at specified used context (2)
Kernel Heap
- Only req is writing SetKernelBrk() (3)
Machine Instructions
- Kernel code will call special machine instructions for things like changing the page table pointers (4)
Syscalls, Interrupts, Exceptions
- Determine which trap handler to invoke (5)
- Write the trap handlers
	- Each one takes in a UserContext (UC) and outputs void
	- See IET Handling
- Write the syscalls executed by these trap handlers (?)
- Hardware returns to usermode after trap handler (6)
TLB Misses
- If userland/kernelland touch an address not in TLB, hardware handles the miss (7)
Kernel Context Siwtching
- Write special kernel context switching functions which are invoked by our kernel code through preprovided stuff
- KCCopy (for forks) 
- KCSwitch (for switching between processes)
	- Switch the Kernel stack and Region 1 for the process
- See the Kernel Switching section for details
- These are surrounded by (8, 9)
MMU
- I honestly have no idea
Other
- Write a PCB structure which stores
	- kernel context
	- user context
	- pointer to the page table perhaps?

![[Pasted image 20250430112029.png]]

## Hardware Spec
Yalnix includes a number of registers
### General Registers
 - PC - program counter, vaddr from which currently execution instruction was fetched
 - SP - stack pointer, contains the vaddr of top of current proc's stack
 - EBP - base pointer, contains the vaddr of the current proc's stack frame (bottom of stack)
 - R0,...,R7 - general registers for accumulating/storing values

### Privileged Registers
All privilaged registers have a privilaged read & write function provided by hardware
![[Pasted image 20250430112622.png]]

void WriteRegister(int which, uint value) - write to designated reg
uint ReadRegister(int which) - read from designated reg

### Memory System
We have an MMU and each process has a page table which handles the memory mapping of a process's virtmem to phymem, a number of constants are defined for dealing with addrs and page numbers

#### Constants and Macros
Defined in hardware.h
- PAGESIZE - bitsize of pages
- PAGEOFFSET - addr & PAGEOFFSET is the byte offset within the page where addr points
- PAGEMASK - addr & PAGEMASK gives the first address addr is in
- PAGESHIFT - log_2(PAGESIZE), can turn page numbers into an addr and vice versa
- UP_TO_PAGE(addr) Returns next highest page above addr, or addr if it is the 0th address of a page
- DOWN_TO_PAGE(addr) Returns the next lowest page boundary, or if addr is on a page boundary returns addr

### Physical Memory
- PMEM_BASE is the first byte of physical memory on the machine, constant and determined by hardware design, defined in hardware.h
- Total size is determined by RAM installed on the machine, tested by firmware on boot and is passed to OS kernel during init proc

### vaddr Space
vaddr is separated into region 0 (kernel space mem) and region 1 (user space mem). This is definde by
- VMEM_0_BASE - lowest addr of Region 0
- VMEM_0_LIMIT - the first byte above region 0
- VMEM_0_SIZE - the size of region 0
- Replace 0s with 1s to get the limits for region 1
- VMEM_1_LIMIT == VMEM_0_BASE
- VMEM_BASE and VMEM_LIMIT are the base and limit of all vmem (equal to VMEM_0_BASE and VMEM_1_LIMIT respectively)
- The state of the kernel in region 0 consists of the kernel code and global variables, this state is the same across all user-level processes executing. They kernel stack holds the local kernel state associate with the user-level process. On context switching, Kernel Stack and all of Region 1 must be switched

Kernel mode can reference Region 1 or Region 0, user mode can only reference region 1

### Page Tables
Hardware uses single-level page tables to define mapping from vaddrs to paddrs. Translation happens without intervention from the kernel but the kernel does control the mapping that specifies translation.

The kernel allocates page tables in mem wherever and tells the MMU where to find the page tables through
- REG_PTBR0 - contains vmem base addr of the page table for region 0 of vmem
- REG_PLTR0: The number of virtual pages in region 0
- replace 0 with 1 for region 1
- When the CPU references a vpn, the MMU find the pfn by checking the page table.
- Page table is indexed by vpn and contain entries specifying the pfn and other things for each vpn in the region

vpns are 32 bits (but do not use all of them) and contain
- valid(1b) - if set, valid. otherwise, mem exception generate if page is accesed
- prot(3 bits) - define mem protection applied by hardware to the page, to exec both prot_read, prot_exec must be valid
	- PROT_READ
	- PROT_WRITE
	- PROT_EXEC
- pfn(24 bits) - Contains the pfn that the vpn maps to, ignored if the valid bit is off.
- The pte format is defined in hardware.h as a C data structure as pte_t

### TLB
Page table entries are loaded atuomatically into the TLB by the hardware during translation. The OS can not directly look at the cache but must on occasion flush part or all of the TLB. REG_TLB_FLUSH is a privileged machine reg to allow the os kernel to control the flushing using values defined in hardware.h
- TLB_FLUSH_ALL: flush all entries in the TLB
- TLB_FLUSH_1: flush all entries in region 1
- TLB_FLUSH_KSTACK: flush kernel stack entires in the TLB
- TLB_FLUSH_0: flush all entries in region 0
	- vaddrs that correspond to the text, data, and initial heap when vmem is enable are not flushed. the page table for Region 0 can go here.
	- Flushing the kernel stack from the region 0 TLB causes the kernel stack to be siltently remapped, according to the current page table. When changing region 0 contexts, change the page table first, then flush.
- Otherwise the value is interpreted as a vaddr, if an entry exists in the TLB corresponding to a vmem page that addr points, flush this entry from the TLB, if the entry dne, nothing happens

### Initializing VMEM
On boot, ROM firmware loads the kernel into physical memory and begins execution. Kernel is loaded into contiguous pmem starting at addr PMEM_BASE. vmem is disabled until explicitly enabled by the OS kernel. An example of initializing vmem is
- Create a set of PTEs and write the page table base and limit registers to tell where the PTs are located in memory
- Until vmem is enabled, all addrs are interpreted as physical addrs
- after vmem is enabled, all addrs used are interpreted as vaddrs

The privileged machine register REG_VM_ENABLE says if VMEM is enabled or not
- WriteRegister(REG_VM_ENABLE, 1) enables vmem

VMEM cannot be disabled after enabled without rebooting
- This means that all previous addrs which are interpreted as paddrs are suddenly interpreted as paddrs, this can cause issues so initially PTEs are set up so that vaddr == paddr or in other words pfn = vpn for all kernel entries
![[Pasted image 20250430133644.png]]

### Hardware Clock
The OS includes a hardware clock that, when a clock interrupt occurs, generates a TRAP_CLOCK, the frequency of clock interrupts can be set on invoke. These interrupts are used for any timing needs of the OS, might be used as the source for counting down the quantum in CPU process scheduling

### Terminals
Equipped with NUM_TERMINALS terminals numbered starting at 0 for use by user processes. Terminal 0 is the system console, while other terminal are general purpose terminal devices. The kernel interacts with the terminals via
- Two machine instructions
	- void TtyTransmit(int tty id, void \*buf, int len)
	- int TtyReceive(int tty id, void \*buf, int len)
- Two traps
	- TRAP_TTY_TRANSMIT
	- TRAP_TTY_RECEIVE

TtyTransmit starts a transimission of len characters from mem, starting at addr buf, to terminal tty_id

TtyTransmit returns immediately but the operation will take some time to complete. buf must be in kernel memory so it is available during transmission. 

When the data has been written out the terminal controller generates a transmit trap and the terminal number becomes availabe to the kernel's intterup handler. No new transmit can start until the last transmit output's it's trap.

![[Pasted image 20250430134553.png]]

When users provide input, the recieve trap is dropped and the kernel should execute TtyReceive. The data is copied from terminal tty_id to buf of length len. The acutla length of the input is returned from TtyReceive

![[Pasted image 20250430134754.png]]

TERMINAL_MAX_LINE in hardware.h is the maximum buffer size supported, the user must break it up so no more than TERMINAL_MAX_LINE bytes get set at once.

### Interupts, Exceptions, and Traps (IET)
IETs are all handled similarly by the hardware, switches into kernel mode, calls a handler in the kernel by indexing into the interrupt vector table  (IVT) based on the type of trap. The DCS 58 hardware defines
- TRAP_KERNEL - results from a syscall trap instruction executed by the current user processes. All kernel call requests enter the kernel through this trap
- TRAP_CLOCK - comes from the hardware clock
- TRAP_ILLEGAL - Exception results from execution of an illegal instruction (undefined machine language opcode, illegal addr mode, privledged instruction in user mode)
- TRAP_MEMORY - Results from disallowed mem access by user process. Can happen because; Accessed memory is outside region 0 and 1, addr is not mapped in the current page tables, access violates the page protection in the PTE. Handler must figure out what to do based on the address. 
	- Code filed in UC contains YALNIX_MAPERR (addr not mapped) and YALNIX_ACCER (invalid permissions)
- TRAP_MATH - arithmetic error from user such as div by 0 or arithmetic overflow
- TRAP_TTY_RECEIVE - Generated by the terminal device controller hardware when a complete line of input is available from one of the terminals
- TRAP_TTY_TRANSMIT - Generated by the terminal device controller hardware when the current buffer of data sent by TtyTransmit has been completely sent to the terminal
- TRAP_DISK - Generated by the disk when it completes an operation

The IVT is an array of function pointers which handle the related trap. The above names are symbolic constants in hardware.h

Each of the functions are given a pointer to a UC and return void

The privileged machine register REG_VECTOR_BASE is the base addr in memory where the interrupt vector table is stored. After initializing the table in memory, REG_VECTOR_BASE must be written to with the addr.

The IVT must have exactly TRAP_VECTOR_SIZE entries in it, even if the use is undefined by the hardware. 

Allegedly mentioned in class, the kernel is not interruptibile

#### User Context
UC contains the hardware state of the running user process, on a trap, a copy is created and passed to the trap handler, when the trap handler returns, the hardware restores whatever UC is at that address and resumes execution in user mode. Since kernel may switch to a different userland process while handling a trap, keep a full copy of the UC in the process control block (PCB)

UC struct is defined in hardware.h with fields
- int vector - type of IET. index into IVT
- int code - A code value with more info on the IET. Values for code can be found in {}
- void \*addr - only meaningful for a TRAP_MEMORY
- void \*pc - Program counter at time of IET
- void \*sp - Stack pointer at time of IET
- void \*ebp - Bae pointer at time of IET
- u_long regs\[8\] - Contents of the general purpose CPU registers at time of IET


| Vector       | Code                              |
| ------------ | --------------------------------- |
| KERNEL       | kernel call number                |
| CLOCK        | Undefined                         |
| ILLEGAL      | Type of illegal instruct          |
| MEMORY       | Type of disallowed mem access     |
| MATH         | Type of math error                |
| TTY_TRANSMIT | Terminal number causing interrupt |
| TTY_RECEIVE  | Terminal number causing interrupt |
| DISK         | Undefined                         |
To context switch between uprocs, The kernel must copy the running proc's UC into some kernel data stucture, select another process to run, and restore that process' previously stored context by copying that data to the address (i.e. the User Context pointer) passed into the trap handler. The hardware handles extracting and restoring the hardware state info from the trap handler's UC argument.

The privileged machine registers are not included in the User Context structure. The values are associated with the process by the kernel, not by the hardware, and must be changed by kernel on context switch if needed.

### Other CPU instructions
In addition to the hardware features the CPU provides Two other instructions for special purposes
- void Pause(void) stops the CPU until the next interrupt occurs. Idle process in your kernel should be a loop that executes the Pause instruction in each loop iteration
- void Halt(void) Stops the CPU, like pausing the state of the OS

## The OS Spec
### Syscalls
yuser.h provides function prototypes for the interface for all syscalls, as well as the syscall constant for each syscall type (see UC). If nothing is written next to the syscall assume same as normal.

#### Proc Coord
- int Fork(void)
- int Exec(char \*filename, char \*\*argvec)
- void Exit(int status)
- int Wait(int \*status_ptr)
- int GetPid(void)
- int Brk(void \*addr)
- int Delay(int clock_ticks) - blocked until clock_ticks clock interrupts have occured after the call, error occurs with negative clock_ticks

#### I/O
- int TtyRead(int tty_id, void \*buf, int len)
- int TtyWrite(int tty_id, void \*buf, int len)
- int TtyPrintf(int, char \*, ...) - like printf

#### IPC
- int PipeInit(int \*pipe_idp)
- int PipeRead(int pipe_id, void \*buf, int len)
- int PipeWrite(int pipe_id, void \*buf, int len)

#### Sync

- int LockInit(int \*lock_idp)
- int Acquire(int lock_id)
- int Release(int lock_id)
- int CvarInit(int \*cvar_idp)
- int CvarSignal(int cvar_id)
- int CvarBroadcast(int cvar_id)
- int CvarWait(int cvar_id, int lock_id)
- int Reclaim(int id) - Destroy pipe, lock, or cvar

See the doc for semaphores

### IET handling
- TRAP_KERNEL - Execute the requested syscall based on code field of UC, arguments of syscall are found in registers starting with regs\[0\], return of syscall goes in regs\[0\]
- TRAP_CLOCK - If other procs are on ready queue, context switch to next runnable proc
- TRAP_ILLEGAL - Abort, allow others to continue
- TRAP_MEMORY - Determine if this exepction is a request to enlarge the tack, if so do that and return (see below), otherwise abort, allow others to continue
- TRAP_MATH - Abort, allow others to continue
- TRAP_TTY_RECEIVE - New line of input is available from terminal, kernel reads input from terminal w/ TtyReceive, and if necessary, buffer the input line for a subsequent TtyRead
- TRAP_TTY_TRANSMIT - TtyTransmit operation is completed, specific terminal is indacted by code field in the UC, kernel completes blocked process that started this terminal output, ass necessary; also start the next terminal output on this terminal, if any
- TRAP_DISK - OS ignores these traps w/o extra functionality involving disk

### Syscall Library
Reread this later 3.3

### Process Death
When the kernel aborts a process, kernel should TracePrintf a message at level 0, giving pid of process, and explanation of the problem. The exit status reported to the parent process of the aborted process when the parent calls Wait should be the value ERROR

### Memory Management
Kernel is responsible for all aspects of memory management, both for user processes and kernel's own use of memory

### Initializing vmem
See checkpoint 2

### User Memory Management
Heap - User code links to malloc and friends, kernel should not let the heap shrink below the original user data pges or grow into the user stack

Stack - When a process pushes to the stack the stack pointer is decremented and the value is written to the stack. If the stack pointer goes lower than it has been before in the process a TRAP_MEMORY is triggered which as stated before should be an implicit request to grow the process's stack. If the new stack address is: in region 1, below the currently allocated memory for the stack, and is above the current brk. The kernel should grow the stack if possible, otherwise abort that process.

Red Zone - Leave at least one page unmapped (valid bit zeroed) between the heap and the stack so the stack will not grow into and overlap with the heap without triggering a TRAP_MEMORY.

### Kernel Memory Management
Heap - SetKernelBrk is invoked through the kernelland malloc library when it needs to adjust the kernel heap.

Stack - The kernel has a stack for each running process separate from the userland stack. The kernel stack for each process is of fixed max size KERNEL_STACK_MAXSIZE bytes. The kernel stack if located at exactly the same memory address in virtual memory for all processes. The bounds for the kernel stack are KERNEL_STACK_BASE, and KERNEL_STACK_LIMIT which is the first byte beyond the top of region 0 (same as VMEM_LIMIT_0).

Red Zone - helpful to have between kernel stack and heap as well

## Kernel Context Switching
Switching between processes happens in kernel mode. proc A (in kernel mode) switches to proc B (still in kernel mode). The stack, the contents of the pc, sp, and other registers also change (except priviliged)
![[Pasted image 20250501104211.png]]

To switch to process B, process A must restore the physical frame numbers containing process B's kernel stack, and the kernel context that should be restored when B starts running again.

Each process has 2 contexts, a UC and a KernelContext (KC) and the PCB should have a place to store the full structures.

### How it works
int KernelContextSwitch(KCSFunc_t \*, void \*, void \*) (KCS)
This function is provided and it takes a KC pointer and two void pointers and returns a KC pointer. (all of this is defined in hardware.h)

This function temporarily stops using the standard kernel context and calls the function stored at the function pointer you gave it using three args
- A pointer to the kernel context of the caller
- The two void pointer arguments
KCS needs to perform two distinct tasks
- Switching from one process to another.
- cloning a new process in the first place.

Recomended to do these task in separate functions
![[Pasted image 20250501104923.png]]

An example of how the function could look that you pass into KCS

KernelContext \*KCSwitch(KernelContext \*kc_in,
					void \*curr_pcb_p,
					void \*next_pcb_p)

and calling KCS would look like

rc = KernelContextSwitch(KCSwitch,
					(void \*) &curr_pcb_p,
					(void \*) &next_pcb_p)

KCSwitch might copy the bytes of the kernel context into the curr proc's PCB and return a pointer to a kernel context it has saved in the next proc's PCB. The flow for this if A switches to B and B later switches back to A may look like
1. A calls KCS
2. KCSwitch is executed with the arguments from A by KCS
3. B starts running
4. B calls KCS
5. A resumes running as if it returned from the KCS call in step 1

KCS returns 0 on succes, if an error occurs and the switch does not occur, it returns -1 and should exit since it generally indicates a programming error in kernel.

KCS does not move a PCV from one queue to another in kernel or do bookkeeping as to why the context switched or when it can be switched again. KCS helps with the saving of the process state and restoring it.

KC is a data structure provided by hardware.h and what is in it does not particularly matter, just copy it into the PCB

If process B is a new process, it must have a kernel already so KCCopy must be written
KernelContext \*KCCopy(Kernel Context \*kc_in
					void \*new_pcb_p
					void * ot_used)

KCCopy simply copies the kernel context from \*kc_in into the new pcb, and copies the contents of the current kernel stack into the frames allocated for the new process's kernel stack. 