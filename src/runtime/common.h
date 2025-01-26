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

#define BSQ_MEM_ALIGNMENT 8

#define DEBUG

#ifdef DEBUG
#define debug_print(fmt, ...) \
            fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define debug_print(fmt, ...) \
            do { } while (0)
#endif

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
