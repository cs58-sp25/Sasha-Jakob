#include "syscalls.h"

// Syscall handler table
void (*syscall_handlers[256])(UserContext *) = {

    // And the syscall code with the mask to get just the code, idk why this was set up this way in yuser.h
    // Highest syscall code though was 0xFF (YALNIX_BOOT), hence 256
    [YALNIX_FORK & YALNIX_MASK] = (int)SysUnimplemented, //SysFork,
    [YALNIX_EXEC & YALNIX_MASK] = (int)SysUnimplemented, //SysExec,
    [YALNIX_EXIT & YALNIX_MASK] = (int)SysUnimplemented, //SysExit,
    [YALNIX_WAIT & YALNIX_MASK] = (int)SysUnimplemented, //SysWait,
    [YALNIX_GETPID & YALNIX_MASK] = (int)SysGetPid,
    [YALNIX_BRK & YALNIX_MASK] = (int)SysBrk,
    [YALNIX_DELAY & YALNIX_MASK] = (int)SysDelay,
    [YALNIX_TTY_READ & YALNIX_MASK] = (int)SysUnimplemented, //SysTtyRead,
    [YALNIX_TTY_WRITE & YALNIX_MASK] = (int)SysUnimplemented, //SysTtyWrite,
    [YALNIX_PIPE_INIT & YALNIX_MASK] = (int)SysUnimplemented, // SysPipeInit,
    [YALNIX_PIPE_READ & YALNIX_MASK] = (int)SysUnimplemented, //SysPipeRead,
    [YALNIX_PIPE_WRITE & YALNIX_MASK] = (int)SysUnimplemented, //SysPipeWrite,
    [YALNIX_LOCK_INIT & YALNIX_MASK] = (int)SysUnimplemented, //SysLockInit,
    [YALNIX_LOCK_ACQUIRE & YALNIX_MASK] = (int)SysUnimplemented, //SysAcquire,
    [YALNIX_LOCK_RELEASE & YALNIX_MASK] = (int)SysUnimplemented, //SysRelease,
    [YALNIX_CVAR_INIT & YALNIX_MASK] = (int)SysUnimplemented, //SysCvarInit,
    [YALNIX_CVAR_SIGNAL & YALNIX_MASK] = (int)SysUnimplemented, //SysCvarSignal,
    [YALNIX_CVAR_BROADCAST & YALNIX_MASK] = (int)SysUnimplemented, //SysCvarBroadcast,
    [YALNIX_CVAR_WAIT & YALNIX_MASK] = (int)SysUnimplemented, //SysCvarWait,
    [YALNIX_RECLAIM & YALNIX_MASK] = (int)SysUnimplemented, //SysReclaim,
    // Add other syscall handlers here
};

void SysUnimplemented(UserContext *uctxt){
    TracePrintf(1, "The syscall %d has not yet been implemented.\n", uctxt->code & YALNIX_MASK)
    uctxt->regs[0] = error

}


// All of these will also check the registers to ensure that the values stored within make sense
// If not they will return an error.
void SysFork(UserContext *uctxt) {
    // Create a new PCB object for the new process
    // Clone the page table for region 1
    
    // (for now, cow might be implemented later) 
    // copy the data from all of userland over to new pages
    
    // KCCopy to add the kernel to the new PCB
    // Set the return value in the child context to 0
    // Set the return value in the parent context to the child's pid

    // Add the child to the ready queue
}

void SysExec(UserContext *uctxt) {
    TracePrintf(1, "Enter SysExec.\n")
    // Get the filename and args from user space
    char *filename = (char*) uctxt->regs[0];
    char **argvec = (char**) uctxt->regs[1];
    // Load the program, then context switch
    int rc = LoadProgram(filename, argvec, current_process);
    if(rc == ERROR) {
        Traceprintf(1, "ERROR, Loading the program has failed.\n");
        // Probably need to write code here to make it so a new process is loaded made to run
    }
    current_process->state = PROCESS_DEFAULT;
    add_to_ready_queue(current_process);
    pcb_t *next = schedule();
    uctxt = next->user_context;
    TracePrintf(1, "Exit SysExec.\n")
}

void SysExit(UserContext *uctxt) {
    // Set zombie to true for process
    // Save the exit status to the PCB
    // Check to see if the parent is blocked and waiting on children to exit
    // Clear all memory or mark it for reclamation
    // Add the process to the zombie queue
    // Schedule the next process

}

