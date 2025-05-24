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
// #include "sync.h"
// #include "syscalls.h"

/* -------------------------------------------------------------- Define Global Variables -------------------------------------------------- */
pcb_t *current_process = NULL;


/* ------------------------------------------------------------------ Kernel Start --------------------------------------------------------*/
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt) {

    // Print debug information about memory layout
    TracePrintf(0, "KernelStart: text start = 0x%x\n", _first_kernel_text_page);
    TracePrintf(0, "KernelStart: data start = 0x%x\n", _first_kernel_data_page);
    TracePrintf(0, "KernelStart: brk start = 0x%x\n", _orig_kernel_brk_page);

    // Initialize the interrupt vector table
    trap_init();

    // Initialize page tables for Region 0 and initial kernel break
    init_region0_pageTable((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page, pmem_size);

    // Create the idle process
    pcb_t *idle_process = create_idle_process(uctxt); // The uctxt parameter here is the initial UserContext provided by the hardware
    if (!idle_process) {
        TracePrintf(0, "ERROR: Failed to create idle process\n");
        Halt(); // System cannot function without an idle process; halt.
    }

    enable_virtual_memory(); // Enable virtual memory

    current_process = idle_process; // Set the global 'current_process' to the newly created idle process
    uctxt->pc = (void *)DoIdle; // Set the PC to the idle function
    uctxt->sp = (void *)(VMEM_1_LIMIT -1); // Set the stack pointer to the top of the kernel stack

    TracePrintf(0, "Leaving KernelStart, returning to idle process (PID %d)\n", idle_process->pid);
    return; // Return to user mode, entering the idle loop.
}


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
    new_pcb->region1_pt[stack_index].valid = 1;
    new_pcb->region1_pt[stack_index].prot = PROT_READ | PROT_WRITE; // User stack needs read/write permissions
    new_pcb->region1_pt[stack_index].pfn = pfn; // Map to the allocated physical frame

    // Set hardware registers for Region 1 page table.
    WriteRegister(REG_PTBR1, (u_long)new_pcb->region1_pt); // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, (unsigned int)1); // Set limit of Region 1 page table to 1 entry initially
    TracePrintf(0, "Wrote region 1 page table base register to %p\n", (u_long)new_pcb->region1_pt);
    TracePrintf(0, "Wrote region 1 page table limit register to %u\n", (unsigned int)1);


    int kernel_stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
    int kernel_stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int num_kernel_stack_frames = kernel_stack_end - kernel_stack_start;

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

    // Assign PID to the new process
    int pid = helper_new_pid(new_pcb->region1_pt);
    new_pcb->pid = pid;
    TracePrintf(1, "EXIT create_process. Created idle process with PID %d\n", new_pcb->pid);
    
}

// ------------------------------------ NEED TO CHANGE WHERE THE PAGES POINT TO FOR DOIDLE ------------------------------------
pcb_t *create_idle_process(UserContext *uctxt) {
    TracePrintf(1, "ENTER create_idle_process.\n");
    pcb_t *idle_pcb = create_pcb();
    if (idle_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate idle PCB\n");
        return NULL;
    }

    // Allocate a NEW physical frame for the idle process's Region 1 page table.
    int pfn = allocate_frame();
    int stack_index = MAX_PT_LEN - 1;
    idle_pcb->region1_pt[stack_index].valid = 1;
    idle_pcb->region1_pt[stack_index].prot = PROT_READ | PROT_WRITE; // User stack needs read/write permissions
    idle_pcb->region1_pt[stack_index].pfn = pfn; // Map to the allocated physical frame

    // Set hardware registers for Region 1 pidle table.
    WriteRegister(REG_PTBR1, (u_long)idle_pcb->region1_pt); // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, (unsigned int)1); // Set limit of Region 1 page table to 1 entry initially
    TracePrintf(0, "Wrote region 1 page table base register to %p\n", (u_long)idle_pcb->region1_pt);
    TracePrintf(0, "Wrote region 1 page table limit register to %u\n", (unsigned int)1);


    int kernel_stack_start = KERNEL_STACK_BASE >> PAGESHIFT;
    int kernel_stack_end = KERNEL_STACK_LIMIT >> PAGESHIFT;
    int num_kernel_stack_frames = kernel_stack_end - kernel_stack_start;

    idle_pcb->kernel_stack_pages = malloc(num_kernel_stack_frames * sizeof(int));
    if (idle_pcb->kernel_stack_pages == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate space for kernel stack frames\n");
        // Clean up previously allocated resources
        free_frame(num_kernel_stack_frames); // Free the user stack frame
        free(idle_pcb);
        return NULL;
    }

    for (int i = kernel_stack_start; i < num_kernel_stack_frames; i++) {
        int pfn = allocate_frame();
        idle_pcb->kernel_stack_pages[i] = pfn;
    }

    // Register the new process with the helper function to get a PID.
    int pid = helper_new_pid(idle_pcb->region1_pt);
    idle_pcb->pid = pid;

    TracePrintf(1, "EXIT create_idle_process. Created idle process with PID %d\n", idle_pcb->pid);
    return idle_pcb;
}


void DoIdle(void) {
    // Infinite loop that runs when no other process is ready
    // This prevents the CPU from halting when there's no work
    while (1) {
        TracePrintf(3, "Idle process running\n");  // Debug message at low priority
        Pause();                                   // Hardware instruction that yields CPU until next interrupt
                                                   // This is more efficient than busy-waiting
    }
}