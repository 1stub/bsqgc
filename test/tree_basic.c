#include "../src/language/bsqtype.h"
#include "../src/runtime/memory/allocator.h"
#include "../src/runtime/support/threadinfo.h"

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
    .ptr_mask = "01",  
    .typekey = "ListNode"
};

struct TypeInfoBase TreeNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "11",  
    .typekey = "ListNode"
};

/* Lets create a simple tree and collect */
int main(int argc, char** argv) {
    /* Setup necessary bin and thread related stuff */
    initializeStartup();
    initializeThreadLocalInfo();

    AllocatorBin* bin16 = getBinForSize(16);
    AllocatorBin* bin8 = getBinForSize(8);

    void** root = (void**)allocate(bin16, &TreeNode);
    void* leaf1 = allocate(bin8, &Empty);
    void* leaf2 = allocate(bin8, &Empty);

    root[0] = leaf1;
    root[1] = leaf2;

    // Need to do some asserts now

    return 0;
}