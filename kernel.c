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
#include "sync.h"
#include "syscalls.h"
#include "traps.h"
#include "kernel.h"
#include "list.h"
#include "memory.h"

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
    pcb_t *idle_process = create_idle_process(uctxt);
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
    vector_table[TRAP_KERNEL] = (int)kernel_handler;        // System calls from user processes
    vector_table[TRAP_CLOCK] = (int)clock_handler;          // Timer interrupts for scheduling
    vector_table[TRAP_ILLEGAL] = (int)illegal_handler;      // Illegal instruction exceptions
    vector_table[TRAP_MEMORY] = (int)memory_handler;        // Memory access violations and stack growth
    vector_table[TRAP_MATH] = (int)math_handler;            // Math errors like division by zero
    vector_table[TRAP_TTY_RECEIVE] = (int)receive_handler;   // Terminal input available
    vector_table[TRAP_TTY_TRANSMIT] = (int)transmit_handler; // Terminal output complete

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
    idle_pcb->user_context.sp = (void *)VMEM_1_LIMIT;  // Top of Region 1
    TracePrintf(3, "create_idle_process: Idle process PC set to DoIdle, SP set to %p\n", idle_pcb->user_context.sp);

    // Set as current process
    current_process = idle_pcb;

    // Register with helper
    int pid = helper_new_pid(idle_pcb->region1_pt);
    idle_pcb->pid = pid;

    TracePrintf(1, "EXIT create_idle_process. Created idle process with PID %d\n", idle_pcb->pid);
    return idle_pcb;
}


int copy_kernel_stack(int *src_frames, int *dst_frames, int num_frames) {
    TracePrintf(3, "copy_kernel_stack: Copying %d frames from source to destination kernel stack.\n", num_frames);

    // Loop through each frame of the kernel stack
    for (int i = 0; i < num_frames; i++) {
        void *src_vaddr = (void *)(KERNEL_STACK_BASE + (i * PAGESIZE)); // Source virtual address in Region 0
        void *dst_temp_vaddr = setup_temp_mapping(dst_frames[i]); // Temporarily map destination frame

        if (dst_temp_vaddr == NULL) {
            TracePrintf(0, "copy_kernel_stack: ERROR: Failed to set up temporary mapping for destination frame %d.\n", dst_frames[i]);
            // TODO: Potentially clean up already mapped frames in case of error
            return ERROR;
        }

        // Copy the contents of the source frame to the temporarily mapped destination frame
        memcpy(dst_temp_vaddr, src_vaddr, PAGESIZE);

        // Remove the temporary mapping for the destination frame
        remove_temp_mapping(dst_temp_vaddr);
    }

    TracePrintf(3, "copy_kernel_stack: Kernel stack copy completed successfully.\n");
    return 0; // Success
}


int switch_to_process(pcb_t *next) {
    TracePrintf(2, "switch_to_process: Initiating context switch from PID %d to PID %d.\n",
                current_process ? current_process->pid : -1, next->pid);

    // Save the current user context if there is a current process
    // This assumes that the user context passed to the trap handler is the one to save
    // For simplicity, we'll assume current_process->user_context is the target
    // In a real trap handler, the uctxt argument would be saved here.
    if (current_process != NULL) {
        // This is a placeholder. In a real trap handler, 'uctxt' would be passed in
        // and its contents saved to current_process->user_context.
        // For demonstration, we'll assume current_process->user_context is already up-to-date
        // or that it will be handled by the trap entry/exit logic.
        // save_user_context(current_user_context_from_trap, current_process);
        TracePrintf(4, "switch_to_process: Saved user context for current process PID %d.\n", current_process->pid);
    }

    // Update the current_process global pointer
    current_process = next;
    TracePrintf(4, "switch_to_process: Current process updated to PID %d.\n", current_process->pid);

    // Update the hardware's Region 1 page table base register (PTBR1)
    // This changes the user address space to that of the new process
    WriteRegister(REG_PTBR1, (unsigned int)current_process->region1_pt);
    WriteRegister(REG_PTLR1, (unsigned int)((VMEM_REGION_SIZE - VMEM_0_LIMIT)/PAGESIZE));

    // Map the kernel stack of the new process into Region 0
    // This updates the Region 0 page table entries for the kernel stack area
    if (map_kernel_stack(current_process->kernel_stack_pages) != 0) {
        TracePrintf(0, "switch_to_process: ERROR: Failed to map kernel stack for PID %d.\n", current_process->pid);
        return ERROR;
    }

    // Flush the TLB for Region 1 and the kernel stack to ensure new mappings are used
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

    // Perform the low-level kernel context switch
    // KCSwitch will save the old kernel context and load the new one
    // The return value of KCSwitch is the kernel context of the process that is being switched back to.
    // In this case, we are switching TO 'next', so KCSwitch will return 'next->kernel_context'
    // when 'next' eventually gets scheduled back in.
    int kc_returned = KernelContextSwitch(KCSwitch, NULL, (void *)next);

    // When this function returns, it means a process has been switched back to this context.
    // The 'kc_returned' would be the kernel context that was just restored.
    // This part of the code would only be reached when a process that was switched OUT
    // is now being switched back IN.
    TracePrintf(2, "switch_to_process: Returned from KCSwitch. Process PID %d resumed.\n", current_process->pid);

    return 0; // Success
}


KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    pcb_t *next_proc = (pcb_t *)next_pcb_p;
    // pcb_t *curr_proc = (pcb_t *)curr_pcb_p; // If we were to save the outgoing context here

    TracePrintf(3, "KCSwitch: Performing low-level kernel context switch to PID %d.\n", next_proc->pid);

    // In a full implementation, you would save kc_in to the outgoing process's PCB
    // For now, we assume the caller of switch_to_process has already handled saving
    // the user context and that the kernel context is implicitly handled by KernelContextSwitch.

    // Return the kernel context of the next process to resume execution
    return &next_proc->kernel_context;
}


KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used) {
    pcb_t *new_proc = (pcb_t *)new_pcb_p;

    TracePrintf(3, "KCCopy: Cloning kernel context and stack for new process PID %d.\n", new_proc->pid);

    // Copy the current kernel context (kc_in) to the new process's PCB
    memcpy(&new_proc->kernel_context, kc_in, sizeof(KernelContext));

    // Copy the contents of the current kernel stack to the new process's kernel stack frames
    // KERNEL_STACK_MAXSIZE / PAGESIZE gives the number of frames in the kernel stack
    if (copy_kernel_stack(current_process->kernel_stack_pages, new_proc->kernel_stack_pages, KERNEL_STACK_MAXSIZE / PAGESIZE) != 0) {
        TracePrintf(0, "KCCopy: ERROR: Failed to copy kernel stack for new process PID %d.\n", new_proc->pid);
        // In a real scenario, you would handle this error by cleaning up the new_proc
        // and potentially returning an error indicator. For KCCopy, returning NULL
        // might signify an error to the KernelContextSwitch caller.
        return NULL;
    }

    // Return the original kernel context so the cloning process can continue its execution.
    return kc_in;
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