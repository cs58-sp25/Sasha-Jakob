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
 * DoIdle - The idle process function
 *
 * This function runs in userland when no other processes
 * are ready to run. It calls Pause() to yield the CPU.
 */
void DoIdle(void);

#endif /* _KERNEL_H_ */