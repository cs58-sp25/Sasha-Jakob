/**
 * Date: 5/4/25
 * File: kernel.h
 * Description: Kernel header file for Yalnix kernel
 */

#ifndef _KERNEL_H_
#define _KERNEL_H_

#include <hardware.h>
#include <pcb.h>

/**
 * KernelStart - Entry point for the Yalnix kernel
 *
 * This function is called by the hardware when the system boots.
 * It initializes all kernel subsystems and prepares to run user processes.
 *
 * @param cmd_args Command line arguments passed to the kernel
 * @param pmem_size Size of physical memory in bytes
 * @param uctxt Initial user context to be modified for first process
 */
//void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);


/**
 * init_interrupt_vector - Sets up the interrupt vector table
 *
 * Creates and initializes the table of trap handlers for all
 * system interrupts, exceptions, and traps.
 */
void init_interrupt_vector(void);



/**
 * create_idle_process - Creates the idle process
 *
 * Sets up a PCB for the idle process which runs when no
 * other processes are runnable.
 *
 * @return Pointer to idle PCB, or NULL on failure
 */
pcb_t *create_idle_process(UserContext *uctxt);

/**
 * @brief Copy kernel stack contents
 *
 * Copies the contents of the current kernel stack (identified by src_frames)
 * to a new set of kernel stack frames (identified by dst_frames).
 * This function is crucial during process cloning (Fork) to set up the new process's
 * kernel stack. It temporarily maps destination frames into the kernel's address space
 * to perform the copy, then unmaps them.
 *
 * @param src_frames An array of physical frame numbers for the source kernel stack.
 * @param dst_frames An array of physical frame numbers for the destination kernel stack.
 * @param num_frames The number of physical frames that constitute the kernel stack.
 * @return 0 on success, ERROR on failure (e.g., if temporary mapping fails).
 */
int copy_kernel_stack(int *src_frames, int *dst_frames, int num_frames);


/**
 * @brief Switch to a new process
 *
 * This is a high-level function that orchestrates a complete context switch
 * to the specified 'next' process. It involves saving the current process's
 * user context, updating global pointers, changing kernel stack mappings,
 * and finally invoking KCSwitch to perform the low-level kernel context switch.
 *
 * @param next A pointer to the PCB of the next process to run.
 * @return 0 on success, ERROR on failure.
 */
int switch_to_process(pcb_t *next);


/**
 * @brief Low-level kernel context switch function
 *
 * This function is called by KernelContextSwitch to perform the actual saving
 * and restoring of kernel contexts. It is executed on a special context/stack.
 *
 * @param kc_in The kernel context of the process that is being switched OUT.
 * @param curr_pcb_p Pointer to the PCB of the current (outgoing) process.
 * (Not used in this simplified KCSwitch, but kept for signature compliance).
 * @param next_pcb_p Pointer to the PCB of the next (incoming) process.
 * @return A pointer to the kernel context of the process that should be resumed.
 */
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);


/**
 * @brief Low-level kernel context copy function
 *
 * This function is called by KernelContextSwitch during process cloning (Fork).
 * It copies the kernel context and kernel stack contents from the current process
 * to the new process.
 *
 * @param kc_in The kernel context of the process that is being cloned (current process).
 * @param new_pcb_p Pointer to the PCB of the newly created process.
 * @param not_used Unused parameter (kept for signature compliance).
 * @return The original kernel context (kc_in), so the cloning process can continue.
 */
KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);



/**
 * DoIdle - The idle process function
 *
 * This function runs in userland when no other processes
 * are ready to run. It calls Pause() to yield the CPU.
 */
void DoIdle(void);

#endif /* _KERNEL_H_ */