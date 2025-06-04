#include <yalnix.h>
#include <yuser.h>
#include <ykernel.h>

#include "sync.h"

sync_obj_t *sync_table[MAX_SYNCS];
int global_sync_counter = 0;
char id_used[MAX_SYNCS] = {0};


int InitSyncObject(sync_type_t type, void *object){
    // allocate a new sync object
        // if it fails return an error
    sync_obj_t *new_sync = (sync_obj_t*)malloc(sizeof(sync_obj_t));
    if(new_sync == NULL){
        TracePrintf(1, "ERROR, the new sync object failed to allocate.\n");
        return ERROR;
    }
    // set the sync object's id and type
    new_sync->type = type;
    int id = GetNewID();
    if(id < 0){
        TracePrintf(1,"ERROR, could not find a valid id.\n");
        free(new_sync);
    }
    new_sync->id = id;
    // check to see type
        // Cast the object to the type based on sync and add it to the sync object
        // throw an error if invalid type somehow
    switch(type){
        case PIPE:
            new_sync->object.pipe = (pipe_t *)object;
            break;
        case LOCK:
            new_sync->object.lock = (lock_t *)object;
            break;
        case CVAR:
            new_sync->object.cvar = (cvar_t *)object;
            break;
        default:
            // This should never be reached in normal running
            free(new_sync);
            FreeID(id);
            TracePrintf(1,"Error, an invalid sync type has tried to be initialized.\n");
            return ERROR;
    }
    // add the sync object to the global sync table
    sync_table[id] = new_sync;
    // return id
    return id;
}

int GetCheckSync(int id, sync_type_t expected, sync_obj_t **out_sync){
    if (id < 0 || id >= MAX_SYNCS || sync_table[id] == NULL){
        TracePrintf(1, "ERROR, invalid sync object ID %d.\n", id);
        return ERROR;
    }

    sync_obj_t *sync = sync_table[id];
    if (sync->type != expected) {
        TracePrintf(1, "ERROR, Sync object ID %d is not of expected type %d (got type %d", id, expected, sync->type);
    }

    *out_sync = sync;
    return SUCCESS;
}

int SyncInitPipe(int *pipe_idp){
    TracePrintf(1, "Enter SyncInitPipe.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter >= MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached.\n");
        return ERROR;
    }
    // initialize the pipe fields
    pipe_t *new_pipe = (pipe_t *)malloc(sizeof(pipe_t));
    if(new_pipe == NULL){
        TracePrintf(1, "ERROR, the new pipe could not be allocated.\n");
    }
    new_pipe->read_pos = 0;
    new_pipe->write_pos = 0;
    new_pipe->bytes_in_buffer = 0;
    list_init(&new_pipe->readers);
    list_init(&new_pipe->writers);
    new_pipe->open_for_read = true;
    new_pipe->open_for_write = true;
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(PIPE, (void *)new_pipe);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
        free(new_pipe);
        return ERROR;
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    *pipe_idp = rc;
    TracePrintf(1, "Exit SyncInitPipe.\n");
    return SUCCESS;
}

int SyncDestroyPipe(pipe_t *pipe){
}

// Does not wait to fill the entirety of the buffer
// blocks if nothing is available
// otherwise reads whatever is and returns

