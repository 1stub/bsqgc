#pragma once

#include "../common.h"

#ifdef MEM_STATS
#include <stdio.h> //printf
#endif

#ifdef BSQ_GC_CHECK_ENABLED
#define ALLOC_DEBUG_MEM_INITIALIZE
#define ALLOC_DEBUG_CANARY
#endif

//Can also use other values like 0xFFFFFFFFFFFFFFFFul
#define ALLOC_DEBUG_MEM_INITIALIZE_VALUE 0x0ul

//Must be multiple of 8
#define ALLOC_DEBUG_CANARY_SIZE 16
#define ALLOC_DEBUG_CANARY_VALUE 0xDEADBEEFDEADBEEFul

#define MAX_ROOTS 100

/* This queue size will need to be tinkered with */
#define WORKLIST_CAPACITY 1024

/*Negative offset to find metadata assuming obj is the start of data seg*/
#define META_FROM_OBJECT(obj) ((MetaData*)((char*)(obj) - sizeof(MetaData)))

#ifdef MEM_STATS
#define ENABLE_MEM_STATS
#define MEM_STATS_OP(X) X
#define MEM_STATS_ARG(X) X
#else
#define MEM_STATS_OP(X)
#define MEM_STATS_ARG(X)
#endif

#define SETUP_META_FLAGS(meta)           \
do {                                     \
    (*meta)->isalloc = true;             \
    (*meta)->isyoung = true;             \
    (*meta)->ismarked = false;           \
    (*meta)->isroot = false;             \
    (*meta)->forward_index = UINT32_MAX; \
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

typedef struct {
    Object* data[WORKLIST_CAPACITY];
    size_t size;
} Worklist;

extern Object* root_stack[MAX_ROOTS];
extern size_t root_count;

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
AllocatorBin* initializeAllocatorBin(uint16_t entrysize, PageManager* page_manager);

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
void evacuate(Worklist* marked_nodes_list); 

/**
 * Method(s) for iterating through the root stack and marking all elements
 * inside said stack.
 **/
void mark_from_roots();

/**
 * Traverse pages and freelists ensuring no canaries are clobbered and that
 * our freelists contain no already allocated objects.
 **/
#ifdef ALLOC_DEBUG_CANARY
void verifyAllCanaries(AllocatorBin* bin);
void verifyCanariesInPage(AllocatorBin* bin);
bool verifyCanariesInBlock(char* block, uint16_t entry_size);
#endif

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
 * Allocate a block of memory of size `size` from the given page
 **/
static inline void* allocate(AllocatorBin* alloc, MetaData** metadata)
{
    if(alloc->freelist == NULL) {
        getFreshPageForAllocator(alloc);
    }

    FreeListEntry* ret = alloc->freelist;
    alloc->freelist = ret->next;

    void* obj;

    #ifndef ALLOC_DEBUG_CANARY
    *metadata = (MetaData*)ret;
    obj = (void*)((uint8_t*)ret + sizeof(MetaData));
    #else
    obj = setupSlowPath(ret, alloc, metadata);
    #endif

    // Do I really want this to always create an object of the same
    // format as our object struct (in common.h)? design choice...
    Object* new_obj = (Object*)obj;
    new_obj->num_children = 0;

    SETUP_META_FLAGS(metadata);

    return (void*)obj;
}