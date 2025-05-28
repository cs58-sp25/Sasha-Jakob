/**
 * Date: 5/4/25
 * File: context_switch.h
 * Description: Context switching header file for Yalnix OS
 */

#ifndef _CONTEXT_SWITCH_H_
#define _CONTEXT_SWITCH_H_

#include <hardware.h>
#include "kernel.h"

/**
 * Function type for kernel context switch functions
 * Used with KernelContextSwitch
 */
typedef KernelContext *(*kcs_func_t)(KernelContext *, void *, void *);


/**
 * @brief Performs a low-level kernel context switch.
 *
 * This function is called by KernelContextSwitch to perform the actual context
 * saving and restoring. It is executed on a special, independent stack.
 *
 * @param kc_in A pointer to a temporary copy of the current kernel context
 * of the process that is being switched OUT.
 * @param curr_pcb_p A void pointer to the PCB of the current (outgoing) process.
 * This parameter is not used in this simplified implementation,
 * as the saving of the outgoing context is assumed to be handled
 * by the caller (e.g., switch_to_process).
 * @param next_pcb_p A void pointer to the PCB of the next (incoming) process.
 *
 * @return A pointer to the kernel context of the process that should resume execution.
 * This will be the kernel context of 'next_proc'.
 */
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);


/**
 * @brief Clones the current kernel context and stack for a new process.
 *
 * This function is called by KernelContextSwitch when a new process is being
 * created (e.g., during a Fork syscall). It copies the kernel context and
 * kernel stack contents from the current process to the new process.
 *
 * @param kc_in A pointer to a temporary copy of the current kernel context
 * of the process that is initiating the copy (the parent).
 * @param new_pcb_p A void pointer to the PCB of the new (child) process.
 * @param not_used This parameter is not used in KCCopy.
 *
 * @return A pointer to the original kernel context (kc_in). This ensures the
 * parent process continues its execution after the cloning operation.
 * Returns NULL on failure to copy the kernel stack.
 */
KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);


/**
 * Save the current user context to the PCB
 * Called during trap handling
 *
 * @param uctxt User context from trap
 * @param proc Process to save context to
 */
void save_user_context(UserContext *uctxt, pcb_t *proc);


/**
 * Restore user context from PCB
 * Called before returning from trap
 *
 * @param uctxt User context destination
 * @param proc Process to restore context from
 */
void restore_user_context(UserContext *uctxt, pcb_t *proc);


/**
 * Copy kernel stack contents
 * Copies the current kernel stack to new kernel stack frames
 *
 * @param src_frames Source stack frame array
 * @param dst_frames Destination stack frame array
 * @param num_frames Number of frames in kernel stack
 * @return 0 on success, ERROR on failure
 */
int copy_kernel_stack(int *src_frames, int *dst_frames, int num_frames);


/**
 * Switch to a new process
 * High-level function that handles complete context switch
 *
 * @param next Next process to run
 * @return 0 on success, ERROR on failure
 */
int switch_to_process(pcb_t *next);


/**
 * Dispatch the next ready process
 * Selects the next process from ready queue and switches to it
 *
 * @return 0 on success, ERROR on failure
 */
int dispatch_next_process(void);


/**
 * Initialize kernel context in PCB
 * Sets up initial kernel context for a new process
 *
 * @param proc Process PCB to initialize
 * @return 0 on success, ERROR on failure
 */
int init_kernel_context(pcb_t *proc);


/**
 * Initialize user context in PCB
 * Sets up initial user context for a new process
 *
 * @param proc Process PCB to initialize
 * @param entry_point Program entry point address
 * @param stack_pointer Initial stack pointer
 * @return 0 on success, ERROR on failure
 */
int init_user_context(pcb_t *proc, void *entry_point, void *stack_pointer);


/**
 * Setup temporary mapping for kernel stack
 * Creates a temporary mapping to access a kernel stack frame
 *
 * @param frame Physical frame to map
 * @return Virtual address of temporary mapping, or NULL on failure
 */
void *setup_temp_mapping(int frame);


/**
 * Remove temporary mapping
 * Removes a temporary mapping created by setup_temp_mapping
 *
 * @param addr Virtual address to unmap
 */
void remove_temp_mapping(void *addr);


int map_kernel_stack(int *kernel_stack_frames);
#endif /* _CONTEXT_SWITCH_H_ */
