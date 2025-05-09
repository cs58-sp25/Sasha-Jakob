#ifndef _SYNC_H_
#define _SYNC_H_

#include "pcb.h"
#include "hardware.h"

typedef struct pipe {
    char buffer[PIPE_BUFFER_LEN];
    int read_pos;
    int write_pos;
    int bytes_in_buffer;
    int pipe_id;
    struct list_node readers;
    struct list_node writers;
    bool open_for_read;
    bool open_for_write;
} pipe_t;

typedef struct lock {
    int lock_id;
    bool locked;
    pcb_t *owner;
    struct list_node waiters;
} lock_t;

typedef struct cvar {
    int cvar_id;
    struct list_node waiters;
} cvar_t;

// Used to determine what a syncing struct is
typedef enum {
    PIPE,
    LOCK,
    CVAR
} sync_type_t;

// A structure used to store either a pipe, a lock, or a cvar
typedef struct sync_obj {
    sync_type_t type;
    int id;
    union {
        pipe_t *pipe;
        lock_t *lock;
        cvar_t *cvar;
    } object;
} sync_obj_t;

// Eventually this will likely need to be able to reclaim old syncing objects for now keep it simple
extern sync_obj_t *sync_table[MAX_SYNCS];  // Storing all sync objects (pipes, locks, cvars)
extern int global_sync_counter; 

static int InitSyncObject(sync_type_t type, void *object, int *idp);

int InitPipe(int *pipe_idp)
void ShiftPipeBuffer(pipe_t *pipe, int len)
int ReadPipe(int pipe_id, void *buf, int len, pcb_t *curr);
void UnblockOneReader(pipe_t *pipe);
int WritePipe(int pipe_id, void *buf, int len);

int InitLock(int *lock_idp);
int LockAcquire(int lock_id);
int LockRelease(int lock_id);

int InitCvar(int *cvar_idp);
int CvarSignal(int cvar_id);
int CvarBroadcast(int cvar_id);
int CvarWait(int cvar_id, int lock_id)

int ReclaimSync(int id);

#endif