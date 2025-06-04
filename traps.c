#include <yalnix.h>
#include <ykernel.h>
#include "traps.h"
#include "pcb.h"
#include "list.h"
#include "syscalls.h"

trap_handler_t trap_handlers[TRAP_VECTOR_SIZE];

void trap_init(void) {
    TracePrintf(1, "Enter trap_init.\n");
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
    TracePrintf(1, "Exit trap_init.\n");
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
    TracePrintf(1, "There has been a clock trap.\n");
    // Loops through all delayed processes, decrements their time, and puts them in the ready queue if they're done delaying
    update_delayed_processes();
    // Update the current process's run_time
    pcb_t *curr = current_process;
    curr->run_time++;
    // Check if the current process run_time == it's time slice
        // If so, change the currently schedeuled process using a KCSwitch
    if(curr->run_time > curr->time_slice){
        pcb_t *next = pcb_from_queue_node(peek(ready_queue));
        TracePrintf(1, "The process has reached it's max timeslices %d.\n", curr->time_slice);
        if(next != NULL){
            // Set the current process's UC to the given user context
            curr->user_context = cont;
            // Set the current process's status to the default
            curr->state = PROCESS_DEFAULT;
            // Add the process to the ready queue

            // Schedule another process
            next = schedule();
            if(next == NULL){
                TracePrintf(1, "ERROR, scheduling a new process has failed.\n");
                return;
            }
            // Set the uctxt to the newly scheduled processes uc
            add_to_ready_queue(curr);
            cont = next->user_context;
        }
        
    } else {
        TracePrintf(1, "The process has taken %d of %d timeslices.\n", curr->run_time, curr->time_slice);
    }

}


void illegal_handler(UserContext* cont){
    other();
    //traceprintf something based on the USerContext code value to say what went wrong
    //use exit to kill the process with code ERROR
    //send the process to the zombie queue to be waited on by the parent
}


void memory_handler(UserContext* cont) {
    int regionNumber = ((unsigned long) cont->addr) / VMEM_REGION_SIZE;
    void* relativeMemLocation = regionNumber == 1 ? cont->addr - VMEM_1_BASE : cont->addr;
    int page = (int) relativeMemLocation >> PAGESHIFT;
    // Print the offending address
    TracePrintf(0, "Memory trap: Offending address 0x%lx\n", (unsigned long)cont->addr);

    // Calculate and print the offending page number
    // Assuming PAGESHIFT is defined (e.g., 12 for 4KB pages)
    TracePrintf(0, "Memory trap: Offending page %d in region %d \n", page, regionNumber);

    if (regionNumber == 0) {
        TracePrintf(0, "Region 0 "); // omit newline, we're prepending the pte traceprint
        print_pte(region0_pt, page);
    } else if (regionNumber == 1) {
        TracePrintf(0, "Region 1 ");
        print_pte(current_process->region1_pt, page);
    }   
    // ... rest of your memory_handler logic (e.g., handling the trap, potentially halting)
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
    TracePrintf(1, "An unimplemented trap has occured.\n");
}

// Useful helper print function ------------------------------
void print_pte(pte_t pageTable[], int pte_index) {
    if (pte_index < 0 || pte_index > MAX_PT_LEN) {
        TracePrintf(0, "ERROR: Can't print pte with index %d, out of bounds!", pte_index);
        return;
    }

    pte_t pte = pageTable[pte_index];
    char r = pte.prot & PROT_READ ? 'r': '-';
    char w = pte.prot & PROT_WRITE ? 'w': '-';
    char x = pte.prot & PROT_EXEC ? 'x': '-';
    TracePrintf(0, "pte[%d]: valid: %d   pfn: %d   PROT:%c%c%c\n", pte_index, pte.valid, pte.pfn, r, w, x);
}
