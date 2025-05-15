/**
 * Date: 5/4/25
 * File: pcb.h
 * Description: PCB header file for Yalnix OS
 */
#include "hardware.h"
#include "kernel.h"
#include "list.h"
#include "memory.h"
#include "ylib.h"

/**
 * Process Control Block structure
 * Contains all information about a process
 */

#define DEFAULT_TIMESLICE 4

 
typedef enum {
    PROCESS_DEFAULT,
    PROCESS_RUNNING,  // Currently running
    PROCESS_READY,    // Ready to run
    PROCESS_DELAYED,  // Delayed (waiting for some #ticks)
    PROCESS_BLOCKED,  // Blocked (waiting for some event)
    PROCESS_ZOMBIE    // Terminated but not reaped
} state_t;

typedef struct pcb {
    // Process context
    UserContext user_context;      // User context (saved registers, PC, etc.)
    KernelContext kernel_context;  // Kernel context

    // Process identification
    int pid;   // Process ID

    // Memory management
    struct pte *region1_pt;                                   // Region 1 page table
    int kernel_stack_pages[KERNEL_STACK_MAXSIZE / PAGESIZE];  // Physical frames for kernel stack
    void *brk;                                                // Current program break (heap limit)

    // Process state
    state_t state;

    // Scheduling
    int time_slice;  // Time quantum for this process
    int run_time;    // How long process has been running

    // Delay information
    int delay_ticks;  // Ticks remaining for Delay syscall

    // Exit information
    int exit_code;      // Exit code when process terminates

    // Process relationships
    struct pcb_t *parent;        // Pointer to parent PCB
    list_t children;           // List of children PCBs
    int waiting_for_children;  // True if process is blocked on Wait

    // Terminal I/O
    char *tty_read_buffer;  // Buffer for terminal read
    int tty_read_len;       // Length of requested read
    int tty_read_terminal;  // Terminal ID for read

    char *tty_write_buffer;  // Buffer for terminal write
    int tty_write_len;       // Length of requested write
    int tty_write_terminal;  // Terminal ID for write
    int tty_write_offset;    // Current offset in write buffer

    // Synchronization
    int waiting_lock_id;  // ID of lock being waited for
    int waiting_cvar_id;  // ID of condition variable being waited for
    int waiting_pipe_id;  // ID of pipe being waited for

    // Queue nodes (intrusive linked list nodes)
    list_node_t queue_node;     // Node for all queues
    list_node_t children_node;  // Node for parent's children list

} pcb_t;


// Container_of macro to get PCB pointer from a list_node
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member)))

// Macros to get PCB from its various nodes
#define pcb_from_queue_node(ptr) container_of(ptr, pcb_t, queue_node)
#define pcb_from_children_node(ptr) container_of(ptr, pcb_t, children_node)

// Process queues and current process
extern list_t *ready_queue;      // Processes ready to run
extern list_t *delay_queue;      // Processes waiting for Delay
// THIS MIGHT BE ABSTRACTED FURTHER LATER (probably not though)
extern list_t *blocked_queue;    // Processes blocking for some other reasons
extern list_t *zombie_queue;     // Terminated but not reaped processes
extern pcb_t *current_process;  // Currently executing process

/**
 * Initialize PCB subsystem
 * Sets up global queues and data structures
 * 
 * @return success or failure of the init
 */
int init_pcb_system(void);


/**
 * Create a new PCB
 * Allocates and initializes PCB and associated resources
 *
 * @return Pointer to new PCB, or NULL on failure
 */
pcb_t *create_pcb(void);


/**
 * Add process to ready queue
 *
 * @param process PCB to add
 */
void add_to_ready_queue(pcb_t *process);


/**
 * Remove process from ready queue
 *
 * @param process PCB to remove
 */
void remove_from_ready_queue(pcb_t *process);


/**
 * Add process to delay queue
 *
 * @param process PCB to add
 * @param ticks Number of clock ticks to delay
 */
void add_to_delay_queue(pcb_t *process, int ticks);


/**
 * Remove process from delay queue
 *
 * @param process PCB to remove
 */
void remove_from_delay_queue(pcb_t *process);


/**
 * Add process to zombie queue
 *
 * @param process PCB to add
 */
void add_to_zombie_queue(pcb_t *process);


/**
 * Remove process from zombie queue
 *
 * @param process PCB to remove
 */
void remove_from_zombie_queue(pcb_t *process);

/**
 * Add process to blocked queue
 *
 * @param process PCB to add
 */
void add_to_blocked_queue(pcb_t *process);

/**
 * Remove process from blocked queue
 *
 * @param process PCB to remove
 */
void remove_from_blocked_queue(pcb_t *process) ;


/**
 * Schedule next process to run
 * Selects the next ready process based on scheduling policy
 *
 * @return Pointer to next process to run
 */
pcb_t *schedule_next_process(void);


/**
 * Check if process has exited children
 *
 * @param process PCB to check
 * @return Pointer to zombie child, or NULL if none
 */
pcb_t *find_zombie_child(pcb_t *process);

/**
 * Update delayed processes
 * Decrements delay counters and moves ready processes
 * Called on each clock tick
 */
void update_delayed_processes(void);


/**
 * Check and clean zombies
 * Looks for orphaned zombie processes and performs the necessary cleanup
 */
void check_zombies(void);

/**
 * Given a process id, checks if the process is in the given list
 * 
 * @param the list to check in
 * @param the pid of the process to check for
 */
pcb_t *list_contains_pcb(list_t *list, int pid);

/**
 * Get PCB by PID
 * Finds a process by its ID
 *
 * @param pid Process ID to find
 * @return PCB pointer if found, NULL if not
 */
pcb_t *get_pcb_by_pid(int pid);


/**
 * Add child to parent's children list
 *
 * @param parent Parent PCB
 * @param child Child PCB
 */
void add_child(pcb_t *parent, pcb_t *child);


/**
 * Remove child from parent's children list
 *
 * @param child Child PCB to remove
 */
void remove_child(pcb_t *child);


/**
 * Orphan all children of a process
 * Sets their parent to NULL when a parent exits
 *
 * @param parent Parent PCB
 */
void orphan_children(pcb_t *parent);


/**
 * Frees all memory in relation to a process
 * 
 */
void free_process_memory(pcb_t *proc);

/**
 * Terminate a process
 * Marks process as zombie and cleans up resources
 *
 * @param process PCB to terminate
 * @param status Exit status
 */
void terminate_process(pcb_t *process, int status);

/**
 * Destroy a PCB
 * Deallocates PCB and associated resources
 *
 * @param process PCB to destroy
 */
<<<<<<< HEAD
void destroy_pcb(pcb_t *process);

/**
 * Takes the current pcb and switches it out with the next waiting pcb or the idle pcb
 * THIS DOES NOTHING TO THE STATUS SO SET IT YOURSELF
 * Calls KernelContextSwitch and sets the new pcb's timeslice (default if not idle, 1 if idle)
 * 
 * @return the new pcb being used
 */
pcb_t *schedule();
=======
void destroy_pcb(pcb_t *process);
>>>>>>> e118d03 (putting everything back into the repo)
