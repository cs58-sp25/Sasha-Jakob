
#ifndef _SYNC_H_  // If _SYNC_H_ is not defined
#define _SYNC_H_  // Define _SYNC_H_

#include <hardware.h>
#include <stdbool.h>

#include "pcb.h"

// Magic number sorry bout it
#define MAX_SYNCS 128
#define PCB_BLOCKED 30

typedef struct pipe {
    char buffer[PIPE_BUFFER_LEN];
    int read_pos;
    int write_pos;
    int bytes_in_buffer;
    struct list readers;
    struct list writers;
    bool open_for_read;
    bool open_for_write;
} pipe_t;

typedef struct lock {
    bool locked;
    pcb_t *owner;
    struct list waiters;
} lock_t;

typedef struct cvar {
    struct list waiters;
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

int InitSyncObject(sync_type_t type, void *object);
int GetCheckSync(int id, sync_type_t expected, sync_obj_t **out_sync);

int SyncInitPipe(int *pipe_idp);
int SyncReadPipe(int pipe_id, void *buf, int len);
void SyncDrainWriters(pipe_t *pipe);
void SyncDrainReaders(pipe_t *pipe);
int SyncWritePipe(int pipe_id, void *buf, int len);

int SyncInitLock(int *lock_idp);
int SyncLockAcquire(int lock_id);
int SyncLockRelease(int lock_id);

int SyncInitCvar(int *cvar_idp);
int SyncCvarSignal(int cvar_id);
int SyncCvarBroadcast(int cvar_id);
int SyncCvarWait(int cvar_id, int lock_id);

int SyncReclaim(int id);
int GetNewID(void);
void FreeID(int id);

#endif
