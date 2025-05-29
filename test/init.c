/**
 * Updated: 5/2725
 * File: init.c
 * Description: Userland init.c file for the Yalnix operating system
 */

#include <yalnix.h>
#include <hardware.h> // For constants like VMEM_0_LIMIT if needed for custom setup
#include <stdio.h> // For printf or TracePrintf for debugging

/*
 * init.c
 *
 * This is a simple default init program for the Yalnix OS.
 * As per TA feedback, it performs minimal setup (just prints info)
 * and then exits. The kernel should halt when this process exits.
 *
 * It serves as a basic demonstration that the kernel can:
 * 1. Load a user-level program.
 * 2. Pass command-line arguments to it.
 * 3. Handle the Exit() system call.
 * 4. Halt the system when the init process terminates.
 */
int main(int argc, char *argv[]) {
    // Print a message to confirm init has started and its PID
    printf("Init process (PID %d) started.\n", getpid());

    // Print the command-line arguments received by init
    printf("Init received %d arguments:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d]: \"%s\"\n", i, argv[i]);
    }

    // Indicate that init is exiting
    printf("Init process (PID %d) is now exiting.\n", getpid());

    // Exit the process. The kernel should then halt.
    return 0;

    // This line should theoretically never be reached if Exit() works correctly
    return 1;
}