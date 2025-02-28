#pragma once

#include "allocator.h"
#include "../support/threadinfo.h"

/**
*   
* This generational garbage collector is designed to have a compacted young space
* and a reference counted old space. 
*
**/

/**
* Wrapper around marking, evacuation, and rebuilding. Useful for 
* throwing other helper methods in here
**/
void collect();

/**
 * Evacuate objects, making them old. Update parent pointers.
 **/
void evacuate(); 

/**
 * Process all objects starting from roots in BFS manner
 **/
void mark_and_evacuate();

/* Testing */
void walk_stack(struct WorkList* worklist);

/* Incremented in marking */
static inline void increment_ref_count(void* obj) 
{
    GC_REF_COUNT(obj)++;
}

/* Old location decremented in evacuation */
static inline void decrement_ref_count(void* obj) 
{   
    if(GC_REF_COUNT(obj) > 0) {
        GC_REF_COUNT(obj)--;
    }

    // might make more sense to put back on freelist here if refcnt 0
}