/**
 * Date: 5/4/25
 * File: pcb.c
 * Description: PCB implementation for Yalnix OS
 */

#include "pcb.h"

// Global variables
list_t *ready_queue;
list_t *delay_queue;
list_t *blocked_queue;
list_t *zombie_queue;

int init_pcb_system(void) {
    TracePrintf(1, "ENTER init_pcb_system.\n");
    ready_queue = create_list();
    delay_queue = create_list();
    blocked_queue = create_list();
    zombie_queue = create_list();

    // If any of the queues failed to initialize return Error, else return 0
    if(ready_queue == NULL || delay_queue  == NULL || zombie_queue  == NULL || blocked_queue == NULL) {
        TracePrintf(1, "ERROR, The kernel has failed to allocate a pcb queue.\n");
        return ERROR;
    }
    TracePrintf(1, "EXIT init_pcb_system.\n");
    return 0;
}

pcb_t *create_pcb(void) {
    TracePrintf(1, "ENTER create_pcb.\n");
    // Allocate memory for new PCB
    pcb_t *new_pcb = malloc(sizeof(pcb_t));
    if (new_pcb == NULL) {
        TracePrintf(1, "ERROR, The kernel has failed to allocate the pcb.\n");
        return NULL;
    } // If it failed return NULL

    // Set initial state
    new_pcb->state = PROCESS_READY;

    // Zero out timers, exit code
    new_pcb->time_slice = DEFAULT_TIMESLICE;
    new_pcb->run_time = 0;
    new_pcb->delay_ticks = 0;
    new_pcb->exit_code = 0;

    // Relationships
    new_pcb->parent = NULL;
    list_init(&new_pcb->children);
    new_pcb->waiting_for_children = 0;

    // Initialize region 1 page table as invalid
    for (int i = 0; i < MAX_PT_LEN; i++) {
        new_pcb->region1_pt[i].valid = 0;
        new_pcb->region1_pt[i].prot = 0;
        new_pcb->region1_pt[i].pfn = 0;
    }
    
    new_pcb->brk = NULL;
    // These should all be set by the calling function, idk how memory works

    new_pcb->tty_read_buffer = NULL;
    new_pcb->tty_read_len = 0;
    new_pcb->tty_read_terminal = -1;

    new_pcb->tty_write_buffer = NULL;
    new_pcb->tty_write_len = 0;
    new_pcb->tty_write_terminal = -1;
    new_pcb->tty_write_offset = 0;

    new_pcb->waiting_lock_id = -1;
    new_pcb->waiting_cvar_id = -1;
    new_pcb->waiting_pipe_id = -1;

    TracePrintf(1, "EXIT create_pcb.\n");
    return new_pcb;
}


// FOR ANY add_to_{}_queue CALL MAKE SURE THE PROCESS IS EITHER (1) NEW or (2)HAD REMOVE_FROM_{}_QUEUE CALLED ON IT
// The pcb must have it's state set to default for it to work
void add_to_ready_queue(pcb_t *process) {
    TracePrintf(1, "ENTER add_to_ready_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_DEFAULT) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_DEFAULT.\n");
        return;
    };

    process->state = PROCESS_READY;
    insert_tail(ready_queue, &process->queue_node);
    TracePrintf(1, "EXIT add_to_ready_queue.\n");
}

void remove_from_ready_queue(pcb_t *process) {
    TracePrintf(1, "ENTER remove_from_ready_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_READY) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_READY.\n");
        return;
    }
    process->state = PROCESS_DEFAULT;
    list_remove(ready_queue, &process->queue_node);
    TracePrintf(1, "EXIT remove_from_ready_queue.\n");
}

void add_to_delay_queue(pcb_t *process, int ticks) {
    TracePrintf(1, "ENTER add_to_delay_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    // Check if process is already in a queue
    if (process->state != PROCESS_DEFAULT) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_DEFAULT.\n");
        return;
    };

    process->state = PROCESS_DELAYED;
    process-> delay_ticks = ticks;
    insert_tail(delay_queue, &process->queue_node);
    TracePrintf(1, "EXIT add_to_delay_queue.\n");
}

void remove_from_delay_queue(pcb_t *process) {
    TracePrintf(1, "ENTER remove_from_delay_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_DELAYED) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_DELAYED.\n");
        return;
    }
    process->state = PROCESS_DEFAULT;
    list_remove(delay_queue, &process->queue_node);
    TracePrintf(1, "EXIT remove_from_delay_queue.\n");
}

void add_to_zombie_queue(pcb_t *process) {
    TracePrintf(1, "ENTER add_to_zombie_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_DEFAULT) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_DEFAULT");
        return;
    };
    process->state = PROCESS_ZOMBIE;
    insert_tail(zombie_queue, &process->queue_node);
    TracePrintf(1, "EXIT add_to_zombie_queue.\n");
}

void remove_from_zombie_queue(pcb_t *process) {
    TracePrintf(1, "ENTER remove_from_zombie_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_ZOMBIE) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_ZOMBIE.\n");
        return;
    }
    process->state = PROCESS_DEFAULT;
    list_remove(zombie_queue, &process->queue_node);
    TracePrintf(1, "EXIT remove_from_zombie_queue.\n");
}

