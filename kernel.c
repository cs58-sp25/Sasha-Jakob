/**
 * Date: 5/4/25
 * File: kernel.c
 * Description: Kernel implementation for Yalnix OS
 */


#include "hardware.h" 
#include "yalnix.h"
#include "ctype.h"
#include "load_info.h"
#include "ykernel.h"
#include "yuser.h"
#include "ylib.h"
#include "pcb.h"
#include "kernel.h"
#include "memory.h"
#include "syscalls.h"
#include "traps.h"
#include "list.h"
#include "sync.h"


/* Global variables for kernel state */
pcb_t *current_process;  // Currently running process


void main(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
    // Zero-initialize global variables to prevent undefined behavior
    current_process = NULL;
    

    // Print debug information about memory layout to help with debugging
    TracePrintf(0, "KernelStart: text start = 0x%p\n", _first_kernel_text_page);
    TracePrintf(0, "KernelStart: data start = 0x%p\n", _first_kernel_data_page);
    TracePrintf(0, "KernelStart: brk start = 0x%p\n", _orig_kernel_brk_page);

    // Initialize the interrupt vector table to handle traps and interrupts
    // This sets up functions to be called when different events occur
    init_interrupt_vector();

    // Initialize page tables for Region 0 and 1
    // This function also sets the kernel break address
    init_page_table((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page);


    // Enable virtual memory - this is a critical transition point
    // After this call, all memory addresses are interpreted as virtual rather than physical
    enable_virtual_memory();

    // Create the idle process - runs when no other process is ready
    // This process will execute the DoIdle function in an infinite loop
    pcb_t* idle_process = create_idle_process(uctxt);
    if (!idle_process) {
        TracePrintf(0, "ERROR: Failed to create idle process\n");
        Halt();  // System needs at least one process to function
    }
    
    // Return to the idle process's user context
    TracePrintf(0, "Leaving KernelStart, returning to idle process (PID %d)\n", idle_process->pid);
    *uctxt = idle_process->user_context;

    return;
}

void init_interrupt_vector(void) {
    // Allocate memory for interrupt vector table - array of function pointers
    // The hardware uses this table to determine which function to call for each trap type
    int *vector_table = (int *)malloc(sizeof(int) * TRAP_VECTOR_SIZE);
    if (!vector_table) {
        TracePrintf(0, "ERROR: Failed to allocate interrupt vector table\n");
        Halt();  // Critical error - interrupt handling is essential
    }

    // Set up handlers for each trap type by populating the vector table
    // Each entry contains the address of the function to handle that specific trap
    vector_table[TRAP_KERNEL] = (int)trap_kernel_handler;       // System calls from user processes
    vector_table[TRAP_CLOCK] = (int)trap_clock_handler;         // Timer interrupts for scheduling
    vector_table[TRAP_ILLEGAL] = (int)trap_illegal_handler;     // Illegal instruction exceptions
    vector_table[TRAP_MEMORY] = (int)trap_memory_handler;       // Memory access violations and stack growth
    vector_table[TRAP_MATH] = (int)trap_math_handler;           // Math errors like division by zero
    vector_table[TRAP_TTY_RECEIVE] = (int)trap_tty_receive_handler;   // Terminal input available
    vector_table[TRAP_TTY_TRANSMIT] = (int)trap_tty_transmit_handler; // Terminal output complete
    vector_table[TRAP_DISK] = (int)trap_disk_handler;           // Disk operations complete

    // Write the address of vector table to REG_VECTOR_BASE register
    // This tells the hardware where to find the interrupt handlers
    WriteRegister(REG_VECTOR_BASE, (unsigned int)vector_table);

    TracePrintf(0, "Interrupt vector table initialized at 0x%p\n", vector_table);
}

pcb_t *create_idle_process(UserContext *uctxt) {
    TracePrintf(1, "ENTER create_idle_process.\n");
    
    // Create a new PCB for the idle process
    pcb_t *idle_pcb = create_pcb();
    if (idle_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate idle PCB\n");
        return NULL;
    }
    
    // Get the current Region 1 page table pointer - this is a physical address at this point
    unsigned int r1_page_table_addr = ReadRegister(REG_PTBR1);
    
    // After VM is enabled, you'll need to access this through virtual memory
    // Store the physical address for now - your kernel will need to map this later
    idle_pcb->region1_pt = (struct pte *)r1_page_table_addr;
    
    // For kernel stack, save the page frame numbers
    // Since you're mapping 1:1 with page = frame, save these page numbers
    int base_kernelStack_page = KERNEL_STACK_BASE / PAGESIZE;
    int kernelStack_limit_page = KERNEL_STACK_LIMIT / PAGESIZE;
    
    // Allocate space to store kernel stack frame numbers
    int num_stack_frames = kernelStack_limit_page - base_kernelStack_page;
    idle_pcb->kernel_stack_pages = malloc(num_stack_frames * sizeof(int));
    if (idle_pcb->kernel_stack_pages == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate space for kernel stack frames\n");
        free(idle_pcb);
        return NULL;
    }
    
    // Save the frame numbers for the kernel stack
    for (int i = 0; i < num_stack_frames; i++) {
        idle_pcb->kernel_stack_pages[i] = base_kernelStack_page + i;
    }
    
    // Copy the UserContext provided to KernelStart
    memcpy(&idle_pcb->user_context, uctxt, sizeof(UserContext));
    
    // Point PC to DoIdle function and SP to top of user stack
    idle_pcb->user_context.pc = (void *)DoIdle;
    idle_pcb->user_context.sp = VMEM_1_LIMIT; // Top of Region 1
    
    // Set as current process
    current_process = idle_pcb;
    
    // Register with helper
    int pid = helper_new_pid(idle_pcb->region1_pt);
    idle_pcb->pid = pid;
    
    TracePrintf(1, "EXIT create_idle_process. Created idle process with PID %d\n", idle_pcb->pid);
    return idle_pcb;
}


pcb_t *create_init_process(char *name, char **args) {
    // Allocate PCB for init process - the first real user process
    // Init is special as it's the ancestor of all other user processes
    pcb_t *init_pcb = (pcb_t *)malloc(sizeof(pcb_t));
    if (!init_pcb) {
        TracePrintf(0, "ERROR: Failed to allocate PCB for init process\n");
        return NULL;
    }

    // Initialize PCB fields with process metadata
    // This establishes the identity and state of the process
    init_pcb->pid = helper_new_pid();    // Get unique process ID
    init_pcb->state = PROCESS_READY;     // Mark as ready to run
    init_pcb->parent_pid = -1;           // No parent for init

    // Create Region 1 page table for user space
    // This will hold mappings for text, data, heap, and stack
    int region1_pages = VMEM_1_SIZE >> PAGESHIFT;
    pte_t *region1_pt = (pte_t *)malloc(sizeof(pte_t) * region1_pages);
    if (!region1_pt) {
        TracePrintf(0, "ERROR: Failed to allocate Region 1 page table for init process\n");
        free(init_pcb);  // Clean up on error
        return NULL;
    }

    // Initialize page table entries as invalid
    // LoadProgram will set up the actual mappings later
    for (int i = 0; i < region1_pages; i++) {
        region1_pt[i].valid = 0;  // All pages invalid initially
    }

    // Save page table in PCB for later use during context switches
    // The kernel needs to know where each process's page tables are
    init_pcb->region1_pt = region1_pt;
    init_pcb->region1_pt_size = region1_pages;

    // Allocate kernel stack frames for init process
    // Each process needs its own kernel stack when handling traps
    for (int i = 0; i < KERNEL_STACK_MAXSIZE / PAGESIZE; i++) {
        int frame = allocate_frame();
        if (frame == -1) {
            TracePrintf(0, "ERROR: Failed to allocate kernel stack frame for init process\n");
            free(region1_pt);  // Clean up on error
            free(init_pcb);
            return NULL;
        }
        init_pcb->kernel_stack_frames[i] = frame;  // Store frame numbers for later mapping
    }

    TracePrintf(0, "Init process created with PID %d\n", init_pcb->pid);
    return init_pcb;
}

int setup_idle_context(pcb_t *idle_pcb) {
    // Initialize user context for idle process
    // This prepares the register state for when idle process runs
    idle_pcb->user_context.pc = (void *)DoIdle;  // Point to DoIdle function as entry point

    // Allocate one page for the user stack
    // Idle process needs minimal stack space for its simple loop
    int frame = allocate_frame();
    if (frame == -1) {
        TracePrintf(0, "ERROR: Failed to allocate frame for idle process user stack\n");
        return ERROR;
    }

    // Calculate page number for the user stack in Region 1
    // Stack is placed at the top of Region 1 virtual memory
    int stack_page = ((VMEM_1_LIMIT - PAGESIZE) - VMEM_1_BASE) >> PAGESHIFT;

    // Map the frame to the user stack address
    // This creates the virtual-to-physical mapping for the stack
    idle_pcb->region1_pt[stack_page].valid = 1;                   // Page is valid and mapped
    idle_pcb->region1_pt[stack_page].prot = PROT_READ | PROT_WRITE; // Stack needs read/write access
    idle_pcb->region1_pt[stack_page].pfn = frame;                 // Point to the allocated physical frame

    // Set stack pointer to top of user stack
    // Stack grows downward, so start at the high end of the page
    idle_pcb->user_context.sp = (void *)(VMEM_1_LIMIT - sizeof(int));

    // Initialize other context fields
    // Frame pointer typically equals stack pointer initially
    idle_pcb->user_context.ebp = idle_pcb->user_context.sp;

    return 0;
}

int load_init_program(pcb_t *init_pcb, char *filename, char **args) {
    // Set current process to init_pcb (required by LoadProgram)
    // LoadProgram uses this global to know which process to load into
    current_process = init_pcb;

    // Set Region 1 page table registers to point to init's page table
    // This makes the hardware use init's address space mappings
    WriteRegister(REG_PTBR1, (unsigned int)init_pcb->region1_pt);
    WriteRegister(REG_PTLR1, init_pcb->region1_pt_size);

    // Flush TLB for Region 1 to clear any stale mappings
    // This ensures hardware uses the new page table entries
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

    // Call LoadProgram to load executable into memory
    // This reads the program from disk and sets up text, data, and stack
    if (LoadProgram(filename, args, init_pcb) != 0) {
        TracePrintf(0, "ERROR: Failed to load program %s\n", filename);
        return ERROR;
    }

    return 0;
}

void init_lists(void) {
    // Create ready queue for runnable processes
    // This is used by the scheduler to track which processes can run
    ready_queue = create_queue();
    if (!ready_queue) {
        TracePrintf(0, "ERROR: Failed to create ready queue\n");
        Halt();  // Critical error - scheduling can't work without ready queue
    }

    // Initialize other global data structures
    // (Blocked queues, synchronization object tables, etc.)
    // These would be used for process waiting, locks, condition variables, etc.

    TracePrintf(0, "Process and resource lists initialized\n");
}

void DoIdle(void) {
    // Infinite loop that runs when no other process is ready
    // This prevents the CPU from halting when there's no work
    while (1) {
        TracePrintf(3, "Idle process running\n");  // Debug message at low priority
        Pause();  // Hardware instruction that yields CPU until next interrupt
                  // This is more efficient than busy-waiting
    }
}