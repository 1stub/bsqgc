#pragma once

#include "../common.h"
#include <stdio.h>

#include "xalloc.h"

template <typename T>
struct WorkListSegment
{
    T* data;
    WorkListSegment* next;
};

template <typename T>
class WorkList
{
private:
    T* head;
    T* tail;
    struct WorkListSegment<T>* head_segment;
    struct WorkListSegment<T>* tail_segment;

public:
    WorkList(): head(nullptr), tail(nullptr), head_segment(nullptr), tail_segment(nullptr) {}
};

#define WL_SEG_SIZE sizeof(struct WorkListSegment)

/* just need to be able to push like a normal stack and remove the oldest element */
struct xWorkListSegment {
    void** data;
    struct WorkListSegment* next;
};

struct xWorkList {
    void** head;
    void** tail;
    struct WorkListSegment* head_segment;
    struct WorkListSegment* tail_segment;
};

#define worklist_push(L, O) if((L).tail && (L).tail < GET_MAX_FOR_SEGMENT((L).tail, WL_SEG_SIZE)) { *(++(L).tail) = O; } else { worklist_push_slow(&(L), O);}
#define worklist_pop(T, L) ((T*)((L).head != (L).tail ? *((L).head++) : worklist_pop_slow(&(L))))

void worklist_push_slow(struct WorkList* l, void* obj);
void* worklist_pop_slow(struct WorkList* l);
bool worklist_is_empty(struct WorkList* l);