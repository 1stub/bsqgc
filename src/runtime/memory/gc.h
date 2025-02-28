#pragma once
/*
#include "allocator.h"
#include "../support/threadinfo.h"

// extern struct ArrayList prev_roots_set;

void collect();

void evacuate(); 
void mark_and_evacuate();
void walk_stack(struct WorkList* worklist);

static inline void increment_ref_count(void* obj) 
{
    GC_REF_COUNT(obj)++;
}

static inline void decrement_ref_count(void* obj) 
{   
    if(GC_REF_COUNT(obj) > 0) {
        GC_REF_COUNT(obj)--;
    }

    // might make more sense to put back on freelist here if refcnt 0
}
*/