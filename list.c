

#include "list.h"

void list_init(list_t *new_list){
    TracePrintf(1, "ENTER list_init.\n");
    new_list->count = 0;
    // Initialize list sentinel nodes
    new_list->head.prev = &new_list->head;
    new_list->head.next = &new_list->head;
    TracePrintf(1, "EXIT list_init.\n");
}

list_t *create_list(void){
    TracePrintf(1, "ENTER create_list.\n");
    // Initialize the list
    list_t *new_list = malloc(sizeof(list_t));
    // Set queue counters to zero
    list_init(new_list);
    TracePrintf(1, "EXIT create_list.\n");
    return new_list;
}

void insert_tail(list_t *list, list_node_t *node){
    TracePrintf(1, "ENTER insert_tail.\n");
    // Check to see if the node and list exist
    if (node == NULL) {
        TracePrintf(1, "ERROR, The node to insert does not exist.\n");
        return; 
    } 
    if (list == NULL) {
        TracePrintf(1, "ERROR, The list to insert in to does not exist.\n");
        return; 
    }

    // Set the nodes previous to the last element of this list
    node->prev = list->head.prev;
    // Set the nodes next to the head of the list
    node->next = &list->head;
    // Set the previous last nodes next to the new last node
    list->head.prev->next = node;
    // Set the heads previous to the new last node
    list->head.prev = node;
    // increment the count on the list
    list->count ++;
    TracePrintf(1, "EXIT insert_tail.\n");
}

void insert_head(list_t *list, list_node_t *node){
    TracePrintf(1, "Enter insert_head.\n");
    if (node == NULL) {
        TracePrintf(1, "ERROR, The node to insert does not exist.\n");
        return; 
    } 
    if (list == NULL) {
        TracePrintf(1, "ERROR, The list to insert in to does not exist.\n");
        return; 
    }
    node->prev = &list->head;
    node->next = list->head.next;
    list->head.next->prev = node;
    list->head.next = node;
    list->count ++;

    TracePrintf(1, "EXIT insert_head.\n");
}

int list_contains(list_t *list, list_node_t *node) {
    TracePrintf(1, "ENTER list_contains.\n");
    if (node == NULL) {
        TracePrintf(1, "ERROR, The node to check for does not exist.\n");
        return ERROR; 
    } 
    if (list == NULL) {
        TracePrintf(1, "ERROR, The list to check in does not exist.\n");
        return ERROR; 
    }

    list_node_t *curr = list->head.next;
    while (curr != &list->head) {
        if (curr == node) {
            TracePrintf(1, "EXIT list_contains, The node was found.\n");
            return 1;
        }
        curr = curr->next;
    }
    TracePrintf(1, "EXIT list_contains, The node was not found.\n");
    return 0;
}

void list_remove(list_t *list, list_node_t *node) {
    TracePrintf(1, "ENTER list_remove.\n");
    // Check to see if the node is actually in a list (this is more of a sanity check then anything)
    if (!node->next || !node->prev) {
        TracePrintf(1, "ERROR, The node is not in a list or the list was broken.\n");
        return;
    }

    // Update adjacent nodes' pointers to skip this node
    list_node_t *temp = node->prev;
    temp->next = node->next;
    node->next->prev = temp;
    node->next = node->prev = NULL;

    // decrement the list's count
    list->count --;
    TracePrintf(1, "EXIT list_remove.\n");
}

int list_is_empty(list_t *list) {
    TracePrintf(1, "ENTER list_is_empty.\n");
    if (list == NULL) {
        TracePrintf(1, "ERROR, The list to insert in to does not exist.\n");
        return ERROR; 
    }
    TracePrintf(1, "EXIT list_is_empty.\n");
    return list->count == 0;
}

list_node_t *pop(list_t *list) {
    TracePrintf(1, "ENTER pop.\n");
    if (list_is_empty(list)) {
        TracePrintf(1, "ERROR, The list was empty or the list didn't exist (last trace would clarify).\n");
        return NULL;
    }

    list_node_t *ret = list->head.next;

    ret->next->prev = &list->head;
    list->head.next = ret->next;

    ret->next = ret->prev = NULL;
    list->count--;

    TracePrintf(1, "EXIT pop.\n");
    return ret;
}

list_node_t *peek(list_t *list){
    if(list_is_empty(list)){
        TracePrintf(1, "ERROR, The list was empty or the list didn't exist (last trace would clarify).\n");
        return NULL;
    }

    return list->head.next;
}
