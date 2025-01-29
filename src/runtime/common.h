#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdalign.h>
#include <assert.h>
#include <sys/mman.h> //mmap

//DEFAULT ENABLED WHILE LOTS OF DEVELOPMENT!!!!
#define BSQ_GC_CHECK_ENABLED
#define MEM_STATS
#define VERBOSE_HEADER

#ifdef BSQ_GC_CHECK_ENABLED
#define ALLOC_DEBUG_MEM_INITIALIZE
#define ALLOC_DEBUG_CANARY
#endif

#define BSQ_MEM_ALIGNMENT 8

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

#ifdef ALLOC_DEBUG_CANARY
//Gives us the beginning of block (just before canary in case of canaries enabled)
#define BLOCK_START_FROM_OBJ(obj) ((char*)obj - sizeof(MetaData) - ALLOC_DEBUG_CANARY_SIZE)

//Start of our object from the begginning of block (address returned from allocate())
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define BLOCK_START_FROM_OBJ(obj) ((char*)obj - sizeof(MetaData))
#define OBJ_START_FROM_BLOCK(obj) ((char*)obj + sizeof(MetaData))
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
} while(0)

