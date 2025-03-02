#pragma once

#include "../language/bsqtype.h"

#include <sys/mman.h> //mmap

//DEFAULT ENABLED WHILE LOTS OF DEVELOPMENT!!!!
#define BSQ_GC_CHECK_ENABLED
#define MEM_STATS
#define VERBOSE_HEADER

#ifdef BSQ_GC_CHECK_ENABLED
#define ALLOC_DEBUG_MEM_INITIALIZE
#define ALLOC_DEBUG_MEM_DETERMINISTIC
#define ALLOC_DEBUG_CANARY
#define DSA_INVARIANTS
#endif

#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
#define ALLOC_BASE_ADDRESS ((void*)(0x4000000000ul)) 
#define ALLOC_ADDRESS_SPAN 2147483648ul
#endif

#define PAGE_ADDR_MASK 0xFFFFFFFFFFFFF000ul
//Make sure any allocated page is addressable by us -- larger than 2^31 and less than 2^42
#define MIN_ALLOCATED_ADDRESS ((void*)(2147483648ul))
#define MAX_ALLOCATED_ADDRESS ((void*)(281474976710656ul))

#define BSQ_MEM_ALIGNMENT 8
#define BSQ_BLOCK_ALLOCATION_SIZE 4096ul

//mem is an 8byte alliged pointer and n is the number of 8byte words to clear
inline void xmem_zerofill(void* mem, size_t n) noexcept
{
    void** obj = (void**)mem;
    void** end = obj + n;
    while(obj < end) {
        *obj = NULL;
        obj++;
    }
}

//Clears a page of memory
inline void xmem_zerofillpage(void* mem) noexcept
{
    void** obj = (void**)mem;
    void** end = obj + (BSQ_BLOCK_ALLOCATION_SIZE / sizeof(void*));
    while(obj < end) {
        *obj = NULL;
        obj++;
    }
}

//A global mutex lock that all threads will use when accessing shared page lists 
extern mtx_t g_alloclock;

#define ALLOC_LOCK_INIT() assert(mtx_init(&g_alloclock, mtx_plain) == thrd_success)
#define ALLOC_LOCK_ACQUIRE() assert(mtx_lock(&g_alloclock) == thrd_success)
#define ALLOC_LOCK_RELEASE() assert(mtx_unlock(&g_alloclock) == thrd_success)

//A global mutex lock that all threads will use when doing shared GC ops (e.g. getting pages, root resolution, or when doing their inc/dec ref loops)
extern mtx_t g_gclock;

#define GC_LOCK_INIT() assert(mtx_init(&g_gclock, mtx_plain) == thrd_success)
#define GC_LOCK_ACQUIRE() assert(mtx_lock(&g_gclock) == thrd_success)
#define GC_LOCK_RELEASE() assert(mtx_unlock(&g_gclock) == thrd_success)

// Track information that needs to be globally accessible for threads
class GlobalThreadAllocInfo
{
public:
    static size_t s_thread_counter;
    static void* s_current_page_address;

    //TODO: if we need to do deterministic replay we can add a thread page-get buffer here to record/replay from
};

//A handy stack allocation macro
#define BSQ_STACK_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : alloca(SIZE))

// Allows us to correctly determine pointer offsets
#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

#define MAX_FWD_INDEX UINT32_MAX

/**
* If we decide every 4mb we run a collection, we can just force the max number of roots to be capped
* at 4mb. This even accounts for worst case where every single object is a root AND all new/old only
*
**/
#define GC_COLLECTION_THRESHOLD 4000000 //4mb
#define MAX_ROOTS GC_COLLECTION_THRESHOLD //maybe some other stuff

#ifdef VERBOSE_HEADER
struct MetaData 
{
    bool isalloc;
    bool isyoung;
    bool ismarked;
    bool isroot;
    uint32_t forward_index;
    uint32_t ref_count;
    TypeInfoBase* type;
}; 
#else
typedef struct MetaData 
{
    uint64_t meta; //8 byte bit vector
} MetaData;
static_assert(sizeof(MetaData) == 8, "MetaData size is not 8 bytes");
#endif

#define GC_GET_META_DATA_ADDR(O) ((MetaData*)((char*)O - sizeof(MetaData)))

#define GC_IS_MARKED(O) (GC_GET_META_DATA_ADDR(O))->ismarked
#define GC_IS_YOUNG(O) (GC_GET_META_DATA_ADDR(O))->isyoung
#define GC_IS_ALLOCATED(O) (GC_GET_META_DATA_ADDR(O))->isalloc
#define GC_IS_ROOT(O) (GC_GET_META_DATA_ADDR(O))->isroot
#define GC_FWD_INDEX(O) (GC_GET_META_DATA_ADDR(O))->forward_index
#define GC_REF_COUNT(O) (GC_GET_META_DATA_ADDR(O))->ref_count
#define GC_TYPE(O) (GC_GET_META_DATA_ADDR(O))->type
