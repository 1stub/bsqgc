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

#ifdef VERBOSE_HEADER
typedef struct MetaData 
{
    bool isalloc;
    bool isyoung;
    bool ismarked;

    uint8_t padding[5]; //ensure at least 8 bytes in size
} MetaData;
#else
typedef struct MetaData 
{
    uint64_t meta; //8 byte bit vector
} MetaData;

static_assert(sizeof(MetaData) == 8, "MetaData size is not 8 bytes");
#endif