void add_to_blocked_queue(pcb_t *process) {
    TracePrintf(1, "ENTER add_to_blocked_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_DEFAULT) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_DEFAULT");
        return;
    };
    process->state = PROCESS_BLOCKED;
    insert_tail(blocked_queue, &process->queue_node);
    TracePrintf(1, "EXIT add_to_blocked_queue.\n");
}

void remove_from_blocked_queue(pcb_t *process) {
    TracePrintf(1, "ENTER remove_from_blocked_queue.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return;
    }
    if (process->state != PROCESS_BLOCKED) {
        TracePrintf(1, "ERROR, The state of the process was not PROCESS_BLOCKED.\n");
        return;
    }
    process->state = PROCESS_DEFAULT;
    list_remove(blocked_queue, &process->queue_node);
    TracePrintf(1, "EXIT remove_from_blocked_queue.\n");
}

// We might need to move this to where KCSwitch and Copy are to avoid circular includes
pcb_t *schedule_next_process(void) {
    TracePrintf(1, "ENTER schedule_next_process.\n");
    // If the ready queue is empty return NULL, this means that either it's idle (might create idling process later) OR there is no other process to switch to and the current process is still running
    if(list_is_empty(ready_queue)) {
        TracePrintf(1, "EXIT schedule_next_process, no other processes to schedule.\n");
        return NULL;
    }

    // Select next process to run from ready queue
    pcb_t *next = pcb_from_queue_node(pop(ready_queue));
    return next; 
    TracePrintf(1, "EXIT schedule_next_process.\n");
}

// Used in a wait syscall
pcb_t *find_zombie_child(pcb_t *process) {
    TracePrintf(1, "ENTER find_zombie_child.\n");
    if(process == NULL){
        TracePrintf(1, "ERROR, process was not an initialized pcb");
        return NULL;
    }
    if(list_is_empty(&process->children)) {
        TracePrintf(1, "ERROR the process has no children.\n");
        return NULL;
    }

    list_node_t *head = &process->children.head;
    list_node_t *curr = head->next;
    // Iterate through process's children list
    while(curr != head){
        pcb_t *curr_pcb = pcb_from_queue_node(curr);
        // For each child, check if it's in PROCESS_ZOMBIE state, if so return
        if (curr_pcb->state == PROCESS_ZOMBIE) {
            TracePrintf(1, "EXIT find_zombie_child: Found zombie child with PID %d.\n", curr_pcb->pid);
            return curr_pcb;
        }
        curr = curr->next;
    }
    // Return NULL if none
    TracePrintf(1, "EXIT find_zombie_child: No zombie child found.\n");
    return NULL;
}

// This is to be used in the clock thing
void update_delayed_processes(void) {
    TracePrintf(1, "ENTER update_delayed_processes.\n");
    if(list_is_empty(delay_queue)) {
        TracePrintf(1, "EXIT update_delayed_processes, no delaying processes.\n");
        return;
    }
    
    list_node_t *head = &delay_queue->head;
    list_node_t *curr = head->next;
    // Iterate through delay queue
    while(curr != head){
        list_node_t *next = curr->next;
        // Get the pcb from curr
        pcb_t *curr_pcb = pcb_from_queue_node(curr);
        // decrement it's delay ticks
        curr_pcb->delay_ticks --;
        // check if done delaying
        if (curr_pcb->delay_ticks == 0){
            remove_from_delay_queue(curr_pcb);
            add_to_ready_queue(curr_pcb);
        }
        curr = next;
    }
    TracePrintf(1, "EXIT update_delayed_processes.\n");
}

void check_zombies(void) {
    TracePrintf(1, "ENTER check_zombies.\n");
    if(list_is_empty(zombie_queue)) {
        TracePrintf(1, "EXIT check_zombies, no zombie processes.\n");
        return;
    }
    
    list_node_t *head = &zombie_queue->head;
    list_node_t *curr = head->next;
    while(curr != head){
        list_node_t *next = curr->next; // Store next before removal
        pcb_t *curr_pcb = pcb_from_queue_node(curr);
        if(curr_pcb->parent == NULL){
            remove_from_zombie_queue(curr_pcb);
            free(curr_pcb);
        } 
        curr = next;
    }
    TracePrintf(1, "EXIT check_zombies.\n");
}

pcb_t *list_contains_pid(list_t *list, int pid){
    TracePrintf(1, "ENTER list_contains_pid.\n");
    if(list == NULL){
        TracePrintf(1, "The list does not exist or is broken.\n");
        return NULL;
    }
    if(list_is_empty(list)) {
        TracePrintf(1, "EXIT list_contains_pid, the list is empty.\n");
        return NULL;
    }

    list_node_t *head = &list->head;
    list_node_t *curr = head->next;
    while(curr != head){
        pcb_t *curr_pcb = pcb_from_queue_node(curr);
        if(pid == curr_pcb->pid) {
            TracePrintf(1, "EXIT list_contains_pid, pcb found.\n");
            return curr_pcb;
        }
        curr = curr->next;
    }
    TracePrintf(1, "EXIT list_contains_pid, pcb not found.\n");
    return NULL;
}

