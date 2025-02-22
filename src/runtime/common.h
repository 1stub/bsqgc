#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <assert.h>

#include <threads.h>
#include <sys/mman.h> //mmap

#include "../language/bsqtype.h"

//DEFAULT ENABLED WHILE LOTS OF DEVELOPMENT!!!!
#define BSQ_GC_CHECK_ENABLED
#define MEM_STATS
#define VERBOSE_HEADER

#ifdef BSQ_GC_CHECK_ENABLED
#define ALLOC_DEBUG_MEM_INITIALIZE
#define ALLOC_DEBUG_MEM_DETERMINISTIC
#define ALLOC_DEBUG_CANARY
#endif

#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
// Original address for base was ((void*)(281474976710656ul)), prev was causing MAP_FAILED
#define XALLOC_BASE_ADDRESS ((void*)(0x4000000000ul)) 
#define XALLOC_ADDRESS_SPAN 2147483648ul

#define GC_ALLOC_BASE_ADDRESS ((void*)(0x8000000000ul)) 
#define GC_ALLOC_ADDRESS_SPAN 2147483648ul

#endif

#define PAGE_ADDR_MASK 0xFFFFFFFFFFFFF000ul
//Make sure any allocated page is addressable by us -- larger than 2^31 and less than 2^42
#define MIN_ALLOCATED_ADDRESS ((void*)(2147483648ul))
#define MAX_ALLOCATED_ADDRESS ((void*)(281474976710656ul))

#define BSQ_MEM_ALIGNMENT 8
#define BSQ_BLOCK_ALLOCATION_SIZE 4096ul

//mem is an 8byte alliged pointer and n is the number of 8byte words to clear
void xmem_objclear(void* mem, size_t n);

//Clears a page of memory
void xmem_pageclear(void* mem);

// Gets min and max pointers on a page from any address in the page
#define GET_MIN_FOR_SEGMENT(P, SEG) ((void**)(((uintptr_t)(P) & PAGE_ADDR_MASK) + SEG))
#define GET_MAX_FOR_SEGMENT(P, SEG) ((void**)(((uintptr_t)(P) & PAGE_ADDR_MASK) + SEG + BSQ_BLOCK_ALLOCATION_SIZE - (SEG + sizeof(void*))))

extern mtx_t g_lock;
extern size_t tl_id_counter;

//A handy stack allocation macro
#define BSQ_STACK_ALLOC(SIZE) ((SIZE) == 0 ? nullptr : alloca(SIZE))

#define DEBUG

#ifdef DEBUG
#define debug_print(fmt, ...) \
            fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define debug_print(fmt, ...) \
            do { } while (0)
#endif

/**
* Important Note: For not the following macros to get metadata or alignment
* assume we want a pointer to the start of the actual object, not the start
* of the block.
**/
#define PAGE_MASK_EXTRACT_PTR(O) ((uintptr_t)(O) & PAGE_ADDR_MASK)
#define PAGE_MASK_EXTRACT_PINFO(O) ((PageInfo*)PAGE_MASK_EXTRACT_PTR(O))
#define PAGE_MASK_EXTRACT_DATA(O) ((char*)PAGE_MASK_EXTRACT_PTR(O) + sizeof(PageInfo))
#define PAGE_MASK_EXTRACT_INDEX(O) \
    ((char*)O - PAGE_MASK_EXTRACT_DATA(O)) / REAL_ENTRY_SIZE( PAGE_MASK_EXTRACT_PINFO(O)->entrysize )

#ifdef ALLOC_DEBUG_CANARY
#define PAGE_FIND_OBJ_BASE(O) (PAGE_MASK_EXTRACT_DATA(O) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + (PAGE_MASK_EXTRACT_INDEX(O) \
    * REAL_ENTRY_SIZE( PAGE_MASK_EXTRACT_PINFO(O)->entrysize )))
#define PAGE_IS_OBJ_ALIGNED(O) (((char*)O - (PAGE_MASK_EXTRACT_DATA(O) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData))) \
    % REAL_ENTRY_SIZE( PAGE_MASK_EXTRACT_PINFO(O)->entrysize ) == 0)
#else
#define PAGE_FIND_OBJ_BASE(O) (PAGE_MASK_EXTRACT_DATA(O) + sizeof(MetaData) + (PAGE_MASK_EXTRACT_INDEX(O) \
    * REAL_ENTRY_SIZE( PAGE_MASK_EXTRACT_PINFO(O)->entrysize) ))