void SysWait(UserContext *uctxt) {
    traceprintf(1, "Enter SysWait.\n");
    // Check to see if the process has any children
        // If not return an error
    if(list_is_empty(current_process->children)){
        uctxt->regs[0] = ERROR;
        traceprintf(1, "ERROR, the process has no children to wait for.\n");
        reutrn;
    }

    // Get the status pointer
    int *status_ptr = (int *)regs[0];
    // Check to see if the process has any children currently waiting, if there are
        // grab the child's PID and exit status
    pcb_t *z_child = find_zombie_children(current_process);
    // If no children are waiting 
        // add the parent to the waiting queue
        // set it's waiting_child to true
        // Context Switch
        // When the parent is unblocked check which of it's children are done
        // Grab the child's PID and exit status
    if (z_child == NULL){
        current_process->state = PROCESS_DEFAULT;
        current_process->waiting_for_children = 1;
        add_to_blocked_queue(current_process);
        pcb_t *next = schedule();
        uctxt = next->user_context;
    }
    // Remove the child from the list of children
    else {
        remove_from_zombie_queue(z_child);
        if(status_ptr != NULL) &status_ptr = z_child->exit_status;
        UserContext->regs[0] = z_child->pid;
        free(z_child); // Need to go back and make sure terminate_process clears all other parts of memory as well. Maybe need to write a special function
    }
    // else return the the child's PID

}

void SysGetPID(UserContext *uctxt){
    TracePrintf(1, "ENTER SysGetPID.\n");
    // Input of GetPID is void so no need to check args
    // Grab the pid from the pcb in curr_process
    int pid = current_process->pid;
    // Put the pid of the current process into the correct register
    uctxt->regs[0] = (u_long) pid;
    TracePrintf(1, "EXIT SysGetPID.\n");
}

void SysBrk(UserContext *uctxt){
    // Get the addr from the user context
    unsigned int addr = ((unsigned int)) uctxt->regs[0];
    TracePrintf(1, "ENTER SysBrk. addr is %08x.\n", (unsigned int) addr);
    pcb* curr = current_process;
    unsigned int nbrk = UP_TO_PAGE(addr)>>PAGESHIFT;
    unsigned int cbrk = curr->brk>>PAGESHIFT;
    // Check to see if the address is a valid spot for the break (not above the stack or below the base of the heap)
        // If not return an error
    if  nbrk >= (DOWN_TO_PAGE(current_process->usercontext.sp)>>PAGESHIFT){
        TracePrintf(1, "ERROR, new brk is above current stack pointer.\n");
        uctxt->regs[0] = error;
        return;
    }
    // Check to see if the new brk actually has any effect on the pages (i.e. addr is in the current page below brk)
    if (nbrk == cbrk){
        TracePrintf(1, "EXIT SysBrk, new brk is the same as the old brk.\n");
        uctxt->regs[0] = 0;
        return;
    }
    // If brk is above the old break, allocate new pages
    if (nbrk > cbrk){
        TracePrintf(1, "brk is being moved from %08x up to %08x.\n", curr->brk, UP_TO_PAGE(addr));
        for(unsigned int i = cbrk; i < nbrk, i++){
            if(curr_process->region1_pt[i].valid == 0){
                int nf = allocateFreeFrame();
                if(nf == error){
                    TracePrintf(1, "ERROR, no new frames to allocate for SysBrk.\n");
                    uctxt->regs[0] = error;
                    return;
                }
                curr_process->pagetable1[i].valid = 1;
                curr_process->pagetable1[i].prot = PROT_READ | PROT_WRITE;
                curr_process->pagetable1[i].pfn = nf;
            }
        }
        TracePrintf(1, "brk has been moved from %08x up to %08x.\n", curr->brk, UP_TO_PAGE(addr));
    }
    // if brk is below the old break
    else{
        TracePrintf(1, "brk has is being moved from %08x down to %08x.\n", curr->brk, UP_TO_PAGE(addr));
        for(unsigned int i = cbrk - 1; i < nbrk, i--){
            if(curr_process->region1_pt[i].valid == 1){
                fn = curr_process->pagetable1[i].pfn;
                free_frame(fn);
                WriteRegister(REG_TLB_FLUSH, i << PAGESHIFT);
                curr_process->pagetable1[i].valid = 0;
                curr_process->pagetable1[i].prot = 0;
                curr_process->pagetable1[i].pfn = 0;
            }
        }
        TracePrintf(1, "brk has been moved from %08x down to %08x.\n", curr->brk, UP_TO_PAGE(addr));
    }
    uctxt->regs[0] = 0;
    TracePrintf(1, "EXIT SysBrk.\n");
}

