#pragma once

#include "../common.h"
#include <stdio.h>

#include "xalloc.h"

#define WL_SEG_SIZE sizeof(struct WorkListSegment)

/* just need to be able to push like a normal stack and remove the oldest element */
struct WorkListSegment {
    void** data;
    struct WorkListSegment* next;
};

struct WorkList {
    void** head;
    void** tail;
    struct WorkListSegment* head_segment;
    struct WorkListSegment* tail_segment;
};

#define worklist_push(L, O) if((L).tail && (L).tail < GET_MAX_FOR_SEGMENT((L).tail, WL_SEG_SIZE)) { *(++(L).tail) = O; } else { worklist_push_slow(&(L), O);}
#define worklist_pop(T, L) ((T*)((L).head != (L).tail ? *((L).head++) : worklist_pop_slow(&(L))))

void worklist_initialize(struct WorkList* l);
void worklist_push_slow(struct WorkList* l, void* obj);
void* worklist_pop_slow(struct WorkList* l);
bool worklist_is_empty(struct WorkList* l);