int SyncReadPipe(int pipe_id, void *buf, int len){
    TracePrintf(1, "Enter SyncReadPipe with id %d.\n", pipe_id);
    pcb_t *curr = current_process;

    sync_obj_t *sync;
    if (GetCheckSync(pipe_id, PIPE, &sync) == ERROR){
        return ERROR;
    }

    // check to see if the pipe is valid
        // if not return error
    pipe_t *pipe = sync->object.pipe;
    if(!pipe->open_for_read) {
        TracePrintf(1, "ERROR, the pipe is not open for reading.\n");
        return ERROR;
    }
    
    // Check to see if the buffer is empty
        // if it is 
        // set the pcb's read buffer to buf
        // set the pcb's read length to len
        // Add the process to the pipe's read queue
        // When a writer eventually writes to this pipe the writer will make sure the reader is woken, written to, and put into the ready queue
    if(pipe->bytes_in_buffer == 0) {
        TracePrintf(1, "Pipe is empty, blocking process %d.\n", curr->pid);
        curr->waiting_pipe_id = pipe_id;
        curr->pipe_buffer = buf;
        curr->pipe_len = len;

        insert_tail(&pipe->readers, &curr->queue_node);
        curr->state = PROCESS_BLOCKED;

        TracePrintf(1, "Exit SyncReadPipe.\n");
        return PCB_BLOCKED;
    }

    // calculate number of bytest to read (min buffer length, len)
    int to_read = (len < pipe->bytes_in_buffer) ? len : pipe->bytes_in_buffer;
    char *dest = (char *)buf;
    // copy bytes to read bytes from buffer to buf
    for (int i = 0; i < to_read; i++){
        dest[i] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_LEN;
    }
    
    curr->user_context.regs[0] = to_read;

    pipe->bytes_in_buffer -= to_read;
    SyncDrainWriters(pipe);
    // return bytes to read

    TracePrintf(1, "Exit SyncReadPipe.\n");
    return SUCCESS;
}

void SyncDrainWriters(pipe_t *pipe) {
    TracePrintf(1, "Attempting to drain the writers queue for the pipe.\n");
    while(1) {
        list_node_t *node = pop(&pipe->writers);
        if(node == NULL){
            TracePrintf(1, "The writers queue is drained.\n");
            if (pipe->bytes_in_buffer != 0 && pipe->readers.count != 0) SyncDrainReaders(pipe);
            return;
        }

        pcb_t *writer = pcb_from_queue_node(node);
        char *src = (char *)writer->pipe_buffer;
        int len = writer->pipe_len;
        int written = 0;

        while(pipe->bytes_in_buffer < PIPE_BUFFER_LEN && writer->write_loc < len){
            pipe->buffer[pipe->write_pos] = src[writer->write_loc];
            pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_LEN;
            pipe->bytes_in_buffer++;
            writer->write_loc++;
            written++;
        }

        TracePrintf(1, "Writer %d wrote %d bytes to pipe.\n", writer->pid, written);
        if (writer->write_loc < len) {
           insert_head(&pipe->writers, node);
           TracePrintf(1, "Writer %d did not complete it's write, requeued.\n", writer->pid);
           if (pipe->bytes_in_buffer != 0 && pipe->readers.count != 0) SyncDrainReaders(pipe);
           return;
        }

        writer->pipe_buffer = NULL;
        writer->pipe_len = 0;
        writer->write_loc = 0;
        writer->waiting_pipe_id = -1;
        writer->state = PROCESS_DEFAULT;
        writer->user_context.regs[0] = len;

        add_to_ready_queue(writer);
    }
}

void SyncDrainReaders(pipe_t *pipe){
    TracePrintf(1, "Enter SyncDrainReaders.\n");
    while(pipe->bytes_in_buffer > 0){
        list_node_t *node = pop(&pipe->readers);
        if (node == NULL){
            TracePrintf(1,"Exit, SyncDrainReaders, the readers queue is drained.\n");
            if (pipe->bytes_in_buffer < PIPE_BUFFER_LEN && pipe->readers.count != 0) SyncDrainWriters(pipe);
            return;
        }

        pcb_t *reader = pcb_from_queue_node(node);

        char *dest = (char *)reader->pipe_buffer;
        int req = reader->pipe_len;
        int avl = pipe->bytes_in_buffer;
        int to_read = (req < avl) ? req : avl;

        for (int i = 0; i < to_read; i++){
            dest[i] = pipe->buffer[pipe->read_pos];
            pipe->read_pos = (pipe->read_pos + 1) % PIPE_BUFFER_LEN;
            pipe->bytes_in_buffer--;
        }
        
        reader->user_context.regs[0] = to_read;
        reader->waiting_pipe_id = -1;
        reader->state = PROCESS_DEFAULT;
        add_to_ready_queue(reader);

        TracePrintf(1, "Reader %d read %d bytes.\n", reader->pid, to_read);
    }
    TracePrintf(1, "Exit SyncDrainReaders, the pipe buffer has been drained.\n");
    if (pipe->bytes_in_buffer < PIPE_BUFFER_LEN && pipe->readers.count != 0) SyncDrainWriters(pipe);
}

