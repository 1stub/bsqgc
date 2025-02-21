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

/* Idk of a good forever home for these canary methods */
#if 0

/* Following 3 methods verify integrity of canaries */
static bool verifyCanariesInBlock(char* block, uint16_t entry_size)
{
    uint64_t* pre_canary = (uint64_t*)(block);
    uint64_t* post_canary = (uint64_t*)(block + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);

    debug_print("[CANARY_CHECK] Verifying canaries for block at %p\n", (void*)block);
    debug_print("\tPre-canary value: %lx\n", *pre_canary);
    debug_print("\tPost-canary value: %lx\n", *post_canary);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        debug_print("[ERROR] Canary corruption detected at block %p\n", (void*)block);
        return false;
    }
    return true;
}

static void verifyCanariesInPage(PageInfo* page)
{
    FreeListEntry* list = page->freelist;
    char* base_address = (char*)page + sizeof(PageInfo);
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    debug_print("[CANARY_CHECK] Verifying canaries for page at %p\n", (void*)page);

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        debug_print("\tChecking block %d at address %p\n", i, block_address);
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tBlock %d metadata state: isalloc=%d\n", i, metadata->isalloc);

        if (metadata->isalloc) {
            debug_print("\tAllocated block detected, verifying canaries...\n");
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }
    debug_print("\n");

    debug_print("[CANARY_CHECK] Verifying freelist for page at %p\n", (void*)page);
    while(list){
        debug_print("\tChecking freelist block at %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tFreelist block metadata state: isalloc=%d\n", metadata->isalloc);

        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated at %p\n", (void*)list);
            assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    // Make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);

    debug_print("\n");
}

static void verifyAllCanaries(AllocatorBin* bin)
{
    PageInfo* current_page = bin->page_manager->all_pages;
    PageInfo* evac_page = bin->page_manager->evacuate_page;

    debug_print("[CANARY_CHECK] Verifying all pages in bin\n");

    while (current_page) {
        debug_print("[CANARY_CHECK] Verifying canaries in page at %p (all_pages)\n", (void*)current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }

    while (evac_page) {
        debug_print("[CANARY_CHECK] Verifying canaries in page at %p (evacuate_page)\n", (void*)evac_page);
        verifyCanariesInPage(evac_page);
        evac_page = evac_page->next;
    }

}

#endif