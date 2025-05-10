/**
 * Date: 5/4/25
 * File: context_switch.c
 * Description: Context switching implementation for Yalnix OS
 */

#include "context_switch.h"
#include <hardware.h>
#include "kernel.h"
#include "memory.h"


KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p) {
    // Cast void pointers to PCB pointers
    // Save current kernel context in current PCB
    // Update current process pointer to next process
    // Update kernel stack pointer to next process's kernel stack
    // Load next process's page tables:
    //   - Set REG_PTBR1 to next process's Region 1 page table
    //   - Flush TLB for Region 1
    // Return next process's saved kernel context
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used) {
    // Cast void pointers to PCB pointers
    // Allocate kernel stack frames for new process
    // Copy contents of current kernel stack to new kernel stack
    // Set up new PCB's kernel context based on current context
    // Adjust any context-specific pointers in the new context
    // Return original kernel context (for parent process)
}

void save_user_context(UserContext *uctxt, pcb_t *proc) {
    // Copy all fields from uctxt to proc->user_context
    // Save user registers, PC, stack pointer, etc.
}

void restore_user_context(UserContext *uctxt, pcb_t *proc) {
    // Copy all fields from proc->user_context to uctxt
    // Restore user registers, PC, stack pointer, etc.
}

int copy_kernel_stack(int *src_frames, int *dst_frames, int num_frames) {
    // For each frame in kernel stack:
    //   Create temporary mappings for source and destination frames
    //   Copy contents from source to destination frame
    //   Remove temporary mappings
    // Return 0 on success, ERROR on failure
}

int switch_to_process(pcb_t *next) {
    // If no next process, return error
    // Save current process (if any)
    // Call KernelContextSwitch with KCSwitch and appropriate parameters
    // Update global current_process to next
    // Return 0 on success
}

int dispatch_next_process(void) {
    // Remove first process from ready queue
    // If no process in ready queue, switch to idle process
    // Otherwise, switch to the selected process
    // Return result of switch_to_process
}

int init_kernel_context(pcb_t *proc) {
    // Allocate space for kernel context in PCB if needed
    // Initialize kernel context fields to appropriate values:
    //   - Set initial register values
    //   - Set kernel stack pointer to top of kernel stack
    //   - Other context initialization as needed
    // Return 0 on success, ERROR on failure
}

int init_user_context(pcb_t *proc, void *entry_point, void *stack_pointer) {
    // Allocate space for user context in PCB if needed
    // Initialize user context fields:
    //   - Set PC to entry_point
    //   - Set SP to stack_pointer
    //   - Clear general purpose registers
    //   - Set initial PSR value
    // Return 0 on success, ERROR on failure
}

void *setup_temp_mapping(int frame) {
    // Calculate a virtual page number in kernel temporary mapping area
    // Create mapping from virtual page to physical frame
    // Return virtual address corresponding to the mapping
    // Return NULL if mapping fails
}

void remove_temp_mapping(void *addr) {
    // Calculate virtual page number from address
    // Remove mapping for that virtual page
    // Flush TLB entry if VM is enabled
}