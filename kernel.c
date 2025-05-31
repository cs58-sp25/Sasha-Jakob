/**
 * Date: 5/4/25
 * File: kernel.c
 * Description: Kernel implementation for Yalnix OS
 */

#include "kernel.h"

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

static int LoadProgram(char *name, char *args[], pcb_t *proc);

/* ------------------------------------------------------------------ Kernel Start --------------------------------------------------------*/
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

//     // Create the idle process
//     pcb_t *idle_process = create_process(uctxt); // The uctxt parameter here is the initial UserContext provided by the hardware

//     load_program(cmd_args[0], cmd_args, idle_process); // Load the initial program into the idle process

//     // Set the idle process pcb values
//     idle_process->user_context->sp = (void *)(VMEM_1_LIMIT - 4); // Set the stack pointer to the top of the kernel stack
//     idle_process->user_context->pc = &DoIdle; // Set the program counter to the idle function

//     current_process = idle_process; // Set the global 'current_process' to the newly created idle process
//     uctxt->pc = &DoIdle; // Set the PC to the idle function
//     uctxt->sp = (void *)(VMEM_1_LIMIT -4); // Set the stack pointer to the top of the kernel stack

//     TracePrintf(0, "Leaving KernelStart, returning to idle process (PID %d)\n", idle_process->pid);
//     return; // Return to user mode, entering the idle loop
// }

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

    // Set hardware registers for Region 1 page table
    WriteRegister(REG_PTBR1, (u_long)idle_pcb->region1_pt);  // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN);                    // Set limit of Region 1 page table to 1 entry initially

    // Set the global current_process to idle initially
    current_process = idle_pcb;
    idle_pcb->state = PROCESS_RUNNING;

    // Determine the name of the initial program to load
    char *name = (cmd_args != NULL && cmd_args[0] != NULL) ? cmd_args[0] : "test/init";
    TracePrintf(0, "Creating init pcb with name %s\n", name);

    // Create the 'init' process PCB
    pcb_t *init_pcb = create_pcb();
    if (init_pcb == NULL) {
        TracePrintf(0, "ERROR: Failed to create init process PCB\n");
        Halt();
    }
    // Copy initial UserContext for init process from uctxt
    memcpy(init_pcb->user_context, uctxt, sizeof(UserContext));

    // Load the 'init' program into the new 'init_process'
    int load_result = LoadProgram(name, cmd_args, init_pcb);
    if (load_result != SUCCESS) {
        TracePrintf(0, "ERROR: Failed to load init program '%s'. Halting.\n", name);
        Halt();
    }

    // Set hardware registers for Region 1 page table
    WriteRegister(REG_PTBR1, (u_long)init_pcb->region1_pt);  // Set base physical address of Region 1 page table (now a static array address)
    WriteRegister(REG_PTLR1, MAX_PT_LEN);                    // Set limit of Region 1 page table to 1 entry initially
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    // Add the init process to the ready queue
    add_to_ready_queue(init_pcb);

    TracePrintf(0, "KernelStart: Performing initial context switch from idle (PID %d) to init process (PID %d)\n", idle_pcb->pid, init_pcb->pid);
    // This call is correct, assuming KCSwitch is the proper function pointer for KernelContextSwitch.
    int rc = KernelContextSwitch(KCSwitch, (void *)idle_pcb, (void *)init_pcb);
    if (rc == -1) {
        TracePrintf(0, "KernelContextSwitch failed when copying idle into init\n");
        Halt();
    }

    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
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



/*
 *  Load a program into an existing address space.  The program comes from
 *  the Linux file named "name", and its arguments come from the array at
 *  "args", which is in standard argv format.  The argument "proc" points
 *  to the process or PCB structure for the process into which the program
 *  is to be loaded.
 */

/*
 * ==>> Declare the argument "proc" to be a pointer to the PCB of
 * ==>> the current process.
 */
