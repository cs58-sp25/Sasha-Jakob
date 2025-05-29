/**
 * Date: 5/4/25
 * File: kernel.c
 * Description: Kernel implementation for Yalnix OS
 */

#include <hardware.h>
#include <ctype.h>
#include <load_info.h>
#include <yalnix.h>
#include <ykernel.h>
#include <ylib.h>
#include <yuser.h>

#include "pcb.h"
#include "traps.h"
#include "kernel.h"
#include "list.h"
#include "memory.h"
#include "context_switch.h"
#include "load_program.h"
#include "sync.h"
#include "syscalls.h"


/* ------------------------------------------------------------------ Kernel Start --------------------------------------------------------*/
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {

    // Print debug information about memory layout
    TracePrintf(0, "KernelStart: text start = 0x%x\n", _first_kernel_text_page);
    TracePrintf(0, "KernelStart: data start = 0x%x\n", _first_kernel_data_page);
    TracePrintf(0, "KernelStart: brk start = 0x%x\n", _orig_kernel_brk_page);

    trap_init(); // Initialize the interrupt vector table

    // Initialize the PCB system
    if (init_pcb_system() != 0) {
        TracePrintf(0, "ERROR: Failed to initialize PCB system\n");
        Halt(); // System cannot function without a PCB system; halt.
    }

    // Initialize page tables for Region 0 and initial kernel break
    init_region0_pageTable((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page, pmem_size);

    enable_virtual_memory(); // Enable virtual memory

    // Create the idle process
    pcb_t *idle_process = create_process(uctxt); // The uctxt parameter here is the initial UserContext provided by the hardware
    
    load_program(cmd_args[0], cmd_args, idle_process); // Load the initial program into the idle process

    // Set the idle process pcb values
    idle_process->user_context->sp = (void *)(VMEM_1_LIMIT - 4); // Set the stack pointer to the top of the kernel stack
    idle_process->user_context->pc = &DoIdle; // Set the program counter to the idle function
    
    current_process = idle_process; // Set the global 'current_process' to the newly created idle process
    uctxt->pc = &DoIdle; // Set the PC to the idle function
    uctxt->sp = (void *)(VMEM_1_LIMIT -4); // Set the stack pointer to the top of the kernel stack

    TracePrintf(0, "Leaving KernelStart, returning to idle process (PID %d)\n", idle_process->pid);
    return; // Return to user mode, entering the idle loop
}

// void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {

//     // Print debug information about memory layout
//     TracePrintf(0, "KernelStart: text start = 0x%x\n", _first_kernel_text_page);
//     TracePrintf(0, "KernelStart: data start = 0x%x\n", _first_kernel_data_page);
//     TracePrintf(0, "KernelStart: brk start = 0x%x\n", _orig_kernel_brk_page);

//     trap_init(); // Initialize the interrupt vector table

//     // Initialize the PCB system
//     if (init_pcb_system() != 0) {
//         TracePrintf(0, "ERROR: Failed to initialize PCB system\n");
//         Halt(); // System cannot function without a PCB system; halt.
//     }

//     // Initialize page tables for Region 0 and initial kernel break
//     init_region0_pageTable((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page, pmem_size);

//     enable_virtual_memory(); // Enable virtual memory

//     pcb_t *temp_pcb = create_process(NULL); // Pass NULL as it's not a user context being saved directly here
//     if (temp_pcb == NULL) {
//         TracePrintf(0, "ERROR: Failed to create temporary PCB\n");
//         Halt();
//     }

//     // Update the global current_process to this bootstrap PCB
//     current_process = temp_pcb;

//     // Determine the init program name and arguments from cmd_args
//     char *init_program_name = NULL;
//     char **init_program_args = NULL;

//     if (cmd_args != NULL && cmd_args[0] != NULL) {
//         // If arguments are provided, use the first one as the program name
//         init_program_name = cmd_args[0];
//         init_program_args = cmd_args; // Pass all arguments to init
//         TracePrintf(0, "KernelStart: Initializing with program specified on command line: %s\n", init_program_name);
//     } else {
//         // If no arguments are provided, load the default 'init' program
//         init_program_name = "test/init"; // Set the default init program name
//         init_program_args = NULL;   // The default init program will receive no arguments
//         TracePrintf(0, "KernelStart: No init program specified on command line. Loading default init: %s\n", init_program_name);
//     }

//     // Create the 'init' process PCB
//     pcb_t *init_process = create_process(NULL); // create_process will set up its own user_context
//     if (init_process == NULL) {
//         TracePrintf(0, "ERROR: Failed to create init process PCB\n");
//         Halt();
//     }

//     // Load the 'init' program into the new 'init_process'
//     int load_result = load_program(init_program_name, init_program_args, init_process);
//     if (load_result != 0) { // Assuming load_program returns 0 on success
//         TracePrintf(0, "ERROR: Failed to load init program '%s'. Halting.\n", init_program_name);
//         Halt();
//     }

//     TracePrintf(0, "KernelStart: Performing initial context switch to init process (PID %d)\n", init_process->pid);
//     KCSwitch(temp_pcb->kernel_context, temp_pcb, init_process);
    
//     TracePrintf(0, "ERROR: KCSwitch unexpectedly returned to KernelStart. Halting.\n");
//     Halt();
//     return;
// }


pcb_t *create_process(UserContext *uctxt){
    TracePrintf(1, "ENTER create_process.\n");
    pcb_t *new_pcb = create_pcb();

    if (new_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate idle PCB\n");
        return NULL;
    }

    // Allocate a NEW physical frame for the idle process's Region 1 page table.
    int pfn = allocate_frame();
    int stack_index = MAX_PT_LEN - 1;
    map_page(new_pcb->region1_pt, stack_index, pfn, PROT_READ | PROT_WRITE); // Map the physical frame to the last entry in Region 1 page table
    
    // Assign important values to the new process PCB
    int pid = helper_new_pid(new_pcb->region1_pt);
    new_pcb->pid = pid;
    new_pcb->user_context = uctxt; // Copy the UserContext from the argument

    // Set hardware registers for Region 1 page table
    WriteRegister(REG_PTBR1, (u_long)new_pcb->region1_pt); // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN); // Set limit of Region 1 page table to 1 entry initially
    TracePrintf(0, "Wrote region 1 page table base register to %p\n", (u_long)new_pcb->region1_pt);
    TracePrintf(0, "Wrote region 1 page table limit register to %u\n", 1);


    int kernel_stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
    int kernel_stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int num_kernel_stack_frames = kernel_stack_end - kernel_stack_start;

    // Set up kernel stack frames for this process
    new_pcb->kernel_stack_pages = malloc(num_kernel_stack_frames * sizeof(int));
    if (new_pcb->kernel_stack_pages == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate space for kernel stack frames\n");
        // Clean up previously allocated resources
        free_frame(num_kernel_stack_frames); // Free the user stack frame
        free(new_pcb);
        return NULL;
    }

    for (int i = kernel_stack_start; i < num_kernel_stack_frames; i++) {
        int pfn = allocate_frame();
        new_pcb->kernel_stack_pages[i] = pfn;
    }

    TracePrintf(1, "EXIT create_process. Created idle process with PID %d\n", new_pcb->pid);
    return new_pcb;
}


int load_program(char *name, char *args[], pcb_t *proc) {

    if (args == NULL || name == NULL) {
        TracePrintf(0, "No arguments provided -> loading default init process.\n");
        int result = LoadProgram("test/init", NULL, proc);
        return result; // Invalid arguments
    }

    int load_result = LoadProgram(args[0], args, proc);
    if (load_result != SUCCESS) { // Check for error loading program
        TracePrintf(0, "ERROR: LoadProgram failed for '%s' with error %d.\n", args[0], load_result);
        return ERROR;
    }
    return 0;
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
