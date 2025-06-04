/**
 * Date: 5/4/25
 * File: context_switch.h
 * Description: Context switching header file for Yalnix OS
 */

#ifndef _CONTEXT_SWITCH_H_
#define _CONTEXT_SWITCH_H_

#include <hardware.h>
#include "kernel.h"

#define TEMP_MAPPING_VADDR (KERNEL_STACK_BASE - PAGESIZE)
#define KSTACK_START_PAGE (KERNEL_STACK_BASE >> PAGESHIFT)
#define NUM_PAGES_REGION1 (VMEM_1_SIZE / PAGESIZE)

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


void CopyPageTable(pcb_t *parent, pcb_t *child);


/**
 * Setup temporary mapping for kernel stack
 * Creates a temporary mapping to access a kernel stack frame
 *
 * @param frame Physical frame to map
 * @return Virtual address of temporary mapping, or NULL on failure
 */
void setup_temp_mapping(int frame);


/**
 * Remove temporary mapping
 * Removes a temporary mapping created by setup_temp_mapping
 *
 * @param addr Virtual address to unmap
 */
void remove_temp_mapping(int frame);


/**
 * Map the kernel stack for a process
 * Maps the kernel stack pages into the Region 0 page table
 *
 * @param kernel_stack_pt Pointer to the kernel stack's page table
 * @return 0 on success, -1 on failure
 */
int map_kernel_stack(pte_t *kernel_stack_pt);


#endif /* _CONTEXT_SWITCH_H_ */
