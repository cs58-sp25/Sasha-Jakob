# Yalnix OS Project Design Document

## Division of Responsibilities
- **Sasha**: KernelStart, Virtual Memory, and Context Switching
- **Jakob**: Syscalls, Traps/Interrupts/Exceptions, and SetKernelBrk

## Project Structure

### Core Files

#### `kernel.h`
This file will contain shared declarations, constants, and data structures used throughout the kernel.

#### `kernel.c`
This file will contain the `KernelStart` function and main initialization code.

#### `memory.h` / `memory.c`
These files will handle virtual memory management, page table operations, and frame tracking.

#### `context_switch.h` / `context_switch.c`
These will contain the context switching implementation including `KCSwitch` and `KCCopy` functions.

#### `syscalls.h` / `syscalls.c`
These files will contain syscall handler implementations.

#### `traps.h` / `traps.c`
These will handle trap, interrupt, and exception processing.

#### `pcb.h` / `pcb.c`
Process Control Block implementation and process management.

## Data Structures and Pseudocode

### 1. Process Control Block (PCB)

```c
/* pcb.h */

typedef struct list_node {
    struct list_node *prev;
    stuckt list_node *next;
} list_node;

// We are going to need macros to get from the pointers in the struct to get back to the main struct
typedef struct pcb {
    UserContext *uc;
    KernelContext *kc;

    // pid of the process and the parent process
    int pid;
    int ppid; 

    // Where the page table is stored
    void *page_table

    // Used to handle delaying processes
    bool delaying;
    int delay;

    // Used to keep track of how long a process has been running if it is the current one
    int run_time;
    int time_slice;

    // Marks the process as a zombie and stores it's exit code
    bool is_zombie;
    int exit_code;

    // Relationships
    bool waiting_child;
    struct pcb *parent;
    struct list *children;

    // I/O
    void *tty_read_buffer;
    int tty_read_len;
    int tty_id;
    void *tty_write_buffer;
    int tty_write_len;

    // For pipe I/O
    void *pipe_read_buffer;
    int pipe_read_len;
    int pipe_read_actual;
    struct pipe *waiting_pipe;
    void *pipe_write_buffer;
    int pipe_write_len;

    // Synchronization
    struct lock *waiting_lock; // What lock the process is waiting on
    struct cvar *waiting_cvar; // What cvar the process is waiting on
    

    // Queues
    // These nodes are included so that when the process is added to queue, the 
    struct list_node ready_node;
    struct list_node wait_node;
    
} pcb_t;

// Process queues
extern list_node *ready_queue;
extern list_node *blocked_queue; // Mayhaps we separate this based on how the process is blocked, not sure quite yet
extern list_node *zombie_queue;
extern pcb_t *current_process;

// Process creation and management functions
pcb_t *create_pcb(void);
void destroy_pcb(pcb_t *process);
void add_to_ready_queue(pcb_t *process);
void remove_from_ready_queue(pcb_t *process);
pcb_t *schedule_next_process(void);
void terminate_process(pcb_t *process, int status);

void init_lists(void);
void add_to_list(list_node* list, pcb_t *proc);
```

### 2. Memory Management

For process memory management:
- Each process needs its own Region 1 page table
- Kernel text and data are shared across all processes
- Each process needs its own kernel stack
- Need to consider copy-on-write for Fork efficiency (optional)

```c
/* memory.h */

// Frame tracking
typedef struct {
    unsigned int *frame_bitmap;    // Bitmap to track free frames
    int num_frames;                // Total number of frames
} frame_tracker_t;

extern frame_tracker_t frame_tracker;
extern int vm_enabled;             // Flag to track if virtual memory is enabled

// Free frame management
int allocate_frame(void);          // Returns a free frame or -1 if none available
void free_frame(int frame_num);    // Mark a frame as free
void init_frame_tracker(int pmem_size);  // Initialize frame tracker

// Page table management
void init_region0_page_table(void);  // Set up initial Region 0 page table
void init_region1_page_table(struct pte *pt);  // Initialize a Region 1 page table
void map_page(struct pte *pt, int vpn, int pfn, int prot);  // Map a virtual page to a physical frame
void unmap_page(struct pte *pt, int vpn);  // Unmap a virtual page

// Virtual memory functions
int handle_memory_trap(UserContext *uctxt);  // Handle TRAP_MEMORY exceptions
```

## 3. Sasha's Responsibilities

#### KernelStart

```c
/* kernel.c */

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
    /*
    1. Initialize frame tracker based on pmem_size
    2. Set up interrupt vector table
    3. Create initial Region 0 page table
    4. Set kernel stack frames
    5. Set up kernel break
    6. Enable virtual memory
    7. Create idle process PCB
    8. Create init process PCB
    9. Set up idle process's user context
    10. Load init program and set its user context
    11. Set current_process to init
    12. Return to user mode
    */

   // Also run init_lists somewhere
}
```

#### Virtual Memory

