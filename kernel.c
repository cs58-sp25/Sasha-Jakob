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
    TracePrintf(0, "Enter KernelStart.\n");

    // Initialize trap handlers
    trap_init();

    // Initialize PCB system, which includes process queues
    if (init_pcb_system() != 0) {
        TracePrintf(0, "ERROR: Failed to initialize PCB system\n");
        Halt();
    }

    // Initialize Region 0 page table
    init_region0_pageTable((int)_first_kernel_text_page, (int)_first_kernel_data_page, (int)_orig_kernel_brk_page, pmem_size);
    
    // Create the idle process
    pcb_t *idle_pcb = idle_process;
    idle_pcb = create_process();
    if(idle_pcb == NULL){
        TracePrintf(0, "ERROR, failed to initialize the idle pcb.n");
        Halt();
    }

    // Set all of doidle's pages to invalid
    for(int i=0; i<MAX_PT_LEN; i++){
        idle_pcb->region1_pt[i].valid = 0;
    }
    
    // Attempt to get a frame for a location to put doidle
    int pfn = allocate_frame();
    if(pfn == ERROR){
        TracePrintf(1, "ERROR, failed to allocate a frame for idle's stack");
        Halt();
    }

    // map the page for the stack of doidle
    map_page(idle_pcb->region1_pt, MAX_VPN - 1, pfn, PROT_READ | PROT_WRITE);

    // Set the registers for the region 1 page table location for doidle
    WriteRegister(REG_PTBR1, (unsigned int)idle_pcb->region1_pt);
    WriteRegister(REG_PTLR1, MAX_PT_LEN);

    // Set up the kernel stack of doidle, copy the user context into it, and set the pc and sp
    idle_pcb->kernel_stack = InitializeKernelStack();
    cpyuc(idle_pcb->user_context, uctxt);
    idle_pcb->user_context->pc = &DoIdle;                     // DoIdle is in your kernel.c
    idle_pcb->user_context->sp = (void *)(VMEM_1_LIMIT - 4);  // Standard initial stack pointer

    // Now that idle is made turn on virtual memory
    enable_virtual_memory();

    // Determine the name of the initial program to load
    char *name = (cmd_args != NULL && cmd_args[0] != NULL) ? cmd_args[0] : "test/init";
    TracePrintf(0, "Creating init pcb with name %s\n", name);

    // Create the 'init' process PCB
    pcb_t *init_pcb = create_process();
    if (init_pcb == NULL) {
        TracePrintf(0, "ERROR, Failed to create init process PCB\n");
        Halt();
    }

    TracePrintf(0, "Initializing kernelStack for INIT_PCB\n");
    init_pcb->kernel_stack = InitializeKernelStack();
    cpyuc(init_pcb->user_context, uctxt);

    WriteRegister(REG_PTBR1, (unsigned int)init_pcb->region1_pt);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    if (LoadProgram(name, cmd_args, init_pcb) != SUCCESS){
        TracePrintf(1, "ERROR, failed to load init.\n");
    }
    

    WriteRegister(REG_PTBR1, (unsigned int)idle_pcb->region1_pt);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    TracePrintf(1, "Cloning idle into init.\n");
    if(KernelContextSwitch(KCCopy, (void *) init_pcb, NULL) == ERROR){
        TracePrintf(1, "Failed to clone idle into init.\n");
    }

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
    add_to_ready_queue(init_pcb);
    cpyuc(uctxt, init_pcb->user_context);
    current_process = idle_pcb;
    idle_pcb->state = PROCESS_RUNNING;

    TracePrintf(0, "EXIT KernelStart.\n");
}

pcb_t *create_process(void) {
    TracePrintf(1, "ENTER create_process.\n");
    pcb_t *new_pcb = create_pcb();

    if (new_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to allocate idle PCB\n");
        return NULL;
    }

    // Make the kernel stack
    // new_pcb->kernel_stack = InitializeKernelStack();

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

