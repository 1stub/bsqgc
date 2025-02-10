#pragma once

#include "../common.h"
#include <stdio.h>

#include "xalloc.h"

struct ArrayListSegment {
    void* data[BSQ_BLOCK_ALLOCATION_SIZE];
    struct ArrayListSegment* next;
};

struct ArrayList {
    void** head;
    void** tail;
    void** min;
    void** max;
    struct ArrayListSegment* current;
};

#define arraylist_push_head(L, O) if((L).head > (L).min) {*(--(L).head) = O; } else {arraylist_push_head_slow(&(L), O);}
#define arraylist_push_tail(L, O) if((L).tail < (L).max) { *(++(L).tail) = O; } else {arraylist_push_tail_slow(&(L), O);}

#define arraylist_pop_head(T, L) ((T*)((L).head != (L).max ? *(--(L).head) : arraylist_pop_head_slow(&(L))))
#define arraylist_pop_tail(T, L) ((T*)((L).tail != (L).min ? *(--(L).tail) : arraylist_pop_tail_slow(&(L))))

void arraylist_push_head_slow(struct ArrayList* al, void* obj);
void arraylist_push_tail_slow(struct ArrayList* al, void* obj);
void* arraylist_pop_head_slow(struct ArrayList* al);
void* arraylist_pop_tail_slow(struct ArrayList* al);
    