void SysDelay(UserContext *uctxt){
    TracePrintf(1, "ENTER SysDelay.\n");
    // Get the delay from the context, check if the delay is invalid or 0
    int delay = (int) uctxt->regs[0];
    if(delay < 0) {
        TracePrintf(1, "ERROR, delay was negative %d\n", curr->pid, delay);
        uctxt->regs[0] = error
        return;
    }
    if(delay == 0) {
        TracePrintf(1, "EXIT SysDelay, delay was 0\n", curr->pid, delay);
        uctxt->regs[0] = 0
        return;
    }
    // Set the pcb's delaying status to true
    pcb_t *curr = current_process;
    
    // Look at the ready queue to see if another exists, currently this should just be the idle process
    // Eventually idle should be stored separately maybe and returned if no ready processes exist.
    pcb_t *next == peek(ready_queue);
    if(next != NULL){
        // Set return to 0 for when the process is done delaying
        uctxt->regs[0] = 0;
        // Set the current process's UC to the given user context
        curr->user_context = uctxt;
        // Set tHE current process's status to the default
        curr->status = PROCESS_DEFAULT;
        // Add the process to the delay queue
        add_to_delay_queue(curr, delay);

        // Schedule another process
        next = schedule();
        // Set the uctxt to the newly scheduled processes uc
        uctxt = next->user_context;
    } else {
        TracePrintf(1, "ERROR, there was no other process to put in ready.\n");
        uctxt->regs[0] = ERROR;
        return;

    }
    
    TracePrintf(1, "EXIT SysDelay, proccess %d is waiting for %d ticks.\n", curr->pid, delay);
}

void SysTtyRead(UserContext *uctxt){
    // get the terminal ID, buffer pointer, and length from the User Context
    // check if the terminal has buffered data
        // read the data from the terminal into the user-space using TtyRead
        // set the return value to the number of bytes read
        // If the number of bytes < length
            // Make the process wait on the terminal's cvar
            // Set the process to blocking
            // context switch
    // else
        // Make the process wait on the terminal's cvar
        // Set the process to blocking
        // context switch

}

void SysTtyWrite(UserContext *uctxt){
    // get the terminal ID, buffer pointer, and length from the User Context
    // check to see if the terminal is currently being written to (add a data struct for this)
        // if it is, write to the terminal with TtyWrite
        // Ser the return value to the number of bytes written
        // if the number of bytes written < len
            // Make the process wait on the terminal's cvar
            // Set the process to blocking
            // context switch
    // else
        // Make the process wait on the terminal's cvar
        // Set the process to blocking
        // context switch
        
}

void SysPipeInit(UserContext *uctxt){
    // get the pipe id from the UserContext
    // Allocate the pipe using InitPipe fomr sync
        // if it fails return error
    // copy the pipe id to user space
        // if it fails return error
    // return 0

}

void SysPipeRead(UserContext *uctxt){
    // get the pipe id, buf, and len from UserContext
    // pass these values, along with current pcb, to ReadPipe from sync
    // if it returns, return the return value of ReadPipe

}

void SysPipeWrite(UserContext *uctxt){
    // get the pipe id, buf, and len from UserContext
    // pass these values to WritePipe from sync
    // if it returns, return the return value of ReadPipe

}

void SysLockInit(UserContext *uctxt){
    // Get the int *lock_id from the UserContext
    // pass the values to InitLock from sync.c
    // return the return value of InitLock

}

void SysLockAcquire(UserContext *uctxt){
    // Get the int lock_id from the UserContext
    // pass the values to Acquire from sync.c
    // return the return value of Acquire

}

void SysLockRelease(UserContext *uctxt){
    // Get the int lock_id from the UserContext
    // pass the values to Release from sync.c
    // return the return value of release

}

void SysCvarInit(UserContext *uctxt){
    // Get the int *cvar_id from the UserContext
    // pass the values to InitCvar from sync.c
    // return the return value of InitCvar

}

void SysSignal(UserContext *uctxt){
    // Get the int cvar_id from the UserContext
    // pass the values to CvarSignal from sync.c
    // return the return value of CvarSignal

}

void SysBroadcast(UserContext *uctxt){
    // Get the int cvar_id from the UserContext
    // pass the values to CvarBroadcast from sync.c
    // return the return value of CvarBroadcast

}

void SysCvarWait(UserContext *uctxt){
    // Get the int cvar_id and int lock_id from the UserContext
    // pass the values to CvarWait from sync.c
    // return the return value of CvarWait

}

void SysReclaim(UserContext *uctxt){
    // Get the sync object id from the UserContext
    // pass the values to ReclaimSync from sync.c
    // return the return value of release

}

pcb_t *schedule(){
    pcb_t *curr = current_process;
    pcb_t *next = pop(ready_queue);
    KernelContext kc = KernelContextSwitch(KCSwitch, (void *) curr, (void *) next);
    //current_process should already be put into a different queue at this point
    next->status = PROCESS_RUNNING;
    next->run_time = 0;
    current_process = next;
    return next;
}
