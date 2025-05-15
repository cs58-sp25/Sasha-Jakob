/**
 * List node structure for intrusive linked lists
 * Used to link PCBs in various queues
 */

#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include "memory.h"
#include <ylib.h>
#include <hardware.h>

/**
 * List structure to manage linked lists of PCBs
 */

typedef struct list_node {
    list_node_t *prev;
    list_node_t *next;
} list_node_t;

typedef struct list {
    list_node_t head;  // Sentinel node (not a real element)
    int count;         // Number of nodes in the list
} list_t;

/**
 * Initializes a new list
 * 
 * @param the new list to initialize
 */
void list_init(list_t *new_list);

/**
 * Creates a new list from scratch
 * 
 * @return the new list
 */
list_t *create_list(void);

/**
 * Inserts a node to the tail of the list
 * 
 * @param the node to insert into the list
 * @param the list to insert the node into
 */
void insert_tail(list_t* list, list_node_t *node);

/**
 * Checks to see if list contains node
 * 
 * @param the list to check in
 * @param the node to check for
 * @return 1 if the list contains node, 0 otherwise
 */
int list_contains(list_t *list, list_node_t *node);

/**
 * Remove a node from a list
 *
 * @param node Node to remove
 */
void list_remove(list_t *list, list_node_t *node);


/**
 * Check if list is empty
 *
 * @param list List to check
 * @return 1 if empty, 0 if not
 */
int list_is_empty(list_t *list);

/**
 * Pops the first node off the head of the list (if possible)
 * 
 * @param the list to pop the node from
 * @return the first node in the list OR NULL if the list is empty
 */
list_node_t* pop(list_t *list);


#endif /* LIST_H */