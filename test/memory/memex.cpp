#include "../../src/runtime/memory/gc.h"

TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = NULL,  
    .typekey = "Empty"
};

TypeInfoBase ListNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "10",  
    .typekey = "ListNode"
};

GCAllocator alloc1(8, REAL_ENTRY_SIZE(8), collect);
GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);

void* makeList(size_t length, int64_t val) {
    void* ll = alloc1.allocate(&Empty);
    *((int64_t*)ll) = val;

    for(size_t i = 0; i < length; i++) {
        void* n = alloc2.allocate(&ListNode);
        *((void**)ll) = n;
        *((int64_t*)ll + 1) = val + i;
    }

    return ll;
}

int main(int argc, char** argv) {
    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[2] = {&alloc1, &alloc2};
    gtl_info.initializeGC<2>(allocs);

    void* l1 = makeList(2, 5); //stays live
    makeList(1, 0); //dies

    collect();

    assert(*((int64_t*)l1 + 1) == 7);

    return 0;
}
