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
#include "load_program.c"
// #include "sync.h"
// #include "syscalls.h"


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
    uctxt->sp = (void *)(VMEM_1_LIMIT -4); // Set the stack pointer to the top of the kernel stack

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
    WriteRegister(REG_PTLR1, MAX_PT_LEN); // Set limit of Region 1 page table to 1 entry initially
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

    // Assign important values to the new process PCB
    int pid = helper_new_pid(new_pcb->region1_pt);
    new_pcb->pid = pid;
    new_pcb->user_context = *uctxt; // Copy the UserContext from the argument
    TracePrintf(1, "EXIT create_process. Created idle process with PID %d\n", new_pcb->pid);
    
}

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
    map_page(idle_pcb->region1_pt, stack_index, pfn, PROT_READ | PROT_WRITE); // Map the virtual page to the physical frame

    // Set hardware registers for Region 1 pidle table.
    WriteRegister(REG_PTBR1, (u_long)idle_pcb->region1_pt); // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN); // Set limit of Region 1 page table to 1 entry initially
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

    // Assign important values to the idle PCB
    int pid = helper_new_pid(idle_pcb->region1_pt);
    idle_pcb->pid = pid;
    idle_pcb->user_context = *uctxt; // Copy the UserContext from the argument
    idle_pcb->user_context.sp = (void *) VMEM_1_LIMIT - 4; // Set the stack pointer to the top of the kernel stack
    idle_pcb->user_context.pc = (void *) DoIdle; // Set the program counter to the idle function

    TracePrintf(1, "EXIT create_idle_process. Created idle process with PID %d\n", idle_pcb->pid);
    return idle_pcb;
}


pcb_t *create_init_process(UserContext *uctxt, char *cmd_args[]) {
    TracePrintf(1, "Entering create_init_process.\n");

    // Allocate memory for initPCB
    pcb_t *initPCB = (pcb_t *)malloc(sizeof(pcb_t));
    if (!initPCB) {
        TracePrintf(0, "ERROR: Failed to allocate initPCB.\n");
        return NULL;
    }
    memset(initPCB, 0, sizeof(pcb_t)); // Initialize to zeros

    // Allocate and initialize its region 1 page table (all invalid) [cite: 457]
    initPCB->region1_pt = (pte_t *)malloc(sizeof(pte_t) * MAX_PT_LEN);
    if (!initPCB->region1_pt) {
        TracePrintf(0, "ERROR: Failed to allocate region1_pt for initPCB.\n");
        free(initPCB);
        return NULL;
    }
    for (int i = 0; i < MAX_PT_LEN; i++) {
        initPCB->region1_pt[i].valid = 0; // All entries invalid
    }

    // Allocate new frames for its kernel stack frames [cite: 457]
    int num_kernel_stack_pages = KERNEL_STACK_MAXSIZE / PAGESIZE;
    initPCB->kernel_stack_pages = (int *)malloc(sizeof(int) * num_kernel_stack_pages);
    if (!initPCB->kernel_stack_pages) {
        TracePrintf(0, "ERROR: Failed to allocate kernel_stack_pages for initPCB.\n");
        free(initPCB->region1_pt);
        free(initPCB);
        return NULL;
    }
    for (int i = 0; i < num_kernel_stack_pages; i++) {
        int pfn = allocate_frame();
        if (pfn == ERROR) {
            TracePrintf(0, "ERROR: Failed to allocate frame for initPCB kernel stack.\n");
            // Clean up previously allocated frames
            for (int j = 0; j < i; j++) free_frame(initPCB->kernel_stack_pages[j]);
            free(initPCB->kernel_stack_pages);
            free(initPCB->region1_pt);
            free(initPCB);
            return NULL;
        }
        initPCB->kernel_stack_pages[i] = pfn;
    }

    // Set its pid from helper new pid() [cite: 457]
    initPCB->pid = helper_new_pid(initPCB->region1_pt);
    TracePrintf(1, "Created init process with PID %d.\n", initPCB->pid);

    // Copy the UserContext from the uctxt argument to KernelStart [cite: 457]
    initPCB->user_context = *uctxt;
    // The user_context.sp and user_context.pc will be set by LoadProgram

    // Set initial process state
    initPCB->state = PROCESS_READY;
    initPCB->parent = NULL; // Init has no parent
    init_list_head(&initPCB->children);
    initPCB->exit_code = 0;
    initPCB->brk = (void *)USER_VM_AREA_BASE; // Will be adjusted by LoadProgram

    // Determine init program name from cmd_args or default to "init" [cite: 457]
    char *init_program_name = "init";
    char **init_args = cmd_args;

    if (cmd_args[0] != NULL) {
        init_program_name = cmd_args[0];
    } else {
        TracePrintf(1, "No command line arguments provided for init. Using default 'init' program.\n");
        // Create a minimal argument list for default "init" if no args are provided
        // This is a simplification; you might need a more robust way to handle this.
        init_args = (char *[]){"init", NULL};
    }

    // Load the "init" program into initPCB's Region 1 address space [cite: 457]
    int load_status = LoadProgram(init_program_name, init_args, initPCB);
    if (load_status != SUCCESS) {
        TracePrintf(0, "ERROR: Failed to load init program %s (status %d).\n", init_program_name, load_status);
        // Clean up allocated resources if LoadProgram fails
        for (int i = 0; i < num_kernel_stack_pages; i++) free_frame(initPCB->kernel_stack_pages[i]);
        free(initPCB->kernel_stack_pages);
        free(initPCB->region1_pt);
        free(initPCB);
        return NULL;
    }

    // Use KernelContextSwitch to copy the current KernelStart context to initPCB's kernel_context
    // This effectively transfers the initial kernel execution state to init.
    // KCCopy will be the function that saves the incoming KernelContext (from KernelStart)
    // and copies the stack.
    initPCB->kernel_context = (KernelContext *)KernelContextSwitch(KCCopy, NULL, initPCB);
    if (initPCB->kernel_context == NULL) {
        TracePrintf(0, "ERROR: KCCopy failed for init process.\n");
        // Handle error and cleanup
        return NULL;
    }

    TracePrintf(1, "Exiting create_init_process. Init PCB created and program loaded.\n");
    return initPCB;
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