// Just kidding this is blocking now
int SyncWritePipe(int pipe_id, void *buf, int len){
    TracePrintf(1, "Enter SyncWritePipe.\n");
    sync_obj_t *sync;
    if (GetCheckSync(pipe_id, PIPE, &sync) == ERROR){
        return ERROR;
    } 

    // check to see if the pipe is valid
        // if not return error
    pipe_t *pipe = sync->object.pipe;
    if(!pipe->open_for_write) {
        TracePrintf(1, "ERROR, the pipe is not open for writing.\n");
        return ERROR;
    }

    int space = PIPE_BUFFER_LEN - pipe->bytes_in_buffer;
    char *src = (char *)buf;
    int written = 0;
    pcb_t *writer = current_process;

    writer->write_loc = 0;
    while(written < space && writer->write_loc < len){
        pipe->buffer[pipe->write_pos] = src[writer->write_loc];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_BUFFER_LEN;
        pipe->bytes_in_buffer++;
        writer->write_loc++;
        written++;
        
    }
 
    TracePrintf(1, "Writer %d wrote %d bytes to pipe.\n", writer->pid, written);
    if (writer->write_loc < len) {
        writer->pipe_buffer = buf;
        writer->pipe_len = len;
        writer->waiting_pipe_id = pipe_id;

        writer->state = PROCESS_BLOCKED;
        insert_tail(&pipe->writers, &writer->queue_node);
        TracePrintf(1, "Writer %d did not complete it's write, requeued.\n", writer->pid);
        
        SyncDrainReaders(pipe);

        return PCB_BLOCKED;
    }

    writer->user_context.regs[0] = len;
    SyncDrainReaders(pipe);
    return SUCCESS;
}

int SyncInitLock(int *lock_idp){
    TracePrintf(1, "Enter SyncInitLock.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter >= MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached.\n");
        return ERROR;
    }
    // initialize the pipe fields
    lock_t *new_lock= (lock_t *)malloc(sizeof(lock_t));
    if(new_lock == NULL){
        TracePrintf(1, "ERROR, the new lock could not be allocated.\n");
    }
    new_lock->locked = false;
    new_lock->owner = NULL;
    list_init(&new_lock->waiters);
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(LOCK, (void *)new_lock);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
        free(new_lock);
        return ERROR;
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    *lock_idp = rc;
    TracePrintf(1, "Exit SyncInitLock.\n");
    return SUCCESS;
}

int SyncLockAcquire(int lock_id){
    TracePrintf(1, "Enter SyncLockAcquire.\n");
    // Check to see if the lock is valid
        // If not throw and error
    sync_obj_t *sync;
    if (GetCheckSync(lock_id, LOCK, &sync) == ERROR){
        return ERROR;
    } 

    // Get the lock and current pcb
    lock_t *lock = sync->object.lock;
    
    // If the lock is unlocked
        // Lock it
        // Set the owner
        // return 0;
    if(!lock->locked){
        lock->locked = true;
        lock->owner = current_process;
        return SUCCESS;
    } 

    
    // If lock is held
    // Add the pcb to the queue lock
    // set the pcbs waiting lock
    // block the current process
    insert_tail(&lock->waiters, &current_process->queue_node);
    current_process->state = PROCESS_BLOCKED;
    current_process->waiting_lock_id = lock_id;
    
    return PCB_BLOCKED;

}

int SyncLockRelease(int lock_id){
    TracePrintf(1, "Enter SyncLockRelease.\n");
    // Check to see if the lock is valid
        // If not throw and error 
    sync_obj_t *sync;
    if (GetCheckSync(lock_id, LOCK, &sync) == ERROR){
        return ERROR;
    } 
    // Get the lock and current pcb
    lock_t *lock = sync->object.lock;
    pcb_t *curr = current_process;

    // Check if the current process owns the lock
        // If not throw an error
    if(lock->owner != curr){
        TracePrintf(1, "ERROR, lock %d is owned by process %d not process %d.\n", lock_id, lock->owner->pid, curr->pid);
        return ERROR;
    }

    // Check if there are waiters
        // If so give the lock to the next waiter
        // null the waiter's waiting lock
        // Set the next waiter to ready
    if(lock->waiters.count != 0){
        pcb_t *next = pcb_from_queue_node(pop(&lock->waiters));
        next->state = PROCESS_DEFAULT;
        next->waiting_lock_id = -1;

        add_to_ready_queue(next);
        lock->owner = next;
        TracePrintf(1, "Exit SyncLockRelease, the next process waiting was given the lock.\n");
        return SUCCESS;
    }

    // else set locked to false and owner to NULL
    lock->locked = false;
    lock->owner = NULL;
    TracePrintf(1, "Exit SyncLockRelease, there were no processes waiting on this lock.\n");
    return 0;

}

