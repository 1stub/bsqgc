#pragma once
/*
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

    struct PageInfo* next;
} PageInfo;

typedef struct PageManager{
    PageInfo* low_utilization_pages; // Pages with 1-30% utilization (does not hold fully empty)
    PageInfo* mid_utilization_pages; // Pages with 31-85% utilization
    PageInfo* high_utilization_pages; // Pages with 86-100% utilization 

    PageInfo* filled_pages; // Completly empty pages
    PageInfo* empty_pages; // Completeyly full pages
} PageManager;

typedef struct AllocatorBin
{
    FreeListEntry* freelist;
    uint16_t entrysize;

    //Proved to be difficult to work with sorting, using static arrays for now
    void* roots[MAX_ROOTS];
    void* old_roots[MAX_ROOTS];

    size_t roots_count;
    size_t old_roots_count;

    struct WorkList pending_decs;

    PageInfo* alloc_page; // Page in which we are currently allocating from
    PageInfo* evac_page; // Page in which we are currently evacuating from
    PageManager* page_manager;
} AllocatorBin;

//Since entry sizes varry, we can statically declare bins & page managers to avoid any mallocs
extern AllocatorBin a_bin8;
extern PageManager p_mgr8;

extern AllocatorBin a_bin16;
extern PageManager p_mgr16;

void getFreshPageForAllocator(AllocatorBin* alloc);
void getFreshPageForEvacuation(AllocatorBin* alloc); 

PageInfo* getPageFromManager(PageManager* pm, uint16_t entrysize);

AllocatorBin* getBinForSize(uint16_t entrytsize);

PageManager* initializePageManager(uint16_t entry_size);
PageInfo* allocateFreshPage(uint16_t entrysize);

void verifyAllCanaries();
void verifyCanariesInPage(PageInfo* page);
bool verifyCanariesInBlock(char* block, uint16_t entry_size);

static inline void* setupSlowPath(FreeListEntry* ret, AllocatorBin* alloc){
    uint64_t* pre = (uint64_t*)ret;
    *pre = ALLOC_DEBUG_CANARY_VALUE;

    uint64_t* post = (uint64_t*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + alloc->entrysize);
    *post = ALLOC_DEBUG_CANARY_VALUE;

    return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
}

static inline void* allocate(AllocatorBin* alloc, struct TypeInfoBase* type)
{
    assert(alloc->entrysize == type->type_size);

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

    alloc->alloc_page->freecount--;

    debug_print("Allocated object at %p\n", obj);
    return (void*)obj;
}
*/