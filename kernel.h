/**
 * Date: 5/4/25
 * File: kernel.h
 * Description: Kernel header file for Yalnix kernel
 */

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
pcb_t *create_idle_process(UserContext *uctxt)


/**
 * create_init_process - Creates the init process
 *
 * Sets up a PCB for the init process which is the
 * first user process in the system.
 *
 * @param name Program name for init
 * @param args Arguments to pass to init
 * @return Pointer to init PCB, or NULL on failure
 */
pcb_t *create_init_process(char *name, char **args);


/**
 * setup_idle_context - Configures user context for idle process
 *
 * Sets PC to DoIdle function and sets up stack pointer.
 *
 * @param idle_pcb PCB for idle process
 * @return 0 on success, ERROR on failure
 */
int setup_idle_context(pcb_t *idle_pcb);


/**
 * load_init_program - Loads the init program into memory
 *
 * Uses LoadProgram to set up init process Region 1 memory.
 *
 * @param init_pcb PCB for init process
 * @param filename Name of init program file
 * @param args Arguments to pass to init
 * @return 0 on success, ERROR on failure
 */
int load_init_program(pcb_t *init_pcb, char *filename, char **args);


/**
 * init_lists - Initialize all global data structures
 *
 * Sets up ready queue, blocked queue, and other global lists.
 */
void init_lists(void);


/**
 * DoIdle - The idle process function
 *
 * This function runs in userland when no other processes
 * are ready to run. It calls Pause() to yield the CPU.
 */
void DoIdle(void);