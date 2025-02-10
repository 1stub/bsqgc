#pragma once

/** 
* This Array List implementation is NOT perfect. It does boast dynamic resizing utilizing fixed sized pages
* which is really nice. There are most definitely bugs that I have not found or logic errors, but for the
* sake of having a paging array list its fine for now. Needs some more work and I suspect the more I stare
* at it the more stupid bugs I will notice or flaws in my logic. 
**/

#include "../common.h"
#include <stdio.h>

#include "xalloc.h"

struct ArrayListSegment {
    void** data;
    struct ArrayListSegment* next;
    struct ArrayListSegment* prev;
};

struct ArrayList {
    void** head;
    void** tail;
    struct ArrayListSegment* head_segment;
    struct ArrayListSegment* tail_segment;    
};

/* Gets min and max pointers on a page from any address in the page, intended to be passed in an object */
#define GET_MIN(P) ((void**)(((uintptr_t)(P) & PAGE_ADDR_MASK) + sizeof(struct ArrayListSegment)))
#define GET_MAX(P) ((void**)(((uintptr_t)(P) & PAGE_ADDR_MASK) + sizeof(struct ArrayListSegment) + BSQ_BLOCK_ALLOCATION_SIZE - (sizeof(struct ArrayListSegment) + sizeof(void*))))

#define arraylist_push_head(L, O) if((L).head && (L).head > GET_MIN((L).head)) {*(--(L).head) = O; } else {arraylist_push_head_slow(&(L), O);}
#define arraylist_push_tail(L, O) if((L).tail && (L).tail < GET_MAX((L).tail)) { *(++(L).tail) = O; } else {arraylist_push_tail_slow(&(L), O);}

#define arraylist_pop_head(T, L) ((T*)((L).head != GET_MAX((L).head) ? *((L).head++) : arraylist_pop_head_slow(&(L))))
#define arraylist_pop_tail(T, L) ((T*)((L).tail != GET_MIN((L).tail) ? *((L).tail--) : arraylist_pop_tail_slow(&(L))))

void arraylist_initialize(struct ArrayList* al);
void arraylist_push_head_slow(struct ArrayList* al, void* obj);
void arraylist_push_tail_slow(struct ArrayList* al, void* obj);
void* arraylist_pop_head_slow(struct ArrayList* al);
void* arraylist_pop_tail_slow(struct ArrayList* al);
    
