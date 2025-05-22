/**
 * Date: 5/4/25
 * File: context_switch.c
 * Description: Context switching implementation for Yalnix OS
 */

#include "context_switch.h"
#include <hardware.h>
#include "kernel.h"
#include "list.h" // Assuming list.h contains pcb_t definition or is included by kernel.h
#include "memory.h" // Assuming memory.h contains allocate_frame, free_frame, map_kernel_stack, setup_temp_mapping, remove_temp_mapping

// Global variable to keep track of the currently running process.
// This is defined in kernel.c, but declared as extern here for use.
extern pcb_t *current_process;


KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    // Cast the void pointer to a pcb_t pointer for the next process.
    pcb_t *next_proc = (pcb_t *)next_pcb_p;

    // TracePrintf for debugging, indicating the process being switched to.
    TracePrintf(3, "KCSwitch: Performing low-level kernel context switch to PID %d.\n", next_proc->pid);

    // In a full implementation, if 'curr_pcb_p' were used, the 'kc_in'
    // would be copied into 'curr_proc->kernel_context' here.
    // For this simplified version, we assume the caller of KernelContextSwitch
    // (e.g., switch_to_process) has already saved the necessary state of the
    // outgoing process or that the KernelContextSwitch mechanism implicitly handles it.

    // Return the kernel context of the next process. This context will be loaded
    // by the KernelContextSwitch hardware support function, allowing 'next_proc'
    // to resume execution from where it was last suspended.
    return &next_proc->kernel_context;
}


KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used) {
    // Cast the void pointer to a pcb_t pointer for the new process.
    pcb_t *new_proc = (pcb_t *)new_pcb_p;

    // TracePrintf for debugging, indicating the new process being cloned.
    TracePrintf(3, "KCCopy: Cloning kernel context and stack for new process PID %d.\n", new_proc->pid);

    // Copy the contents of the current kernel context (kc_in) to the new process's PCB.
    // This effectively gives the new process an identical kernel execution state as the parent
    // at the point of the KCCopy call.
    memcpy(&new_proc->kernel_context, kc_in, sizeof(KernelContext));

    // Copy the contents of the current kernel stack to the new process's kernel stack frames.
    // KERNEL_STACK_MAXSIZE / PAGESIZE calculates the number of physical frames
    // that comprise the kernel stack. This ensures the new process has a copy
    // of the parent's kernel stack contents.
    if (copy_kernel_stack(current_process->kernel_stack_pages, new_proc->kernel_stack_pages, KERNEL_STACK_MAXSIZE / PAGESIZE) != 0) {
        // If copying the kernel stack fails, print an error and return NULL.
        // In a more robust system, proper cleanup of partially allocated resources
        // for 'new_proc' would be necessary here.
        TracePrintf(0, "KCCopy: ERROR: Failed to copy kernel stack for new process PID %d.\n", new_proc->pid);
        return NULL;
    }

    // Return the original kernel context (kc_in). This allows the parent process
    // (the one that called KernelContextSwitch with KCCopy) to continue its
    // execution from where it left off, as if nothing happened beyond a function call.
    return kc_in;
}

// Placeholder for other functions that would typically be in context_switch.c
// These would be implemented as part of the overall kernel design.

/**
 * @brief Saves the user context from the hardware into the process's PCB.
 * @param uctxt The UserContext provided by the hardware trap.
 * @param proc The PCB of the current process.
 */
void save_user_context(UserContext *uctxt, pcb_t *proc) {
    // Copy all fields from uctxt to proc->user_context
    memcpy(&proc->user_context, uctxt, sizeof(UserContext));
    TracePrintf(4, "save_user_context: User context saved for PID %d.\n", proc->pid);
}

/**
 * @brief Restores the user context from the process's PCB to the hardware's UserContext structure.
 * @param uctxt The UserContext structure to be populated for hardware restoration.
 * @param proc The PCB of the process whose context is to be restored.
 */
void restore_user_context(UserContext *uctxt, pcb_t *proc) {
    // Copy all fields from proc->user_context to uctxt
    memcpy(uctxt, &proc->user_context, sizeof(UserContext));
    TracePrintf(4, "restore_user_context: User context restored for PID %d.\n", proc->pid);
}

/**
 * @brief Copies the contents of physical frames used by one kernel stack to another.
 * @param src_frames Array of physical frame numbers for the source kernel stack.
 * @param dst_frames Array of physical frame numbers for the destination kernel stack.
 * @param num_frames The number of frames in the kernel stack.
 * @return 0 on success, ERROR on failure.
 */
