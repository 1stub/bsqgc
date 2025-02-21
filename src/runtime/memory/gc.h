#pragma once

#include "allocator.h"
#include "../support/threadinfo.h"

/**
*   
* This generational garbage collector is designed to have a compacted young space
* and a reference counted old space. The specifics of both have not been perfectly
* implemented thus far, but much of the core logic is present.
*
**/

/**
* Not implemented yet, but we use this for determining eligibility for deletion.
* Whenever we find a "prev root" that is not in "current roots" with a ref count
* of zero, he is eligible for deletion.
**/
// extern struct ArrayList prev_roots_set;

/**
* Wrapper around marking, evacuation, and rebuilding.
* Exists incase more logic needs to be added in addition to those aforementioned
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
static inline void increment_ref_count(void* obj) {
    GC_REF_COUNT(obj)++;
}

/* Old location decremented in evacuation */
static inline void decrement_ref_count(void* obj) {   
    if(GC_REF_COUNT(obj) > 0) {
        GC_REF_COUNT(obj)--;
    }

    // Maybe free object if not root and ref count 0 here?
}