static int LoadProgram(char *name, char *args[], pcb_t *proc) {
    TracePrintf(1, "Enter LoadProgram.\n");
    int fd;
    int (*entry)();
    struct load_info li;
    int i;
    char *cp;
    char **cpp;
    char *cp2;
    int argcount;
    int size;
    int text_pg1;
    int data_pg1;
    int data_npg;
    int stack_npg;
    long segment_size;
    char *argbuf;

    /*
     * Open the executable file
     */
    if ((fd = open(name, O_RDONLY)) < 0) {
        TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
        return ERROR;
    }

    if (LoadInfo(fd, &li) != LI_NO_ERROR) {
        TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
        close(fd);
        return (-1);
    }

    if (li.entry < VMEM_1_BASE) {
        TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
        close(fd);
        return ERROR;
    }

    /*
     * Figure out in what region 1 page the different program sections
     * start and end
     */
    text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
    data_npg = li.id_npg + li.ud_npg;
    /*
     *  Figure out how many bytes are needed to hold the arguments on
     *  the new stack that we are building.  Also count the number of
     *  arguments, to become the argc that the new "main" gets called with.
     */
    size = 0;
    for (i = 0; args[i] != NULL; i++) {
        TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
        size += strlen(args[i]) + 1;
    }
    argcount = i;

    TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

    /*
     *  The arguments will get copied starting at "cp", and the argv
     *  pointers to the arguments (and the argc value) will get built
     *  starting at "cpp".  The value for "cpp" is computed by subtracting
     *  off space for the number of arguments (plus 3, for the argc value,
     *  a NULL pointer terminating the argv pointers, and a NULL pointer
     *  terminating the envp pointers) times the size of each,
     *  and then rounding the value *down* to a double-word boundary.
     */
    cp = ((char *)VMEM_1_LIMIT) - size;

    cpp = (char **)(((int)cp - ((argcount + 3 + POST_ARGV_NULL_SPACE) * sizeof(void *))) & ~7);

    /*
     * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
     * reserved above the stack pointer, before the arguments.
     */
    cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;

    TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n", li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);

    /*
     * Compute how many pages we need for the stack */
    stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

    TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n", li.t_npg + data_npg, stack_npg);

    /* leave at least one page between heap and stack */
    if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN) {
        close(fd);
        return ERROR;
    }

    /*
     * This completes all the checks before we proceed to actually load
     * the new program.  From this point on, we are committed to either
     * loading succesfully or killing the process.
     */

    /*
     * Set the new stack pointer value in the process's UserContext
     */

    /*
     * ==>> (rewrite the line below to match your actual data structure)
     * ==>> proc->uc.sp = cp2;
     */
    // Rewrite the line from above to do what it's supposed to
    proc->user_context->sp = cp2;

    /*
     * Now save the arguments in a separate buffer in region 0, since
     * we are about to blow away all of region 1.
     */
    cp2 = argbuf = (char *)malloc(size);

    /*
     * ==>> You should perhaps check that malloc returned valid space
     */
    // Check to see if the malloc failed
    if (cp2 == NULL) {
        TracePrintf(1, "ERROR, malloc for cp2 failed in LoadProgram.\n");
        return ERROR;
    }

    for (i = 0; args[i] != NULL; i++) {
        TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
        strcpy(cp2, args[i]);
        cp2 += strlen(cp2) + 1;
    }

    /*
     * Set up the page tables for the process so that we can read the
     * program into memory.  Get the right number of physical pages
     * allocated, and set them all to writable.
     */

    /* ==>> Throw away the old region 1 virtual address space by
     * ==>> curent process by walking through the R1 page table and,
     * ==>> for every valid page, free the pfn and mark the page invalid.
     */
    // Loop over the region 1 page table
    for (int i = 0; i < VMEM_1_SIZE; i++) {
        // Get the ith page table entry
        pte_t entry = proc->region1_pt[i];
        if (entry.valid) {
            // free the pfn
            TracePrintf("Load_program: freeing physical frame corresponding to virtual page %d\n", i);
            int pfn = entry.pfn;
            free_frame(pfn);
            entry.pfn = 0;
            // Set the protections and validity of the page to all 0
            entry.prot = 0;
            entry.valid = 0;
        }
    }

    /*
     * ==>> Then, build up the new region1.
     * ==>> (See the LoadProgram diagram in the manual.)
     */

    /*
     * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
     * ==>> the "text_pg1" page in region 1 address space.
     * ==>> These pages should be marked valid, with a protection of
     * ==>> (PROT_READ | PROT_WRITE).
     */
    for (int i = text_pg1; i < text_pg1 + li.t_npg; i++) {
        pte_t entry = proc->region1_pt[i];
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        entry.valid = 1;
        entry.prot = PROT_READ | PROT_WRITE;
        entry.pfn = nf;
    }

    /*
     * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
     * ==>> the  "data_pg1" in region 1 address space.
     * ==>> These pages should be marked valid, with a protection of
     * ==>> (PROT_READ | PROT_WRITE).
     */
    for (int i = data_pg1; i < data_pg1 + data_npg; i++) {
        pte_t entry = proc->region1_pt[i];
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        entry.valid = 1;
        entry.prot = PROT_READ | PROT_WRITE;
        entry.pfn = nf;
    }

    /*
     * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
     * ==>> of the region 1 virtual address space.
     * ==>> These pages should be marked valid, with a
     * ==>> protection of (PROT_READ | PROT_WRITE).
     */
    for (int i = MAX_PT_LEN - stack_npg; i < MAX_PT_LEN; i++) {
        pte_t entry = proc->region1_pt[i];
        int nf = allocate_frame();
        if (nf == ERROR) {
            TracePrintf(1, "ERROR, no new frames to allocate for LoadProgram.\n");
            return ERROR;
        }
        entry.valid = 1;
        entry.prot = PROT_READ | PROT_WRITE;
        entry.pfn = nf;
    }

    /*
     * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
     */
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    /*
     * All pages for the new address space are now in the page table.
     */

    /*
     * Read the text from the file into memory.
     */
    lseek(fd, li.t_faddr, SEEK_SET);
    segment_size = li.t_npg << PAGESHIFT;
    if (read(fd, (void *)li.t_vaddr, segment_size) != segment_size) {
        close(fd);
        return KILL;  // see ykernel.h
    }

    /*
     * Read the data from the file into memory.
     */
    lseek(fd, li.id_faddr, 0);
    segment_size = li.id_npg << PAGESHIFT;

    if (read(fd, (void *)li.id_vaddr, segment_size) != segment_size) {
        close(fd);
        return KILL;
    }

    close(fd); /* we've read it all now */

    /*
     * ==>> Above, you mapped the text pages as writable, so this code could write
     * ==>> the new text there.
     *
     * ==>> But now, you need to change the protections so that the machine can execute
     * ==>> the text.
     *
     * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
     * ==>> If any of these page table entries is also in the TLB,
     * ==>> you will need to flush the old mapping.
     */
    for (int i = text_pg1; i < text_pg1 + li.t_npg; i++) {
        pte_t entry = proc->region1_pt[i];
        entry.prot = PROT_READ | PROT_EXEC;
        // This operation has no effect if the page is not in the TLB, may be inefficient but oh well
        WriteRegister(REG_TLB_FLUSH, i << PAGESHIFT);
    }

    /*
     * Zero out the uninitialized data area
     */
    bzero(li.id_end, li.ud_end - li.id_end);
    bzero((void *)li.id_end, li.ud_end - li.id_end);

    /*
     * Set the entry point in the process's UserContext
     */

    /*
     * ==>> (rewrite the line below to match your actual data structure)
     * ==>> proc->uc.pc = (caddr_t) li.entry;
     */
    proc->user_context->pc = (caddr_t)li.entry;

    /*
     * Now, finally, build the argument list on the new stack.
     */

    memset(cpp, 0x00, VMEM_1_LIMIT - ((int)cpp));

    *cpp++ = (char *)argcount; /* the first value at cpp is argc */
    cp2 = argbuf;
    for (i = 0; i < argcount; i++) { /* copy each argument and set argv */
        *cpp++ = cp;
        strcpy(cp, cp2);
        cp += strlen(cp) + 1;
        cp2 += strlen(cp2) + 1;
    }
    free(argbuf);
    *cpp++ = NULL; /* the last argv is a NULL pointer */
    *cpp++ = NULL; /* a NULL pointer for an empty envp */

    TracePrintf(0, "Load_program: returned from load program with success\n");
    return SUCCESS;
}

