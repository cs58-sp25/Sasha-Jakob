#ifndef _SYSCALLS_H_
#define _SYSCALLS_H_

#include "hardware.h"
#include "pcb.h"
#include "yuser.h"
#include "sync.h"
#include "memory.h"

// Declare the array of syscall handler function pointers
extern void (*syscall_handlers[20])(UserContext *);
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


#endif //_SYSCALLS_H_