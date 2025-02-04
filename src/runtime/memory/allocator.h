#pragma once

#include "../common.h"
#include "../arraylist.h"
#include "../stack.h"

#ifdef MEM_STATS
#include <stdio.h> //printf
#endif

#include <string.h> //memcpy

//Can also use other values like 0xFFFFFFFFFFFFFFFFul
#define ALLOC_DEBUG_MEM_INITIALIZE_VALUE 0x0ul

//Must be multiple of 8
#define ALLOC_DEBUG_CANARY_SIZE 16
#define ALLOC_DEBUG_CANARY_VALUE 0xDEADBEEFDEADBEEFul

#ifdef MEM_STATS
#define ENABLE_MEM_STATS
#define MEM_STATS_OP(X) X
#define MEM_STATS_ARG(X) X
#else
#define MEM_STATS_OP(X)
#define MEM_STATS_ARG(X)
#endif

#define SETUP_META_FLAGS(meta)             \
do {                                       \
    (meta)->isalloc = true;                \
    (meta)->isyoung = true;                \
    (meta)->ismarked = false;              \
    (meta)->isroot = false;                \
    (meta)->forward_index = MAX_FWD_INDEX; \
    (meta)->ref_count = 0;                 \
} while(0)

////////////////////////////////
//Memory allocator

#define BSQ_STACK_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : alloca(SIZE))

//Block allocation size
#define BSQ_BLOCK_ALLOCATION_SIZE 4096ul

//Make sure any allocated page is addressable by us -- larger than 2^31 and less than 2^42
#define MIN_ALLOCATED_ADDRESS 2147483648ul
#define MAX_ALLOCATED_ADDRESS 281474976710656ul

struct FreeListEntry
{
   struct FreeListEntry* next;
};
typedef struct FreeListEntry FreeListEntry;

static_assert(sizeof(FreeListEntry) <= sizeof(MetaData), "BlockHeader size is not 8 bytes");

typedef uint16_t PageStateInfo;
#define PageStateInfo_GroundState 0x0
#define AllocPageInfo_ActiveAllocation 0x1
#define AllocPageInfo_ActiveEvacuation 0x2

typedef struct PageInfo
{
    FreeListEntry* freelist; //allocate from here until nullptr

    uint16_t entrysize; //size of the alloc entries in this page (excluding metadata)
    uint16_t entrycount; //max number of objects that can be allocated from this Page

    uint16_t freecount;

    PageStateInfo pagestate;

    struct PageInfo* next; //need this for allocator page management
} PageInfo;

typedef struct PageManager{
    PageInfo* all_pages;
    PageInfo* evacuate_page;
    PageInfo* filled_pages; //Array list?
} PageManager;
extern PageManager p_mgr;

typedef struct AllocatorBin
{
    FreeListEntry* freelist;
    uint16_t entrysize;
    PageInfo* page;
    PageManager* page_manager;
} AllocatorBin;
extern AllocatorBin a_bin;

extern ArrayList f_table;

/* A collection of roots we can read from when marking */
extern ArrayList root_list;

/* Pages that are empty --- avoid munmapping excessively */
extern Stack backstore_pages;

/**
 * Always returns true (for now) since it only gets called from allcoate.
 **/
bool isRoot(void* obj);

/**
 * When needed, get a fresh page from mmap to allocate from 
 **/
void getFreshPageForAllocator(AllocatorBin* alloc);

/**
 * For our allocator to be usable, the AllocatorBin must be initialized
 **/
AllocatorBin* initializeAllocatorBin(uint16_t entrysize);

/**
 * Setup pointers for managing our pages 
 * we have a list of all pages and those that have stuff in them
 **/
PageManager* initializePageManager(uint16_t entry_size);

/**
 * We have a list containing all children nodes that will need to be moved
 * over to our evacuate page(s). Traverse this list, move nodes, update
 * pointers from their parents.
 **/
void evacuate(Stack* marked_nodes_list, AllocatorBin* bin); 

/**
 * Process all objects starting from roots in BFS manner
 **/
void mark_from_roots(AllocatorBin* bin);

/* Incremented in marking */
static inline void increment_ref_count(Object* obj) {
    META_FROM_OBJECT(obj)->ref_count++;
}

/* Old location decremented in evacuation */
static inline void decrement_ref_count(Object* obj) {
    MetaData* meta = META_FROM_OBJECT(obj);
    
    if(meta->ref_count > 0) {
        meta->ref_count--;
    }

    // Maybe free object if not root and ref count 0 here?
    // Hard to say since it will get caught when rebuilding the page
}


/**
 * Slow path for usage with canaries --- debug
 **/
static inline void* setupSlowPath(FreeListEntry* ret, AllocatorBin* alloc, MetaData** meta){
    uint64_t* pre = (uint64_t*)ret;
    *pre = ALLOC_DEBUG_CANARY_VALUE;

    uint64_t* post = (uint64_t*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + alloc->entrysize);
    *post = ALLOC_DEBUG_CANARY_VALUE;

    *meta = (MetaData*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE);

    return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
}

/**
 * Allocate a block of memory of size `size` from the given page. If preserve_meta is true 
 * we are using allocate for moving object to new pages.
 **/
static inline void* allocate(AllocatorBin* alloc, MetaData* metadata)
{
    /* If meta is not null we do not want to overwrite any data in it */
    bool preserve_meta = (metadata != NULL);

    if(alloc->freelist == NULL) {
        // Use backstore of empty pages to avoid munmap or mmap excessively
        if(!s_is_empty(&backstore_pages)) {
            alloc->page = (PageInfo*)s_pop(&backstore_pages);
            alloc->freelist = alloc->page->freelist;
        } else {
            getFreshPageForAllocator(alloc);
        }
    }

    FreeListEntry* ret = alloc->freelist;
    alloc->freelist = ret->next;

    void* obj;

    #ifndef ALLOC_DEBUG_CANARY
    if(!preserve_meta) *metadata = (MetaData*)ret;
    obj = (void*)((uint8_t*)ret + sizeof(MetaData));
    #else
    if (preserve_meta) {
        obj = (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
    } else {
        obj = setupSlowPath(ret, alloc, &metadata);
    }    
    #endif

    // Do I really want this to always create an object of the same
    // format as our object struct (in common.h)? design choice...
    Object* new_obj = (Object*)obj;
    new_obj->num_children = 0;

    // If meta was created initialize object
    if(!preserve_meta) {
        SETUP_META_FLAGS(metadata);
    }

    return (void*)obj;
}