int SyncInitCvar(int *lock_idp){
    TracePrintf(1, "Enter SyncInitCvar.\n");
    // If there are too many syncing objects return an error
    if(global_sync_counter >= MAX_SYNCS){
        TracePrintf(1, "ERROR, the maximum number of synchronization constants has been reached.\n");
        return ERROR;
    }
    // initialize the cvar fields
    cvar_t *new_cvar= (cvar_t *)malloc(sizeof(cvar_t));
    if(new_cvar == NULL){
        TracePrintf(1, "ERROR, the new cvar could not be allocated.\n");
    }
    list_init(&new_cvar->waiters);
    // Init the sync object with InitSyncObject
    int rc = InitSyncObject(CVAR, (void *)new_cvar);
    if(rc == ERROR){
        TracePrintf(1, "ERROR, there was an issue with allocating the synchonization object.\n");
        free(new_cvar);
        return ERROR;
    }
    // Return the return of InitSyncObject and set the idp value to the returned id
    *lock_idp = rc;
    TracePrintf(1, "Exit SyncInitCvar.\n");
    return 0;

}

int SyncCvarWait(int cvar_id, int lock_id){
    TracePrintf(1, "Enter SyncCvarWait.\n");
    // Check to see if the cvar is valid
        // If not throw and error 
    sync_obj_t *sync;
    if (GetCheckSync(cvar_id, CVAR, &sync) == ERROR){
        return ERROR;
    } 
    // Get the cvar
    cvar_t *cvar = sync->object.cvar;

    // Check to see if the lock is valid
        // If not throw and error 
    if (GetCheckSync(lock_id, LOCK, &sync) == ERROR){
        return ERROR;
    } 
    // Get the lock
    lock_t *lock = sync->object.lock;
    pcb_t *curr = current_process;

    // Check to see if the current process owns the lock
        // if it fails return error 
    if(lock->owner != curr){
        TracePrintf(1, "ERROR, lock %d is owned by process %d not process %d.\n", lock_id, lock->owner->pid, curr->pid);
        return ERROR;
    }

    // Release the lock as above, don't call release though since that would validate process and lock again
    // Check if there are waiters
        // If so give the lock to the next waiter
        // null the waiter's waiting lock
        // Set the next waiter to ready
    if(lock->waiters.count != 0){
        pcb_t *next = pcb_from_queue_node(pop(&lock->waiters));
        next->state = PROCESS_DEFAULT;
        next->waiting_lock_id = -1;

        add_to_ready_queue(next);
        lock->owner = next;
              
    } else {
        lock->locked = false;
        lock->owner = NULL;

    }

    // Add the process to cvar's waiters
    insert_tail(&cvar->waiters, &curr->queue_node);
    // Set the process's waiting cvar
    curr->waiting_cvar_id = cvar_id;
    curr->state = PROCESS_BLOCKED;
    
    TracePrintf(1, "Exit SyncCvarWait.\n");
    return PCB_BLOCKED;
    
}

