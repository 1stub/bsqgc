#pragma once

#include "allocator.h"
#include "../support/threadinfo.h"

/**
* OVERVIEW OF GC CODE: 
*   
* This generational garbage collector is designed to have a compacted young space
* and a reference counted old space. The specifics of both have not been perfectly
* implemented thus far, but much of the core logic is present.
*
* EVACUATION:
*    - "evacuate_object(...)" handles movement of objects to evacuation page.
*       The data is directly memcpy'd into the evacuation page, where a forward
*       index is stored in both evac page and old page meta data to allow for easy
*       parent pointer updates. The old location of any object evacuated will be reset
*       in the pending_resets stack.
*    - "finalize_metadata_reset(...)" simply resets all metadata to a state allowing insertion
*       back onto free list.
*    - "update_children_pointers(...)" takes in a parent object and iterates through all children
*       looking for forward table entries. It will then use the childs forward index to update
*       its pointer to said child after it has been evacuated since it will reside in new memory.
*    - "evacuate(...)" iterates through the stack of roots generated in mark_from_roots(...) and
*       handles evacuation, updating parent pointers, and processing of root nodes. It is effectively
*       the glue for the previous three methods.
*
* MARKING: 
*    - "rebuild_freelist(...)" takes in a page and looks for any either non allocated or non referenced       
*       non root objects and puts their slot back onto the freelist.
*    - "clean_nonref_nodes(...)" interates through all pages and looks for nodes that need to have their
*       metadata reset so it can be visible to the freelist. May not be necessary.
*    - "mark_and_evacuate(...)" starts by iterating through our root stack and inserting all items onto a
*       work list. It then iterates through all roots children, and children children, ... looking to 
*       set the mark bit on all nodes reachable. Those not marked will be caught and returned to the
*       freelist when cleaned. It returns a BFS list of all roots and children visible for usage in 
*       evacuation. After marking is done it evacuates and cleans up any junk left over. This is the main
*       method used for actually collecting. 
**/

/* Forward table used in evacuation */
extern struct ArrayList f_table;

/* A collection of roots we can read from when marking */
extern struct ArrayList root_list;

/** 
* We can calculate this prev_roots_set by first ensuring our root list is sorted (quick sort prob)
* based on pointer address. So we can very easily tell whether a object exists by walking the list.
* We need to do some delta computation to determine the difference of addressing for actual insertion
* into this prev_roots_set.
**/
extern struct ArrayList prev_roots_set;

/**
 * Always returns true (for now) 
 **/
bool isRoot(void* obj);

/**
* This method is where we call all marking, evacuation, cleaning methods.
* We will handle checking old roots (isyoung == false) to check that these old roots
* have old children AND that they have non zero references. If any references drops to zero
* we can free from this method.
**/
void collect(AllocatorBin* bin);

/**
 * We have a list containing all children nodes that will need to be moved
 * over to our evacuate page(s). Traverse this list, move nodes, update
 * pointers from their parents.
 **/
void evacuate(struct Stack* marked_nodes_list, AllocatorBin* bin); 

/**
* Iterates through page looking for objects whos ref count has dropped to zero,
* or objects whos alloc flag is not set. Reset these objects metadata then rebuild
* freelist for given page.
**/
void clean_nonref_nodes(AllocatorBin* bin);

/**
 * Process all objects starting from roots in BFS manner
 **/
void mark_and_evacuate(AllocatorBin* bin);

/* Testing */
void walk_stack();

/* Incremented in marking */
static inline void increment_ref_count(Object* obj) {
    MetaData* m = NULL;
    GC_GET_META_DATA_ADDR(obj, m);
    GC_REF_COUNT(m)++;
}

/* Old location decremented in evacuation */
static inline void decrement_ref_count(Object* obj) {   
    MetaData* m = NULL;
    GC_GET_META_DATA_ADDR(obj, m); 
    if(GC_REF_COUNT(m) > 0) {
        GC_REF_COUNT(m)--;
    }

    // Maybe free object if not root and ref count 0 here?
}