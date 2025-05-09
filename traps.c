#include "traps.h"


void kernel_handler(UserContext* cont){

    int ind = cont->code & YALNIX_MASK;
    TracePrintf(1, "Userspace is trying to use the syscall with code %x.\n", ind);
    // Eventually this will be helpful
    // if (ind >= 0 && ind < 256 && syscall_handlers[ind] != NULL){ 
    //     // If the syscall exists call it
    //     syscall_handlers[index](context);
    // } else {
    //     // Otherwise return an error to the User
    //     UserContext->regs[0] = ERROR
    // }
}

void clock_handler(UserContext* cont){
    TracePrintf(1, "There has been a clock trap");
    // Loop through each of the blocked processes
        // For each one, if it is delaying, decrement the delay counter
        // If the delay counter is at 0 move it back to the ready queue
    // Update the current process's run_time
    // Check if the current process run_time == it's time slice
        // If so, change the currently schedeuled process using a KCSwitch
    // Have processes waiting on other processes to complete check if that process is in zombie pcb
        // If the process it's waiting on is in zombies give it the exit code in reg[0] and put it in the ready queue
}

void other(void){
    TracePrintf(1, "An unimplemented trap has occured");
}

void illegal_handler(UserContext* cont){
    other(void);
    //traceprintf something based on the USerContext code value to say what went wrong
    //use exit to kill the process with code ERROR
    //send the process to the zombie queue to be waited on by the parent
}

void memory_handler(UserContext* cont){
    other(void);
    //check to see if addr is between the bottom of the user stack and the redzone
        //if it is drop the UserStack and carry on with life
    //else 
        //traceprintf something based on the UserContext code value to say what went wrong (or if the next stack address goes into the redzone/heap)
        //use exit to kill the process with code ERROR
        //send the process to the zombie queue to be waited on by the parent
        //return null in this case
}

void math_handler(UserContext* cont){
    other(void);
    //traceprintf something based on the USerContext code value to say what went wrong
    //use exit to kill the process with code ERROR
    //send the process to the zombie queue to be waited on by the parent
}

void receive_handler(UserContext* cont){
    other(void);
    // Determine the terminal which generated the interrupt from code
    // Create a buffer for the incoming line (or reuse a static one)
    // Read the line of input from the terminal hardware

    // Save the input line into the kernel's terminal buffer for that terminal

    // Check if any process is blocked waiting on a TtyRead for this terminal
        // Get the first waiting process
        // Copy data from buffer to user-space (e.g., using memcpy to process->user_buffer)
        // Set return value (e.g., in process->user_context->regs[0])
        // Move the process to the ready queue
}

void transmit_handler(UserContext* cont){
    other(void);
    // Determine the terminal which generated the interrupt from code
    // Make the terminal no longer busy

    // Unblock the process that initiated started the completed TtyWrite (if any)
        // Set the return value for TtyWrite (e.g., number of bytes written)
        // Move the process to the ready queue

    // Check if there's more data queued for transmission on this terminal
        // Start transmitting the next line (non-blocking call)
        // Set terminal busy again
}