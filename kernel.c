/**
 * Date: 5/4/25
 * File: kernel.c
 * Description: Kernel implementation for Yalnix OS
 */

#include <ctype.h>
#include <hardware.h>
#include <load_info.h>
#include <yalnix.h>
#include <ykernel.h>
#include <ylib.h>
#include <yuser.h>
#include <fcntl.h>

#include "context_switch.h"
#include "list.h"
#include "memory.h"
#include "pcb.h"
#include "sync.h"
#include "syscalls.h"
#include "traps.h"
#include "load_program.h"
#include "kernel.h"

/* ------------------------------------------------------------------ Kernel Start --------------------------------------------------------*/
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {
    TracePrintf(0, "KernelStart\n");

    // Initialize trap handlers
    trap_init();

    // Initialize PCB system, which includes process queues
    if (init_pcb_system() != 0) {
        TracePrintf(0, "ERROR: Failed to initialize PCB system\n");
        Halt();
    }

    // Initialize Region 0 page table
    init_region0_pageTable((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page, pmem_size);

    // Enable virtual memory
    enable_virtual_memory();

    // Create the idle process
    pcb_t *idle_pcb = create_process();  // create_process handles its own user_context setup
    if (idle_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to create idle process PCB\n");
        Halt();
    }

    // Set the idle process's user context to point to DoIdle
    // The initial user context for the idle process should be copied from the kernel's entry context (uctxt).
    memcpy(idle_pcb->user_context, uctxt, sizeof(UserContext));
    idle_pcb->user_context->pc = &DoIdle;                     // DoIdle is in your kernel.c
    idle_pcb->user_context->sp = (void *)(VMEM_1_LIMIT - 4);  // Standard initial stack pointer
    
    // Enqueue doidle
    add_to_ready_queue(idle_pcb);

    // Set hardware registers for Region 1 page table
    WriteRegister(REG_PTBR1, (u_long)idle_pcb->region1_pt);  // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN);                    // Set limit of Region 1 page table to 1 entry initially

    // Determine the name of the initial program to load
    char *name = (cmd_args != NULL && cmd_args[0] != NULL) ? cmd_args[0] : "test/init";
    TracePrintf(0, "Creating init pcb with name %s\n", name);

    // Create the 'init' process PCB
    pcb_t *init_pcb = create_process();
    if (init_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to create init process PCB\n");
        Halt();
    }
    // Copy initial UserContext for init process from uctxt
    memcpy(init_pcb->user_context, uctxt, sizeof(UserContext));

    current_process = init_pcb;
    init_pcb->state = PROCESS_RUNNING;
    
    // Set hardware registers for Region 1 page table init_process
    WriteRegister(REG_PTBR1, (u_long)init_pcb->region1_pt);  // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN);                    // Set limit of Region 1 page table to 1 entry initially


    // Load the 'init' program into the new 'init_process'
    int load_result = LoadProgram(name, cmd_args, init_pcb);
    if (load_result != SUCCESS) {
        TracePrintf(0, "ERROR: Failed to load init program '%s'. Halting.\n", name);
        Halt();
    }

    TracePrintf(0, "KernelStart: Performing initial context switch from idle (PID %d) to init process (PID %d)\n", idle_pcb->pid, init_pcb->pid);
    // This call is correct, assuming KCSwitch is the proper function pointer for KernelContextSwitch.
    int rc = KernelContextSwitch(KCSwitch, (void *)idle_pcb, (void *)init_pcb);
    if (rc == -1) {
        TracePrintf(0, "KernelContextSwitch failed when copying idle into init\n");
        Halt();
    }

    TracePrintf(0, "Exiting KernelStart\n");
}

pcb_t *create_process(void) {
    TracePrintf(1, "ENTER create_process.\n");
    pcb_t *new_pcb = create_pcb();

    if (new_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate idle PCB\n");
        return NULL;
    }

    // Allocate a NEW physical frame for the idle process's Region 1 page table.
    int pfn = allocate_frame();
    int stack_index = MAX_PT_LEN - 1;
    map_page(new_pcb->region1_pt, stack_index, pfn, PROT_READ | PROT_WRITE);  // Map the physical frame to the last entry in Region 1 page table

    // Assign important values to the new process PCB
    int pid = helper_new_pid(new_pcb->region1_pt);
    new_pcb->pid = pid;

    int kernel_stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
    int kernel_stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int num_kernel_stack_frames = kernel_stack_end - kernel_stack_start;

    // Set up kernel stack frames for this process
    new_pcb->kernel_stack = malloc(num_kernel_stack_frames * sizeof(pte_t));
    if (new_pcb->kernel_stack == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate space for kernel stack frames\n");
        // Clean up previously allocated resources
        free_frame(num_kernel_stack_frames);  // Free the user stack frame
        free(new_pcb);
        return NULL;
    }

    new_pcb->kernel_stack = InitializeKernelStack();

    TracePrintf(1, "EXIT create_process. Created process with PID %d\n", new_pcb->pid);
    return new_pcb;
}

void DoIdle(void) {
    // Infinite loop that runs when no other process is ready
    // This prevents the CPU from halting when there's no work
    while (1) {
        TracePrintf(0, "Idle process running\n");  // Debug message at low priority
        Pause();                                   // Hardware instruction that yields CPU until next interrupt
                                                   // This is more efficient than busy-waiting
    }
}