pcb_t *get_pcb_by_pid(int pid) {
    TracePrintf(1, "ENTER get_pcb_by_pid.\n");
    pcb_t *pcb = list_contains_pid(ready_queue, pid);
    if(pcb != NULL) {
        TracePrintf(1, "The pcb %d exists in ready_queue.\n", pid);
        return pcb;
    }
    
    pcb = list_contains_pid(delay_queue , pid);
    if(pcb != NULL) {
        TracePrintf(1, "The pcb %d exists in delay_queue.\n", pid);
        return pcb;
    }

    pcb = list_contains_pid(blocked_queue, pid);
    if(pcb != NULL) {
        TracePrintf(1, "The pcb %d exists in blocked_queue.\n", pid);
        return pcb;
    }

    pcb = list_contains_pid(zombie_queue, pid);
    if(pcb != NULL) {
        TracePrintf(1, "The pcb %d exists in zombie_queue.\n", pid);
        return pcb;
    }

    TracePrintf(1, "The pcb %d does not exist.\n", pid);
    return NULL;
    
}

void add_child(pcb_t *parent, pcb_t *child) {
    TracePrintf(1, "ENTER add_child.\n");
    if(parent == NULL){
        TracePrintf(1, "The parent was not an initialized pcb.\n");
        return;
    } if (child == NULL){
        TracePrintf(1, "The child was not an initialized pcb.\n");
        return;
    }

    // Set child's parent pointer to parent
    child->parent = parent;
    // Add child's children_node to parent's children list
    insert_tail(&parent->children, &child->children_node);
    TracePrintf(1, "EXIT add_child.\n");
}

void remove_child(pcb_t *child) {
    TracePrintf(1, "ENTER remove_child.\n");
    if(child == NULL){
        TracePrintf(1, "ERROR, The child was not an initialized pcb");
        return;
    }
    if (child->parent == NULL){
        TracePrintf(1, "ERROR, The child has no parent.\n");
        return;
    }
    // Remove child's children_node from parent's children list
    list_remove(&child->parent->children, &child->children_node);
    // Set child's parent pointer to NULL
    child->parent = NULL;
    TracePrintf(1, "EXIT remove_child.\n");
}

void orphan_children(pcb_t *parent) {
//     TracePrintf(1, "ENTER orphan_children.\n");
//     if (process == NULL) {
//         TracePrintf(1, "Error: Attempting to orphan children of a NULL PCB.\n");
//         return;
//     }
//     if(list_is_empty(&process->children)) {
//         TracePrintf(1, "EXIT orphan_children the process has no children.\n");
//         return;
//     }

//     if ()
//     while (!list_is_empty(&parent->children)) {
//         pcb_t *child = pcb_from_children_node(pop(&parent->children));
//         if (child->state == PROCESS_ZOMBIE) free(child);
//         else child->parent = NULL;
//     }
//     TracePrintf(1, "EXIT orphan_children.\n");
}

void free_process_memory(pcb_t *proc) {
    // Free all physical frames mapped in Region 1 page table
    // For each valid entry in Region 1 page table:
    //   Get physical frame number
    //   Unmap the virtual page
    //   Free the physical frame
    // Free kernel stack frames
    // For each frame in kernel_stack_pages:
    //   Free the physical frame
}

void terminate_process(pcb_t *process, int status) {
    TracePrintf(1, "ENTER terminate_process.\n");
    if (process == NULL) {
        TracePrintf(1, "Error: Attempting to terminate a NULL PCB.\n");
        return;
    }
    // Set process exit code to state
    process->exit_code = status;
    // Set process state to PROCESS_ZOMBIE
    process->state = PROCESS_ZOMBIE;
    // Orphan children if any (set their parent to NULL)
    orphan_children(process);
    // Release process resources except PCB itself
    free_process_memory(process);
    // Remove from any queue the process might be in
    
    if (process->state != PROCESS_ZOMBIE) {
        if (process->state == PROCESS_READY) {
            remove_from_ready_queue(process);
        } else if (process->state == PROCESS_DELAYED) {
            remove_from_delay_queue(process);
        } else if (process->state == PROCESS_BLOCKED) {
            remove_from_blocked_queue(process);
        } 
    }

    // Check if the parent is waiting on it's children
    if(process->parent != NULL && process->parent->waiting_for_children){
        remove_from_blocked_queue(process->parent);
        add_to_ready_queue(process->parent);
        process->parent->user_context.regs[0] = status;

        free(process);
        return;
    }
    
    // Add to zombie queue
    add_to_zombie_queue(process);
    TracePrintf(1, "EXIT terminate_process.\n");
}

void destroy_pcb(pcb_t *process){
    TracePrintf(1, "ENTER destroy_pcb.\n");
    if (process == NULL) {
        TracePrintf(1, "Error: Attempting to destroy a NULL PCB.\n");
        return;
    }
    // Free all resources associated with the PCB
    free_process_memory(process);
    // Free the PCB structure itself
    free(process);
    TracePrintf(1, "EXIT destroy_pcb.\n");
}