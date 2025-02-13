#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <assert.h>

#include <threads.h>
#include <sys/mman.h> //mmap

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

/* Maximum number of roots on our root stack */
#define MAX_ROOTS 100

/*Negative offset to find metadata assuming obj is the start of data seg*/
#define META_FROM_OBJECT(obj) ((MetaData*)((char*)(obj) - sizeof(MetaData)))

/** 
* Hard defining maximum number of possible children for an obj,
* I suspect this should not be the case but works for testing.
**/
#define MAX_CHILDREN 16

/** 
* Our object struct to allow nesting of children for exploring
* and marking our graph of objects
**/
typedef struct Object{
    struct Object* children[MAX_CHILDREN];
    uint16_t num_children;
}Object;

#define PAGE_OFFSET(p) (char*)p + sizeof(PageInfo)
#define OBJECT_AT(page, index) \
    ((Object*)(PAGE_OFFSET(page) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + \
    ((index) * REAL_ENTRY_SIZE((page)->entrysize))))
#define FREE_LIST_ENTRY_AT(page, index) \
((FreeListEntry*)(PAGE_OFFSET(page) + (index) * REAL_ENTRY_SIZE((page)->entrysize)))

#ifdef ALLOC_DEBUG_CANARY
//Gives us the beginning of block (just before canary in case of canaries enabled)
#define BLOCK_START_FROM_OBJ(obj) ((char*)obj - sizeof(MetaData) - ALLOC_DEBUG_CANARY_SIZE)

//Start of our object from the begginning of block (address returned from allocate())
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#define META_FROM_FREELIST_ENTRY(f_entry) ((MetaData*)((char*)f_entry + ALLOC_DEBUG_CANARY_SIZE))
#else
#define BLOCK_START_FROM_OBJ(obj) ((char*)obj - sizeof(MetaData))
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData))
#define META_FROM_FREELIST_ENTRY(f_entry) ((MetaData*)f_entry)

#endif

/** 
* As this project grows it would be best if we instead predefine multiple
* entry sizes depending on the type of objects to be allocated
**/
#define DEFAULT_ENTRY_SIZE sizeof(Object) 

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
} MetaData; // We want meta to be 8 bytes 
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


#ifdef ALLOC_DEBUG_CANARY

#define GC_IS_MARKED(obj) META_FROM_OBJECT(obj)->ismarked
#define GC_IS_YOUNG(obj) META_FROM_OBJECT(obj)->isyoung
#define GC_IS_ALLOCATED(obj) META_FROM_OBJECT(obj)->isalloc
#define GC_IS_ROOT(obj) META_FROM_OBJECT(obj)->isroot
#define GC_FWD_INDEX(obj) META_FROM_OBJECT(obj)->forward_index
#define GC_REF_COUNT(obj) META_FROM_OBJECT(obj)->ref_count

#else

#define GC_IS_MARKED(obj)
#define GC_IS_YOUNG(obj)
#define GC_IS_ALLOCATED(obj)
#define GC_IS_ROOT(obj)
#define GC_FWD_INDEX(obj)
#define GC_REF_COUNT(obj)

#endif