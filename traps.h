
#ifndef _TRAPS_H_
#define _TRAPS_H_

#include "hardware.h"
#include "syscalls.h"

typedef void (*trap_handler_t)(UserContext *uctxt);

extern trap_handler_t trap_handlers[TRAP_VECTOR_SIZE]; // Array of trap handlers

static void other(void); // Placeholder function for unimplemented traps
void trap_init(void); // Initialize the trap handlers

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

#endif
