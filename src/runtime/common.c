#include "common.h"

mtx_t g_lock;

//Initialize extern variable from common.h
size_t tl_id_counter = 0;

void xmem_objclear(void* mem, size_t n)
{
    void** obj = (void**)mem;
    void** end = obj + n;
    while(obj < end) {
        *obj = NULL;
        obj++;
    }
}

void xmem_pageclear(void* mem)
{
    void** obj = (void**)mem;
    void** end = obj + (BSQ_BLOCK_ALLOCATION_SIZE / sizeof(void*));
    while(obj < end) {
        *obj = NULL;
        obj++;
    }
}
