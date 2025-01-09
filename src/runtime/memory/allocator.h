#pragma once

#include "../common.h"


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

////////////////////////////////
//Memory allocator

#define BSQ_STACK_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : alloca(SIZE))

//Block allocation size
#define BSQ_BLOCK_ALLOCATION_SIZE 4096ul

//Make sure any allocated page is addressable by us -- larger than 2^31 and less than 2^42
#define MIN_ALLOCATED_ADDRESS 2147483648ul
#define MAX_ALLOCATED_ADDRESS 281474976710656ul

alignas(BSQ_MEM_ALIGNMENT) typedef struct FreeListEntry {
   struct FreeListEntry* next;
} FreeListEntry;

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
} PageInfo;
