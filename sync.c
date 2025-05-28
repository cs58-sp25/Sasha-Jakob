#include "sync.h"
// Might have to change to starting this in kernel.c and setting sync counter to 0
extern sync_obj_t *sync_table[MAX_SYNC];  // Storing all sync objects (pipes, locks, cvars)
extern int global_sync_counter = 0;

int InitSyncObject(sync_type_t type, void *object){
    // allocate a new sync object
        // if it fails return an error
    // set the sync object's id and type
    // check to see type
        // Cast the object to the type based on sync and add it to the sync object
        // throw an error if invalid type somehow
    // add the sync object to the global sync table
    // increment the global sync counter
    // return idp
}

int SyncInitPipe(int *pipe_idp){
    TracePrintf(1, "Enter SyncInitPipe.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter > MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached");
        return ERROR;
    }
    // initialize the pipe fields
    pipe_t *new_pipe = (pipe_t *)malloc(sizeof(pipe_t));
    if(new_pipe = NULL){
        TracePrintf(1, "ERROR, the new pipe could not be allocated.\n");
    }
    new_pipe->read_pos = 0;
    new_pipe->write_pos = 0;
    new_pipe->bytes_in_buffer = 0;
    list_init(new_pipe->readers);
    list_init(new_pipe->writers);
    new_pipe->open_for_read = true;
    new_pipe->open_for_write = true;
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(PIPE, (void *)new_pipe);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    &pipe_idp = rc;
    TracePrintf(1, "Exit SyncInitPipe.\n");
    return SUCCESS;
}

void SyncShiftPipeBuffer(pipe_t *pipe, int len){
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

int SyncReadPipe(int pipe_id, void *buf, int len, pcb_t *curr){
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

void SyncUnblockOneReader(pipe_t *pipe){
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
int SyncWritePipe(int pipe_id, void *buf, int len){
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

int SyncInitLock(int *lock_idp){
    TracePrintf(1, "Enter SyncInitLock.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter > MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached");
        return ERROR;
    }
    // initialize the pipe fields
    lock_t *new_lock= (lock_t *)malloc(sizeof(lock_t));
    if(new_lock = NULL){
        TracePrintf(1, "ERROR, the new lock could not be allocated.\n");
    }
    new_lock->locked = false;
    new_lock->owner = NULL;
    list_init(new_lock->waiters);
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(LOCK, (void *)new_lock);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
        return ERROR;
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    &lock_idp = rc;
    TracePrintf(1, "Exit SyncInitLock.\n");
    return SUCCESS;
}

int SyncLockAcquire(int lock_id){
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

int SyncLockRelease(int lock_id){
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

int SyncInitCvar(int *lock_idp){
    TracePrintf(1, "Enter SyncInitCvar.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter > MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached");
        return ERROR;
    }
    // initialize the cvar fields
    cvar_t *new_cvar= (cvar_t *)malloc(sizeof(cvar_t));
    if(new_cvar = NULL){
        TracePrintf(1, "ERROR, the new cvar could not be allocated.\n");
    }
    list_init(new_cvar->waiters);
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(CVAR, (void *)new_cvar);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
        return ERROR;
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    &lock_idp = rc;
    TracePrintf(1, "Exit SyncInitCvar.\n");
    return SUCCESS;

}

int SyncCvarWait(int cvar_id, int lock_id){
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

int SyncCvarSignal(int cvar_id){
    // Check to see if the cvar is valid
        // if it fails return error

    // Grab the cvar
    // If the queue is empty return 0

    // Else pop the first waiter
    // Add the waiter to ready
    // return 0
}

int SyncCvarBroadcast(int cvar_id){
    // Check to see if the cvar is valid
        // if it fails return error
    
    // Grab the cvar
    // If the queue is empty return 0

    // while the queue is not empty
        // pop the first waiter
        // add the waiter to ready
    
    // return 0
    
}



int SyncReclaimSync(int id){
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

int GetNewIPD(void){
    // Finds the next free ipd (loops over byte array)
    // Increment the sync counter
    // returns the ipd
    return 0;
}

void FreeIPD(int ipd){
    // Sets the byte array to 0 at ipd
    // decrements the sync counter
}
