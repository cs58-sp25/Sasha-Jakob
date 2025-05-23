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
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);


/**
 * @brief Creates a new process and initializes its PCB.
 *
 * This function allocates a new PCB, sets up its initial state, and prepares
 * the user context for the new process. It also handles the allocation of
 * kernel stack frames and Region 1 page table entries.
 *
 * @param uctxt A pointer to the UserContext to be used for the new process.
 * @return A pointer to the newly created process's PCB on success, or NULL on failure.
 */
pcb_t *create_process(UserContext *uctxt);


/**
 * @brief Creates and initializes the Process Control Block (PCB) for the idle process.
 *
 * This function sets up the idle process's PCB, including its Region 1 page table
 * (initially mapping only the user stack), its kernel stack frames, and its UserContext
 * to start executing the DoIdle function.
 *
 * @param uctxt A pointer to the initial UserContext provided by KernelStart.
 * This context is copied and then modified for the idle process.
 * @return A pointer to the newly created idle process's PCB on success, or NULL on failure.
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