#define PAGE_IS_OBJ_ALIGNED(O) (((char*)O - (PAGE_MASK_EXTRACT_DATA(O) + sizeof(MetaData))) \
    % REAL_ENTRY_SIZE( PAGE_MASK_EXTRACT_PINFO(O)->entrysize ) == 0)
#endif

#define GC_GET_META_DATA_ADDR(O) PAGE_IS_OBJ_ALIGNED(O) ? (MetaData*)((char*)O - sizeof(MetaData)) \
    : (MetaData*)(PAGE_FIND_OBJ_BASE(O) - sizeof(MetaData))


#define FREE_LIST_ENTRY_AT(page, index) \
((FreeListEntry*)(PAGE_MASK_EXTRACT_DATA(page) + (index) * REAL_ENTRY_SIZE((page)->entrysize)))

#ifdef ALLOC_DEBUG_CANARY
//Gives us the beginning of block (just before canary in case of canaries enabled)
#define BLOCK_START_FROM_PTR(obj) ((char*)obj - sizeof(MetaData) - ALLOC_DEBUG_CANARY_SIZE)

//Start of our object from the begginning of block (address returned from allocate())
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#define META_FROM_FREELIST_ENTRY(f_entry) ((MetaData*)((char*)f_entry + ALLOC_DEBUG_CANARY_SIZE))
#else
#define BLOCK_START_FROM_PTR(obj) ((char*)obj - sizeof(MetaData))
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData))
#define META_FROM_FREELIST_ENTRY(f_entry) ((MetaData*)f_entry)

#endif

/* Used to determine if a pointer points into the data segment of an object */
#define POINTS_TO_DATA_SEG(P) P >= (void*)PAGE_FIND_OBJ_BASE(P) && P < (void*)((char*)PAGE_FIND_OBJ_BASE(P) + PAGE_MASK_EXTRACT_PINFO(P)->entrysize)

// Allows us to correctly determine pointer offsets
#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

#define MAX_FWD_INDEX UINT32_MAX

#ifdef VERBOSE_HEADER
typedef struct MetaData 
{
    bool isalloc;
    bool isyoung;
    bool ismarked;
    bool isroot;
    uint32_t forward_index;
    uint32_t ref_count;
    struct TypeInfoBase* type;
} MetaData; 
#else
typedef struct MetaData 
{
    uint64_t meta; //8 byte bit vector
} MetaData;
static_assert(sizeof(MetaData) == 8, "MetaData size is not 8 bytes");
#endif

// After we evacuate an object we need to update the original metadata
#define RESET_METADATA_FOR_OBJECT(meta)              \
do {                                                 \
    (meta)->isalloc = false;                         \
    (meta)->isyoung = false;                         \
    (meta)->ismarked = false;                        \
    (meta)->isroot = false;                          \
    (meta)->forward_index = MAX_FWD_INDEX;           \
    (meta)->ref_count = 0;                           \
} while(0)

/* Macro for insertion of PageInfo object into list */
#define INSERT_PAGE_IN_LIST(L, O) \
do {                              \
    if ((L) == NULL) {            \
        (L) = (O);                \
        (O)->next = NULL;         \
    } else {                      \
        (O)->next = (L);          \
        (L) = (O);                \
    }                             \
} while (0)

#ifdef ALLOC_DEBUG_CANARY

#define GC_IS_MARKED(O) (GC_GET_META_DATA_ADDR(O))->ismarked
#define GC_IS_YOUNG(O) (GC_GET_META_DATA_ADDR(O))->isyoung
#define GC_IS_ALLOCATED(O) (GC_GET_META_DATA_ADDR(O))->isalloc
#define GC_IS_ROOT(O) (GC_GET_META_DATA_ADDR(O))->isroot
#define GC_FWD_INDEX(O) (GC_GET_META_DATA_ADDR(O))->forward_index
#define GC_REF_COUNT(O) (GC_GET_META_DATA_ADDR(O))->ref_count
#define GC_TYPE(O) (GC_GET_META_DATA_ADDR(O))->type

#else

#define GC_IS_MARKED(obj)
#define GC_IS_YOUNG(obj)
#define GC_IS_ALLOCATED(obj)
#define GC_IS_ROOT(obj)
#define GC_FWD_INDEX(obj)
#define GC_REF_COUNT(obj)

#endif