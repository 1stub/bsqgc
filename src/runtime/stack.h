#pragma once

#include "common.h"

#define MAX_STACK_SIZE 1024

typedef struct {
    Object* data[MAX_STACK_SIZE];
    size_t size;
} Stack;

static inline void stack_init(Stack* s) {
    s->size = 0;
}

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
