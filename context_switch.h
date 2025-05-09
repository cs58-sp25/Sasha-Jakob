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
 * Switch between processes
 * Saves current kernel context, changes kernel stack, and returns next context
 * Used for context switching between processes
 *
 * @param kc_in Current kernel context
 * @param curr_pcb_p Pointer to current process PCB
 * @param next_pcb_p Pointer to next process PCB
 * @return Pointer to next process's kernel context
 */
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);


/**
 * Clone a process
 * Copies current kernel context and kernel stack to new process
 * Used when creating a new process with Fork()
 *
 * @param kc_in Current kernel context
 * @param new_pcb_p Pointer to new process PCB
 * @param not_used Unused parameter (can be NULL)
 * @return Original kernel context
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

#endif /* _CONTEXT_SWITCH_H_ */