int SyncCvarSignal(int cvar_id){
    TracePrintf(1, "Enter SyncCvarSignal.\n");
    // Check to see if the cvar is valid
        // If not throw and error 
    sync_obj_t *sync;
    if (GetCheckSync(cvar_id, CVAR, &sync) == ERROR){
        return ERROR;
    } 
    // Get the cvar
    cvar_t *cvar = sync->object.cvar;

    // If the queue is empty return 0
    if (cvar->waiters.count == 0){
        TracePrintf(1, "Exit SyncCvarSignal.\n");
        return 0;
    }

    // Else pop the first waiter
    pcb_t *next = pcb_from_queue_node(pop(&cvar->waiters));
    TracePrintf(1, "Removing proccess %d from the waiters list of cvar %d.\n", next->pid, cvar_id);
    next->waiting_cvar_id = -1;
    
    // Add the waiter to ready
    next->state = PROCESS_DEFAULT;
    add_to_ready_queue(next);
    
    TracePrintf(1, "Exit SyncCvarSignal.\n");
    return 0;
}

int SyncCvarBroadcast(int cvar_id){
    TracePrintf(1, "Enter SyncCvarBroadcast.\n");
    // Check to see if the cvar is valid
        // If not throw and error 
    sync_obj_t *sync;
    if (GetCheckSync(cvar_id, CVAR, &sync) == ERROR){
        return ERROR;
    } 
    // Get the cvar
    cvar_t *cvar = sync->object.cvar;
    
    // If the queue is empty return 0
    if (cvar->waiters.count == 0){
        TracePrintf(1, "Exit SyncCvarBroadcast.\n");
        return 0;
    }
    

    // while the queue is not empty
        // pop the first waiter
        // add the waiter to ready
    while(cvar->waiters.count != 0){
        pcb_t *next = pcb_from_queue_node(pop(&cvar->waiters));
        TracePrintf(1, "Removing proccess %d from the waiters list of cvar %d.\n", next->pid, cvar_id);
        next->waiting_cvar_id = -1;
    
        // Add the waiter to ready
        next->state = PROCESS_DEFAULT;
        add_to_ready_queue(next);
    
    }
    
    TracePrintf(1, "Exit SyncCvarBroadcast.\n");
    
}



int SyncReclaimSync(int id){
    TracePrintf(1, "Enter SyncReclaimSync.\n");
    // check if it's a valid id
        // if not return error
    if (id < 0 || id >= MAX_SYNCS || sync_table[id] == NULL){
        TracePrintf(1, "ERROR, invalid sync object ID %d.\n", id);
        return ERROR;
    }

    // get the object from the table
    sync_obj_t *sync = sync_table[id];
    switch(sync->type){
        case(PIPE):
            pipe_t *pipe = sync->object.pipe;
            // If these lists aren't empty the processes in the queue WILL break
            clear_list(&pipe->readers);
            clear_list(&pipe->writers);
            free(pipe);
            break;
        case(LOCK):
            lock_t *lock = sync->object.lock;
            clear_list(&lock->waiters);
            free(lock);
            break;
        case(CVAR):
            cvar_t *cvar = sync->object.cvar;
            clear_list(&cvar->waiters);
            free(cvar);
            break;
        default:
            // THIS SHOULD NEVER HAPPEN
            TracePrintf(1,"Error, an invalid sync type has occured.\n");
            return ERROR;

    }
    // Free the sync id
    FreeID(id);
    
    // Set the sync table entry to NULL
    sync_table[id] = NULL;
    
    // Free the sync object
    free(sync);
    TracePrintf(1, "Exit SyncReclaimSync.\n");
    return SUCCESS;
}

int GetNewID(void){
    TracePrintf(1,"Enter GetNewID.\n");
    // Finds the next free id (loops over byte array)
    for (int i=0; i <MAX_SYNCS; i++){
        if(id_used[i] == 0) {
            // When one is found set it to being used, increment the counter and return the id
            id_used[i] = 1;
            global_sync_counter++;
            TracePrintf(1,"Exit GetNewID.\n");
            return i;
        }
    }
    TracePrintf(1,"ERROR, there are no IDs remaining.\n");
    return ERROR;
}

void FreeID(int id){ 
    TracePrintf(1,"Enter FreeID.\n");
    if(id >=0 && id < MAX_SYNCS && id_used[id] == 1){
        TracePrintf(1, "ID %d is now being freed", id);
        id_used[id] = 0;
        global_sync_counter--;
    } else {
        TracePrintf(1, "ERROR, ID %d is invalid.\n", id);
        return;
    }
    TracePrintf(1,"Exit FreeID.\n");
}
