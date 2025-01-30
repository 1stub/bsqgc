#pragma once

#include "common.h"
#include <stdio.h>

/* This queue size will need to be tinkered with */
#define WORKLIST_CAPACITY 1024

typedef struct {
    Object* data[WORKLIST_CAPACITY];
    size_t size;
} Worklist;

/* Worklist helpers */
static inline void initialize_worklist(Worklist* worklist) 
{
    worklist->size = 0;
}

static inline bool add_to_worklist(Worklist* worklist, Object* obj) 
{
    if (worklist->size >= WORKLIST_CAPACITY) {
        /* Worklist is full */
        debug_print("Worklist overflow!\n");
        return false;
    }
    worklist->data[worklist->size++] = obj;
    return true;
}

static inline Object* remove_from_worklist(Worklist* worklist) 
{
    if (worklist->size == 0) {
        return NULL;
    }
    return worklist->data[--worklist->size]; //prefix decrement crucial here
}

/** 
* This shifts the entire worklist which I do not like. 
* There is 100% a better way to handle the work lists, but while in early dev
* stages it gets the job done.
**/
static inline Object* remove_oldest_from_worklist(Worklist* worklist) {
    if (worklist->size == 0) {
        return NULL;
    }
    
    Object* oldest = worklist->data[0];
    
    // Shift all elements to the left
    for (size_t i = 1; i < worklist->size; i++) {
        worklist->data[i - 1] = worklist->data[i];
    }
    
    worklist->size--;
    return oldest;
}

static inline bool is_worklist_empty(Worklist* worklist) 
{
    return worklist->size == 0;
}
