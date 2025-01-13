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

#ifdef VERBOSE_HEADER
typedef struct MetaData 
{
    bool isalloc;
    bool isyoung;

    uint8_t padding[6]; //ensure at least 8 bytes in size
} MetaData;
#else
typedef struct MetaData 
{
    uint64_t meta; //8 byte bit vector
} MetaData;

static_assert(sizeof(MetaData) == 8, "MetaData size is not 8 bytes");
#endif
