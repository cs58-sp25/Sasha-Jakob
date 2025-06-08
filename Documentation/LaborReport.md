# Labor Report
## Jakob:
### Primary Tasks:
- Writing Syscalls
- Writing Traps
- Writing PCB
- Scheduling of new 
- Filling out LoadProgram
- Creating supporting structs and functions for syncs and pcbs
- Editing and debugging code

### Details:
Syscalls Written:
- initializing syscall table
- Editing of Fork
- All of Exec
- All of Wait
- All of GetPID
- All of Brk
- All of Delay
- All Syncing functions

Traps Written:
- initializing trap table
- kernel handler
- clock handler
- illegal handler
- math handler

All PCB and List functions.

Filling out LoadProgram to work with our implementation

Edited KernelStart to function with DoIdle and Init.

Bugfixes

## Sasha:
### Primary Tasks:
-Writing KernelStart
-Setting up compiling
-Writing Page Tables for both regions
-Writing KCcopy() and KCswitch()
-Integrating everything for checkpoints
-Testing and debugging

### Details:
-Kernel Start written for all checkpoints
-Region0 page table initialization 
-Idle cloning into Init succesfully
-Fork syscall
-Editing of PCB
-Editing of Memory handler
-Editing of Load program
-Frame allocation/freeing
-Page to frame mapping

### Time spent
-Over 25 hours a week averaged over the term outside of class

## Unfinished
all TTy, testing of syncing functions