int copy_kernel_stack(int *src_frames, int *dst_frames, int num_frames) {
    TracePrintf(3, "copy_kernel_stack: Copying %d frames from source to destination kernel stack.\n", num_frames);

    // Loop through each frame of the kernel stack
    for (int i = 0; i < num_frames; i++) {
        // Calculate the virtual address of the source kernel stack frame in Region 0.
        // This assumes the kernel stack is always mapped at KERNEL_STACK_BASE in Region 0.
        void *src_vaddr = (void *)(KERNEL_STACK_BASE + (i * PAGESIZE));

        // Temporarily map the destination physical frame into the kernel's address space
        // to allow copying its contents. The setup_temp_mapping function would
        // typically use a temporary virtual page in Region 0 that is not currently used.
        void *dst_temp_vaddr = setup_temp_mapping(dst_frames[i]);

        if (dst_temp_vaddr == NULL) {
            TracePrintf(0, "copy_kernel_stack: ERROR: Failed to set up temporary mapping for destination frame %d.\n", dst_frames[i]);
            // In a real system, you might need to clean up any mappings already made
            // in this loop if an error occurs.
            return ERROR;
        }

        // Copy the contents of the source frame (accessible via its virtual address)
        // to the temporarily mapped destination frame.
        memcpy(dst_temp_vaddr, src_vaddr, PAGESIZE);

        // Remove the temporary mapping for the destination frame after copying.
        // This frees up the temporary virtual page.
        remove_temp_mapping(dst_temp_vaddr);
    }

    TracePrintf(3, "copy_kernel_stack: Kernel stack copy completed successfully.\n");
    return 0; // Success
}

/**
 * @brief Initiates a context switch to the specified process.
 *
 * This function prepares the system for a context switch by updating global
 * process pointers, hardware registers (PTBR1, PTLR1), and flushing the TLB,
 * then invokes the low-level KernelContextSwitch.
 *
 * @param next A pointer to the PCB of the process to switch to.
 * @return 0 on success, ERROR on failure.
 */
int switch_to_process(pcb_t *next) {
    TracePrintf(2, "switch_to_process: Initiating context switch from PID %d to PID %d.\n",
                current_process ? current_process->pid : -1, next->pid);

    // Update the global 'current_process' pointer to the new process.
    // This is crucial for subsequent kernel operations to correctly identify the active process.
    current_process = next;
    TracePrintf(4, "switch_to_process: Current process updated to PID %d.\n", current_process->pid);

    // Update the hardware's Region 1 Page Table Base Register (REG_PTBR1)
    // to point to the new process's Region 1 page table. This changes the
    // user-mode virtual address space.
    WriteRegister(REG_PTBR1, (unsigned int)current_process->region1_pt);
    // Update the hardware's Region 1 Page Table Limit Register (REG_PTLR1).
    // This defines the size of Region 1.
    WriteRegister(REG_PTLR1, (unsigned int)((VMEM_REGION_SIZE - VMEM_0_LIMIT) / PAGESIZE));

    // Map the kernel stack of the new process into Region 0.
    // This involves updating the Region 0 page table entries that correspond
    // to the kernel stack area to point to the physical frames of the new process's
    // kernel stack.
    if (map_kernel_stack(current_process->kernel_stack_pages) != 0) {
        TracePrintf(0, "switch_to_process: ERROR: Failed to map kernel stack for PID %d.\n", current_process->pid);
        return ERROR;
    }

    // Flush the Translation Lookaside Buffer (TLB) for Region 1 and the kernel stack.
    // This ensures that any stale cached translations are invalidated and the
    // hardware uses the newly updated page table entries.
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

    // Perform the low-level kernel context switch.
    // KernelContextSwitch will save the current kernel context (of the process
    // that called switch_to_process) and load the kernel context of 'next'.
    // The 'NULL' for curr_pcb_p is a placeholder; in some designs, the outgoing
    // PCB might be passed here for saving its context.
    // When this call returns, it means 'current_process' (which is now 'next')
    // has been scheduled back in.
    int kc_returned = KernelContextSwitch(KCSwitch, NULL, (void *)next);

    TracePrintf(2, "switch_to_process: Returned from KCSwitch. Process PID %d resumed.\n", current_process->pid);

    return 0; // Success
}

/**
 * @brief Dispatches the next process from the ready queue.
 *
 * This function is responsible for selecting the next process to run.
 * If the ready queue is empty, it switches to the idle process.
 *
 * @return The result of the switch_to_process call (0 on success, ERROR on failure).
 */
int dispatch_next_process(void) {
    // This is a placeholder. A real implementation would involve:
    // 1. Dequeueing the next process from a global ready queue.
    // 2. If the queue is empty, setting 'next' to the idle process.
    // 3. Calling switch_to_process with the selected 'next' process.
    TracePrintf(1, "dispatch_next_process: (Not yet implemented - placeholder)\n");
    // For now, returning ERROR or similar to indicate unhandled state.
    // This function would typically call switch_to_process.
    return ERROR;
}

