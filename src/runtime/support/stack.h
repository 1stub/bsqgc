#pragma once

#include "xalloc.h"

typedef struct {
    void* data;

    struct StackSegment* next;
} StackSegment;

typedef struct {
    void* top;
    void* max;
    StackSegment* current;
} Stack;

extern thread_local Stack marking_stack;

void stack_push_slow(Stack* s, void* obj);

//ALWAYS CALL AT TOP LEVEL WITH SIMPLE ARGUMENTS
#define stack_push(S, obj) if((S).top != (S).max) { *((S).top++) = obj; } else { stack_push_slow(&(S), obj); }

static inline void s_push(Stack* s, Object* obj) {
    if(s->size >= MAX_STACK_SIZE) {
        debug_print("[ERROR] Stack exceeded bounds!\n");
        return ;
    }
    s->data[s->size++] = obj;
}

static inline Object* s_pop(Stack* s) {
    if(s->size == 0) {
        debug_print("[ERROR] Attempted to pop empty stack!\n");
        return NULL;
    }

    return s->data[--s->size];
}

static inline bool s_is_empty(Stack* s) {
    return (s->size == 0);
}
