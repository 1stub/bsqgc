#pragma once

#include "common.h"

#define PTR_MASK_NOP = ((char)0)
#define PTR_MASK_PTR = ((char)1)
#define PTR_MASK_TAG = ((char)2)

#define LEAF_PTR_MASK = NULL

struct TypeInfoBase {
    uint32_t type_id;
    uint32_t type_size;
    uint32_t slot_size;
    const char* ptr_mask; //NULL is for leaf values or structs

    const char* typekey;
};

extern struct TypeInfoBase Empty;
extern struct TypeInfoBase ListNode;

