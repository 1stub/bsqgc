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

#ifdef MEM_STATS
#define ENABLE_MEM_STATS
#define MEM_STATS_OP(X) X
#define MEM_STATS_ARG(X) X
#else
#define MEM_STATS_OP(X)
#define MEM_STATS_ARG(X)
#endif

#define SETUP_META_FLAGS(meta)  \
do {                      \
    (*meta)->isalloc = true; \
    (*meta)->isyoung = true; \
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

/**
 * The layout of these structs is critical. We rely on the fact that the address of the struct
 * matches the address of its first element (PageInfo in PageManager) to avoid using malloc.
 * If the layout changes, this initialization method may break
 **/
typedef struct PageManager{
    PageInfo* all_pages;
    PageInfo* need_collection; 
} PageManager;

typedef struct AllocatorBin
{
    FreeListEntry* freelist;
    uint16_t entrysize;
    PageInfo* page;
    PageManager* page_manager;
} AllocatorBin;

/**
 * Validation to check canary failures and meta flag issues
 **/
#ifdef ALLOC_DEBUG_CANARY
static inline bool validate(void* obj, AllocatorBin* bin, MetaData* meta)
{
    if (obj == NULL || meta == NULL || bin == NULL) {
        return false;
    }

    //check canary before metadata and canary after data
    uint64_t* pre_canary = (uint64_t*)((char*)obj - sizeof(MetaData) - ALLOC_DEBUG_CANARY_SIZE);
    uint64_t* post_canary = (uint64_t*)((char*)obj + bin->entrysize);

    printf("Pre Canary %p: %lx\n", pre_canary, *pre_canary);
    printf("Post Canary %p: %lx\n", post_canary, *post_canary);

    if(*post_canary != ALLOC_DEBUG_CANARY_VALUE || *pre_canary != ALLOC_DEBUG_CANARY_VALUE){
        printf("[ERROR] Canary check failed!: pre=%lx, post=%lx\n", *pre_canary, *post_canary);
        return false;
    }

    //now lets check metadata, bpth should be true given we run this just after allocation
    if(!meta->isalloc || !meta->isyoung){
        printf("[ERROR] MetaData Check Failed!\n");
        return false;
    }

    return true;
}
#endif

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
 * Slow path for debugging stuffs
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

    SETUP_META_FLAGS(metadata);

    return (void*)obj;
}

#ifdef ALLOC_DEBUG_CANARY
extern void runTests();
#endif
