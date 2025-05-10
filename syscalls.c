#include "syscalls.h"

// Syscall handler table
void (*syscall_handlers[256])(UserContext *) = {

    // And the syscall code with the mask to get just the code, idk why this was set up this way in yuser.h
    // Highest syscall code though was 0xFF (YALNIX_BOOT), hence 256
    [YALNIX_FORK & YALNIX_MASK] = SysFork,
    [YALNIX_EXEC & YALNIX_MASK] = SysExec,
    [YALNIX_EXIT & YALNIX_MASK] = SysExit,
    [YALNIX_WAIT & YALNIX_MASK] = SysWait,
    [YALNIX_GETPID & YALNIX_MASK] = SysGetPid,
    [YALNIX_BRK & YALNIX_MASK] = SysBrk,
    [YALNIX_DELAY & YALNIX_MASK] = SysDelay,
    [YALNIX_TTY_READ & YALNIX_MASK] = SysTtyRead,
    [YALNIX_TTY_WRITE & YALNIX_MASK] = SysTtyWrite,
    [YALNIX_PIPE_INIT & YALNIX_MASK] = SysPipeInit,
    [YALNIX_PIPE_READ & YALNIX_MASK] = SysPipeRead,
    [YALNIX_PIPE_WRITE & YALNIX_MASK] = SysPipeWrite,
    [YALNIX_LOCK_INIT & YALNIX_MASK] = SysLockInit,
    [YALNIX_LOCK_ACQUIRE & YALNIX_MASK] = SysAcquire,
    [YALNIX_LOCK_RELEASE & YALNIX_MASK] = SysRelease,
    [YALNIX_CVAR_INIT & YALNIX_MASK] = SysCvarInit,
    [YALNIX_CVAR_SIGNAL & YALNIX_MASK] = SysCvarSignal,
    [YALNIX_CVAR_BROADCAST & YALNIX_MASK] = SysCvarBroadcast,
    [YALNIX_CVAR_WAIT & YALNIX_MASK] = SysCvarWait,
    [YALNIX_RECLAIM & YALNIX_MASK] = SysReclaim,
    // Add other syscall handlers here
};


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
    // Get the filename and args from user space
    // Load the program, likely requires a function
    // Get the current process's pcb
    // Reset the pcb's user context and pcb parts
    // Reset the program's memory, loading in the program code simultaneously
    // Setup the page table for the process
    // Reset the user context
    // Setup the PCBs stack and arguments
    // Context switch

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
    // Get the filename and argvec from the UserContext
    // Check to see if the process has any children
        // If not return an error
    // Check to see if the process has any children currently waiting, if there are
        // grab the child's PID and exit status
    // If no children are waiting 
        // add the parent to the waiting queue
        // set it's waiting_child to true
        // Context Switch
        // When the parent is unblocked check which of it's children are done
        // Grab the child's PID and exit status
    // Remove the child from the list of children
    // if the exit status was not succes
        // return the exit status
    // else return the the child's PID

}

void SysGetPID(UserContext *uctxt){
    // Check the current process' PCB and return the PID stored within
}

void SysBrk(UserContext *uctxt){
    // Get the addr from the user context
    // Check to see if the address is a valid spot for the break (not above the stack or below the base of the heap)
        // If not return an error
    // Check to see if the new brk actually has any effect on the pages (i.e. addr is in the current page below brk)
        // if not return 0
    // If brk is above the old break, allocate new pages
        // If this fails (no more memory) return ERROR
    // if brk is below the old break
        // Mark the pages being freed for reclamation or delete them
    // return 0

}

void SysDelay(UserContext *uctxt){
    // Set the pcb's delaying status to true
    // set the amount of delay
    // move the process to blocked
    // swtich processes
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