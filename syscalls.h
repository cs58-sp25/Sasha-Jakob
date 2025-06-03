#include <hardware.h>
#include <yuser.h>
#include "pcb.h"
#include "sync.h"
#include "memory.h"

// Declare the array of syscall handler function pointers
typedef void (*syscall_handler_t)(UserContext *uctxt);

extern syscall_handler_t syscall_handlers[256]; // Array of trap handlers
void syscalls_init(void);

void SysUnimplemented(UserContext *uctxt);
void SysFork(UserContext *uctxt);
void SysExec(UserContext *uctxt);
void SysExit(UserContext *uctxt);
void SysWait(UserContext *uctxt);
void SysGetPID(UserContext *uctxt);
void SysBrk(UserContext *uctxt);
void SysDelay(UserContext *uctxt);
void SysTtyRead(UserContext *uctxt);
void SysTtyWrite(UserContext *uctxt);
void SysPipeInit(UserContext *uctxt);
void SysPipeRead(UserContext *uctxt);
void SysPipeWrite(UserContext *uctxt);
void SysLockInit(UserContext *uctxt);
void SysLockAcquire(UserContext *uctxt);
void SysLockRelease(UserContext *uctxt);
void SysCvarInit(UserContext *uctxt);
void SysSignal(UserContext *uctxt);
void SysBroadcast(UserContext *uctxt);
void SysCvarWait(UserContext *uctxt);
void SysReclaim(UserContext *uctxt);
pcb_t *schedule(UserContext *uctxt);
