/**
 * Updated: 5/2725
 * File: init.c
 * Description: Userland init.c file for the Yalnix operating system
 */

#include <yalnix.h>
#include <hardware.h> // For constants like VMEM_0_LIMIT if needed for custom setup
#include <stdio.h> // For printf or TracePrintf for debugging

// This is your main init program function
int main(int argc, char *argv[]) {
    // Print a message to indicate init has started (visible if TracePrintf is configured for user processes)
    // You might need to set up a syscall for this or use a simple loop printing to console.
    printf("Init process (PID %d) started.\n", GetPid()); // Assuming GetPid() is implemented

    // You can add any initial setup here if needed for your system.
    // For Checkpoint 3, this might be minimal.
    // Example: Print received arguments
    printf("Init received %d arguments:\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("  argv[%d]: %s\n", i, argv[i]);
    }

    // Transition to the idle state.
    printf("Init process is transitioning to idle...\n");

    // Call Exec to replace the current process with the idle program.
    // "DoIdle" here refers to the filename of your compiled idle program.
    // This assumes you have compiled a separate `doidle.c` into an executable called `DoIdle`.
    // The arguments would be empty for the idle process itself.
    Exec("DoIdle", NULL);

    // If Exec fails, this line will be reached.
    // You should handle this error, as the system cannot function without an idle process.
    printf("ERROR: Exec failed to transition to DoIdle! Halting system.\n");
    Halt(); // Assuming Halt() is a system call to stop the kernel

    // This part should technically not be reached if Exec is successful.
    return 1; // Return non-zero to indicate an error
}