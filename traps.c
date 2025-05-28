#include <yalnix.h>
#include <ykernel.h>
#include "traps.h"
#include "pcb.h"
#include "list.h"
#include "syscalls.h"

trap_handler_t trap_handlers[TRAP_VECTOR_SIZE];

void trap_init(void) {
    // Each entry contains the address of the function to handle that specific trap
    trap_handlers[TRAP_KERNEL] = kernel_handler;        // System calls from user processes
    trap_handlers[TRAP_CLOCK] = clock_handler;          // Timer interrupts for scheduling
    trap_handlers[TRAP_ILLEGAL] = illegal_handler;      // Illegal instruction exceptions
    trap_handlers[TRAP_MEMORY] = memory_handler;        // Memory access violations and stack growth
    trap_handlers[TRAP_MATH] = math_handler;            // Math errors like division by zero
    trap_handlers[TRAP_TTY_RECEIVE] = receive_handler;   // Terminal input available
    trap_handlers[TRAP_TTY_TRANSMIT] = transmit_handler; // Terminal output complete

    // Write the address of vector table to REG_VECTOR_BASE register
    WriteRegister(REG_VECTOR_BASE, (unsigned int)trap_handlers);
    TracePrintf(0, "Interrupt vector table initialized at 0x%p\n", trap_handlers);
}

void kernel_handler(UserContext* cont){
    TracePrintf(1, "Enter Kernel_handler.\n");
    other();
    int ind = cont->code && YALNIX_MASK;
    TracePrintf(1, "Syscall with code %d is being called.\n", ind);
    if (ind >= 0 && ind < 256 && syscall_handlers[ind] != NULL){ 
        // If the syscall exists call it
        syscall_handlers[ind](cont);
    } else {
        // Otherwise return an error to the User
        cont->regs[0] = ERROR;
    }
}

void clock_handler(UserContext* cont){
    TracePrintf(1, "There has been a clock trap");
    // Loops through all delayed processes, decrements their time, and puts them in the ready queue if they're done delaying
    update_delayed_processes();
    // Update the current process's run_time
    pcb_t *curr = current_process;
    curr->run_time++;
    // Check if the current process run_time == it's time slice
        // If so, change the currently schedeuled process using a KCSwitch
    if(curr->run_time > curr->time_slice){
        pcb_t *next = pcb_from_queue_node(peek(ready_queue));
        if(next != NULL){
            // Set the current process's UC to the given user context
            curr->user_context = cont;
            // Set the current process's status to the default
            curr->state = PROCESS_DEFAULT;
            // Add the process to the ready queue
            add_to_ready_queue(curr);

            // Schedule another process
            next = schedule();
            // Set the uctxt to the newly scheduled processes uc
            cont = next->user_context;
        }
        
    }

}


void illegal_handler(UserContext* cont){
    other();
    //traceprintf something based on the USerContext code value to say what went wrong
    //use exit to kill the process with code ERROR
    //send the process to the zombie queue to be waited on by the parent
}

void memory_handler(UserContext* cont){
    other();
    //check to see if addr is between the bottom of the user stack and the redzone
        //if it is drop the UserStack and carry on with life
    //else 
        //traceprintf something based on the UserContext code value to say what went wrong (or if the next stack address goes into the redzone/heap)
        //use exit to kill the process with code ERROR
        //send the process to the zombie queue to be waited on by the parent
        //return null in this case
}

void math_handler(UserContext* cont){
    other();
    //traceprintf something based on the USerContext code value to say what went wrong
    //use exit to kill the process with code ERROR
    //send the process to the zombie queue to be waited on by the parent
}

void receive_handler(UserContext* cont){
    other();
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
    other();
    // Determine the terminal which generated the interrupt from code
    // Make the terminal no longer busy

    // Unblock the process that initiated started the completed TtyWrite (if any)
        // Set the return value for TtyWrite (e.g., number of bytes written)
        // Move the process to the ready queue

    // Check if there's more data queued for transmission on this terminal
        // Start transmitting the next line (non-blocking call)
        // Set terminal busy again
}

static void other(void){
    TracePrintf(1, "An unimplemented trap has occured");
}