```c
/* memory.c */

void init_frame_tracker(int pmem_size) {
    /*
    1. Calculate the number of frames based on pmem_size
    2. Allocate bitmap for tracking frames
    3. Mark all frames as free initially
    4. Mark frames used by kernel text/data/stack as used
    */
}

void init_region0_page_table(void) {
    /*
    1. Allocate page table for Region 0
    2. Set up mappings for kernel text, data, and stack
    3. Set REG_PTBR0 and REG_PTLR0
    */
}

int handle_memory_trap(UserContext *uctxt) {
    /*
    1. Check if memory fault is in user stack region
    2. If yes, allocate a new frame and map it
    3. If no, abort the process
    */
}
```

#### Context Switching

```c
/* context_switch.c */

KernelContext* KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    /*
    1. Save current kernel context to current PCB
    2. Change kernel stack mappings to next process's stack
    3. Return pointer to next process's kernel context
    */
}

KernelContext* KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used) {
    /*
    1. Copy current kernel context to new PCB
    2. Copy kernel stack contents to new kernel stack frames
    3. Return original kernel context
    */
}
```

## 4. Jakob's Responsibilities

#### SetKernelBrk

```c
/* kernel.c */

int SetKernelBrk(void *addr) {
    // Check if virtual memory is on
        // If not just update the kernel brk and grab pages
    // If yes
        // Allocate new pages
        // map pages
        // Add to the page table
    return 0;
}
```

#### Syscalls

```c
/* syscalls.h */

// A table to handle syscalls
void (*syscall_handlers[])(UserContext *);

// Process Coordination
void SysFork(UserContext *uctxt);
void SysExec(UserContext *uctxt);
void SysExit(UserContext *uctxt);
void SysWait(UserContext *uctxt);
void SysGetPID(UserContext *uctxt);
void SysBrk(UserContext *uctxt);
void SysDelay(UserContext *uctxt);

// I/Os
void SysTtyRead(UserContext *uctxt);
void SysTtyWrite(UserContext *uctxt);

// IPC
void SysPipeInit(UserContext *uctxt);
void SysPipeRead(UserContext *uctxt);
void SysPipeWrite(UserContext *uctxt);

// Syncing
void SysLockInit(UserContext *uctxt);
void SysLockAcquire(UserContext *uctxt);
void SysLockRelease(UserContext *uctxt);
void SysCvarInit(UserContext *uctxt);
void SysSignal(UserContext *uctxt);
void SysBroadcast(UserContext *uctxt);
void SysCvarWait(UserContext *uctxt);
void SysReclaim(UserContext *uctxt);

// Implement other syscall handlers like sempahores perhaps
```

```c
/* sync.h */
// Struct for supporting pipe operations
struct pipe_t;
// Struct for supporting lock operations
struct lock_t;
// Struct for supporting cvar operations
struct cvar_t;

// Used to determine what a syncing struct is
typedef enum {
    PIPE,
    LOCK,
    CVAR
} sync_type_t;

// A structure used to store either a pipe, a lock, or a cvar
struct sync_obj_t;

extern sync_obj_t *sync_table[MAX_SYNCS];  // Stores all sync objects (pipes, locks, cvars)
extern int global_sync_counter;  // Counts the number of sync objects

// used to init a Sync Object and store it
static int InitSyncObject(sync_type_t type, void *object, int *idp);

// Pipe related functions
int InitPipe(int *pipe_idp)
void ShiftPipeBuffer(pipe_t *pipe, int len)
int ReadPipe(int pipe_id, void *buf, int len, pcb_t *curr);
void UnblockOneReader(pipe_t *pipe);
int WritePipe(int pipe_id, void *buf, int len);

// Lock related functions
int InitLock(int *lock_idp);
int LockAcquire(int lock_id);
int LockRelease(int lock_id);

// Cvar related functions
int InitCvar(int *cvar_idp);
int CvarSignal(int cvar_id);
int CvarBroadcast(int cvar_id);
int CvarWait(int cvar_id, int lock_id)

// Reclaims any sync variables
int ReclaimSync(int id);
```

#### Traps/Interrupts/Exceptions

```c
/* traps.h */
// Trap handler for system calls (TRAP_KERNEL)
void kernel_handler(UserContext *cont);

// Trap handler for clock interrupts (TRAP_CLOCK)
void clock_handler(UserContext *cont);

// Trap handler for illegal instruction exceptions (TRAP_ILLEGAL)
void illegal_handler(UserContext *cont);

// Trap handler for memory access violations (TRAP_MEMORY)
void memory_handler(UserContext *cont);

// Trap handler for math errors (TRAP_MATH)
void math_handler(UserContext *cont);

// Trap handler for terminal input (TRAP_TTY_RECEIVE)
void receive_handler(UserContext *cont);

// Trap handler for terminal output complete (TRAP_TTY_TRANSMIT)
void transmit_handler(UserContext *cont);
```


## Key Questions

1. How will we track free frames?
2. How will the kernel stack be managed during context switches?
3. How will we handle process termination and zombie cleanup?
4. How will terminal I/O be managed when a process is blocked?
5. What data structures are needed for synchronization primitives?