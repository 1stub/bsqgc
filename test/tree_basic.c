#include "../src/runtime/memory/gc.h"

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
    .typekey = "TreeNode"
};

int run() {
    AllocatorBin* bin16 = getBinForSize(16);
    AllocatorBin* bin8 = getBinForSize(8);

    void** root = (void**)allocate(bin16, &TreeNode);

    void* leaf1 = allocate(bin8, &Empty);
    void* leaf2 = allocate(bin8, &Empty);

    root[0] = leaf1;
    root[1] = leaf2;

    debug_print("%p %p %p\n", root, leaf1, leaf2);

    return 0;
}

/**
* If we do not initialize startup and thread stuff from main THEN do our allocations
* ceratin objects do not get found on the stack. 
**/
int main(int argc, char** argv) {
    initializeStartup();
    initializeThreadLocalInfo();

    run();
    collect();

    run();
    collect();

    return 0;
}