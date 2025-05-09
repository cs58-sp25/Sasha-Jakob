#include "sync.h"

#define MAX_SYNCS 128
extern sync_obj_t *sync_table[MAX_SYNCS];  // Storing all sync objects (pipes, locks, cvars)
extern int global_sync_counter;

static int InitSyncObject(sync_type_t type, void *object, int *idp){
    // allocate a new sync object
        // if it fails return an error
    // set the sync object's id and type
    // check to see type
        // Cast the object to the type based on sync and add it to the sync object
        // throw an error if invalid type somehow
    // add the sync object to the global sync table
    // increment the global sync counter
    // return 0
}

int InitPipe(int *pipe_idp){
    // If there are too many syncing objects return an error
    // Allocate the new pipe at *pipe_id
        // If it fails return error
    // initialize the pipe fields
    // Init the sync object with InitSyncObject
    // Return the return of InitSyncObject
}

void ShiftPipeBuffer(pipe_t *pipe, int len){
    // Check if it's a valid shift
        // if not just return
    
    // See how many remaining bits should be in the buffer (len buffer - len)
    // loop from 0 to remaining
        //shift bytes from i+n to i
    // update pipe length to remaining
}

// Does not wait to fill the entirety of the buffer
// blocks if nothing is available
// otherwise reads whatever is and returns

int ReadPipe(int pipe_id, void *buf, int len, pcb_t *curr){
    // Get the pipe from the id
    // check to see if the pipe is valid
        // if not return error

    // Check to see if the buffer is empty
        // if it is 
        // set the pcb's read buffer to buf
        // set the pcb's read length to len
        // Add the process to the pipe's read queue
        // It will be resumed later by a writer, and execution will continue
        // after this point, eventually returning the number of bytes read.
    
    // calculate number of bytest to read (min buffer length, len)
    // copy bytes to read bytes from buffer to buf
    // call ShiftPipeBuffer on the pipe
    // return bytes to read
}

void UnblockOneReader(pipe_t *pipe){
    //C heck to see if there are any blocked readers
        // if not return
    // Get the first reader in the queue (a pcb)
    // claculate the number of bytes to copy from the pipe to the buffer (min of reader's read len, and buf len)
    // copy data from pipe to the buffer provided
    // call ShiftPipeBuffer on the pipe
    // set the reader's UC reg[0] to the number of bytes read
    // reset the reader's buffer and read len
    // add the reader to the ready queue
}

// Not a blocking write
// writes whatever it can and then returns
int WritePipe(int pipe_id, void *buf, int len){
    // Get the pipe from the id
    // check to see if the pipe is valid
        // if not return error
    
    // calculate available space in the buffer
        // if there isn't any return 0
    // Else copy the data from buf to the pipe's buffer and copy data
    // Update the buffer length
    // unblock a read using UnblockOneReader

    // return the number of bytes written
}

int InitLock(int *lock_idp){
    // malloc the lock
        // if it fails return error
    // initialize the lock fields
    // Init the sync object with InitSyncObject
    // Return the return of InitSyncObject

}

int LockAcquire(int lock_id){
    // Check to see if the lock is valid
        // If not throw and error
    
    // Get the lock and current pcb
    // If the lock is unlocked
        // Lock it
        // Set the owner
        // return 0;
    // If lock is held
    // Add the pcb to the queue lock
    // set the pcbs waiting lock
    // block the current process
    // Context Switch
    // return 0;

}

int LockRelease(int lock_id){
    // Check to see if the lock is valid
        // If not throw and error
    
    // Get the lock and current pcb
    // Check if the current process owns the lock
        // If not throw an error
    // Check if there are waiters
        // If so give the lock to the next waiter
        // null the waiter's waiting lock
        // Set the next waiter to ready
    // else set locked to false and owner to NULL
    // return 0

}

int InitCvar(int *lock_idp){
    // malloc the cvar
        // if it fails return error
    // initialize the cvar fields
    // Init the sync object with InitSyncObject
    // Return the return of InitSyncObject

}

int CvarWait(int cvar_id, int lock_id){
    // Check to see if the cvar is valid
        // if it fails return error
    // Check to see if the lock is valid
        // if it fails return error
    
    // Grab the lock, cvar, and current process
    // Check to see if the current process owns the lock
        // if it fails return error
    // Release the lock as above, don't call release though since that would validate process and lock again
    // Add the process to cvar's waiters
    // Set the process's waiting cvar
    // Context switch
    // While lock is locked
        // Add the pcb to the lock's queue
        // Context Switch
    
    // Claim the lock when able
    // return 0
}

int CvarSignal(int cvar_id){
    // Check to see if the cvar is valid
        // if it fails return error

    // Grab the cvar
    // If the queue is empty return 0

    // Else pop the first waiter
    // Add the waiter to ready
    // return 0
}

int CvarBroadcast(int cvar_id){
    // Check to see if the cvar is valid
        // if it fails return error
    
    // Grab the cvar
    // If the queue is empty return 0

    // while the queue is not empty
        // pop the first waiter
        // add the waiter to ready
    
    // return 0
    
}



int ReclaimSync(int id){
    // check if it's a valid id
        // if not return error
    // get the object from the table
        // if its invalid return error
    // check to see if its one of the object type
        // based on the object type, free what is necessary
        // If it is somehow not a valid type return an error
    // Free the sync object
    // Set the sync table entry to NULL
    // return 0

}
