#include "bsqtype.h"

struct TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = "0",  
    .typekey = "Empty"
};

struct TypeInfoBase ListNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "01",  // First slot is not a pointer, second is a pointer
    .typekey = "ListNode"
};