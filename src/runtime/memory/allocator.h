#pragma once

#include "../common.h"
#include "../support/arraylist.h"
#include "../support/stack.h"
#include "../support/worklist.h"
#include "../support/pagetable.h"

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

#define SETUP_META_FLAGS(M, T)          \
do {                                    \
    (M)->isalloc = true;                \
    (M)->isyoung = true;                \
    (M)->ismarked = false;              \
    (M)->isroot = false;                \
    (M)->forward_index = MAX_FWD_INDEX; \
    (M)->ref_count = 0;                 \
    (M)->type = T;                      \
} while(0)

////////////////////////////////
//Memory allocator

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
extern AllocatorBin a_bin8;
extern AllocatorBin a_bin16;

/**
 * When needed, get a fresh page from mmap to allocate from 
 **/
void getFreshPageForAllocator(AllocatorBin* alloc);

/**
 * For our allocator to be usable, the AllocatorBin must be initialized
 **/
AllocatorBin* initializeAllocatorBin(uint16_t entrysize);

/**
* Since our bins can vary in size (always multiple of 8 bytes) depending on the type they hold
* we need to be able to "dynamically" determine what bin is appropriate given an objects size.
**/
AllocatorBin* getBinForSize(uint16_t entrytsize);

/**
 * Setup pointers for managing our pages 
 * we have a list of all pages and those that have stuff in them
 **/
PageManager* initializePageManager(uint16_t entry_size);

/**
* Allocates fresh page without updates of allocator bin
**/
PageInfo* allocateFreshPage(uint16_t entrysize);

/**
 * Slow path for usage with canaries --- debug
 **/
static inline void* setupSlowPath(FreeListEntry* ret, AllocatorBin* alloc){
    uint64_t* pre = (uint64_t*)ret;
    *pre = ALLOC_DEBUG_CANARY_VALUE;

    uint64_t* post = (uint64_t*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + alloc->entrysize);
    *post = ALLOC_DEBUG_CANARY_VALUE;

    return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
}

/**
 * Allocate a block of memory of size `size` from the given page.
 **/
static inline void* allocate(AllocatorBin* alloc, struct TypeInfoBase* type)
{
    if(alloc->freelist == NULL) {
        getFreshPageForAllocator(alloc);
    }

    FreeListEntry* ret = alloc->freelist;
    alloc->freelist = ret->next;

    #ifndef ALLOC_DEBUG_CANARY
    void* obj = (void*)((uint8_t*)ret + sizeof(MetaData));
    #else
    void* obj = setupSlowPath(ret, alloc);
    #endif

    MetaData* mdata = (MetaData*)((char*)obj - sizeof(MetaData));
    SETUP_META_FLAGS(mdata, type);

    return (void*)obj;
}
