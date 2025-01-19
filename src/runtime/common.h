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
