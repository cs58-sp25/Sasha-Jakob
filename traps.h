#include "syscalls.h"
#include "hardware.h"
#include "pcb.h"
#include "yuser.h"

// Trap handler for system calls (TRAP_KERNEL)
void kernel_handler(UserContext *cont);

// Trap handler for clock interrupts (TRAP_CLOCK)
void clock_handler(UserContext *cont);

// Trap handler for illegal instruction exceptions (TRAP_ILLEGAL)
void illegal_handler(UserContext *cont);

// Trap handler for memory access violations (TRAP_MEMORY)
void memory_handler(UserContext *cont);

// Trap handler for math errors (TRAP_MATH)
void math_handler(UserContext *cont);

// Trap handler for terminal input (TRAP_TTY_RECEIVE)
void receive_handler(UserContext *cont);

// Trap handler for terminal output complete (TRAP_TTY_TRANSMIT)
void transmit_handler(UserContext *cont);