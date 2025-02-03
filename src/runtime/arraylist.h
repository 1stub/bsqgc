#pragma once

#include "common.h"
#include <stdio.h>

/* This queue size will need to be tinkered with */
#define LIST_CAPACITY 4096
#define START_POS (LIST_CAPACITY / 2)

typedef struct ArrayList {
    Object* data[LIST_CAPACITY]; // Static array storage
    size_t size;                 // Number of elements
    uint16_t head;               // First element pointer
    uint16_t tail;               // Last element pointer
} ArrayList;
    
/* Arraylist helpers */
static inline void initialize_list(ArrayList* list) 
{
    list->size = 0;
    list->head = START_POS;
    list->tail = START_POS;
}

static inline void add_to_list(ArrayList* list, Object* obj) 
{
    if(list->tail >= LIST_CAPACITY) {
        debug_print("[ERROR] Excceded list capacity!\n");
        return ;
    }
    list->data[list->tail++] = obj;
    list->size++;
}

/* Remove last element of list */
static inline Object* remove_tail_from_list(ArrayList* list) 
{
    if (list->size == 0) {
        debug_print("[ERROR] Attempted to remove head from empty list!\n");
        return NULL;
    } else if (list->head > LIST_CAPACITY || list->tail == UINT16_MAX) {
        debug_print("[ERROR] Head is too large or small for list size!\n");
        return NULL;
    }

    Object* tail = list->data[--list->tail];
    list->size--;

    return tail;
}

/* Remove first element of list */
static inline Object* remove_head_from_list(ArrayList* list) {
    if (list->size == 0) {
        debug_print("[ERROR] Attempted to remove head from empty list!\n");
        return NULL;
    } else if (list->head > LIST_CAPACITY || list->head == UINT16_MAX) {
        debug_print("[ERROR] Head is too large or small for list size!\n");
        return NULL;
    }

    Object* head = list->data[list->head++];  // Store the object being removed
    list->size--;

    return head;
}

static inline bool is_list_empty(ArrayList* list) 
{
    return (list == NULL || list->size == 0);
}