/**
 * @brief Initializes the kernel context for a new process.
 * @param proc The PCB of the new process.
 * @return 0 on success, ERROR on failure.
 */
int init_kernel_context(pcb_t *proc) {
    // This is a placeholder. A real implementation would:
    // 1. Allocate memory for the kernel context if it's not part of the PCB struct.
    // 2. Initialize the KernelContext structure (e.g., set initial stack pointer,
    //    program counter for kernel entry, and other register values).
    //    For a new process, the kernel context would typically be set up to
    //    return to a user-mode entry point after the first context switch.
    TracePrintf(1, "init_kernel_context: (Not yet implemented - placeholder)\n");
    return 0; // Assuming success for now
}

/**
 * @brief Initializes the user context for a new process.
 * @param proc The PCB of the new process.
 * @param entry_point The virtual address of the program's entry point.
 * @param stack_pointer The virtual address of the top of the user stack.
 * @return 0 on success, ERROR on failure.
 */
int init_user_context(pcb_t *proc, void *entry_point, void *stack_pointer) {
    // This is a placeholder. A real implementation would:
    // 1. Initialize proc->user_context fields.
    // 2. Set proc->user_context.pc to entry_point.
    // 3. Set proc->user_context.sp to stack_pointer.
    // 4. Clear general purpose registers (proc->user_context.regs).
    // 5. Set initial Processor Status Register (PSR) value if applicable.
    TracePrintf(1, "init_user_context: (Not yet implemented - placeholder)\n");
    proc->user_context.pc = entry_point;
    proc->user_context.sp = stack_pointer;
    // Clear general purpose registers
    for (int i = 0; i < GREGS; i++) {
        proc->user_context.regs[i] = 0;
    }
    return 0; // Assuming success for now
}

/**
 * @brief Sets up a temporary mapping in the kernel's address space for a given physical frame.
 * @param frame The physical frame number to map.
 * @return The virtual address of the temporary mapping, or NULL on failure.
 */
void *setup_temp_mapping(int frame) {
    // This is a placeholder. A real implementation would:
    // 1. Find an unused virtual page in Region 0 for temporary mapping.
    // 2. Create a PTE for this virtual page, mapping it to 'frame' with appropriate permissions.
    // 3. Flush the TLB for this specific virtual page.
    // 4. Return the calculated virtual address.
    TracePrintf(1, "setup_temp_mapping: (Not yet implemented - placeholder for frame %d)\n", frame);
    // For demonstration, we'll return a dummy address. In a real system, this needs a valid
    // temporary mapping mechanism.
    // A common approach is to reserve a small, fixed area in Region 0 for temporary mappings.
    // For now, we'll assume a direct mapping for simplicity, which is NOT how it works in a real kernel.
    // This needs to be replaced with actual page table manipulation.
    return (void *)(frame * PAGESIZE); // DUMMY: This assumes direct physical to virtual mapping, which is incorrect for temporary mappings.
}

/**
 * @brief Removes a temporary mapping from the kernel's address space.
 * @param addr The virtual address of the temporary mapping to remove.
 */
void remove_temp_mapping(void *addr) {
    // This is a placeholder. A real implementation would:
    // 1. Determine the virtual page number from 'addr'.
    // 2. Invalidate the PTE for that virtual page in the Region 0 page table.
    // 3. Flush the TLB entry for that virtual page.
    TracePrintf(1, "remove_temp_mapping: (Not yet implemented - placeholder for address %p)\n", addr);
    // DUMMY: No actual unmapping here.
}

/**
 * @brief Maps the kernel stack of the current process into Region 0.
 *
 * This function is called during a context switch to ensure that the kernel
 * stack of the incoming process is correctly mapped into the fixed kernel
 * stack virtual address range in Region 0.
 *
 * @param kernel_stack_frames An array of physical frame numbers that constitute
 * the kernel stack of the process.
 * @return 0 on success, ERROR on failure.
 */
int map_kernel_stack(int *kernel_stack_frames) {
    // This is a placeholder. A real implementation would:
    // 1. Iterate through the virtual pages corresponding to KERNEL_STACK_BASE
    //    to KERNEL_STACK_LIMIT.
    // 2. For each virtual page, update its PTE in the Region 0 page table
    //    to point to the corresponding physical frame from 'kernel_stack_frames'.
    // 3. Ensure appropriate permissions (PROT_READ | PROT_WRITE) are set.
    TracePrintf(1, "map_kernel_stack: (Not yet implemented - placeholder)\n");
    return 0; // Assuming success for now
}
