#include "../src/runtime/memory/gc.h"

struct TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = NULL,  
    .typekey = "Empty"
};

struct TypeInfoBase TreeNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "11",  
    .typekey = "TreeNode"
};


/**
* If we do not initialize startup and thread stuff from main THEN do our allocations
* ceratin objects do not get found on the stack. 
**/
int main(int argc, char** argv) {
    initializeStartup();

    register void* rbp asm("rbp");
    initializeThreadLocalInfo(rbp);

    AllocatorBin* bin16 = getBinForSize(16);
    AllocatorBin* bin8 = getBinForSize(8);

    void** root = (void**)allocate(bin16, &TreeNode);

    root[0] = allocate(bin16, &TreeNode);
    root[1] = allocate(bin16, &TreeNode);

    ((void**)root[0])[0] = allocate(bin8, &Empty);
    ((void**)root[0])[1] = allocate(bin8, &Empty);

    ((void**)root[1])[0] = allocate(bin8, &Empty);
    ((void**)root[1])[1] = allocate(bin8, &Empty);

    loadNativeRootSet();
    collect();

    assert(bin16->evac_page->freecount == bin16->evac_page->entrycount - 2);
    assert(bin8->evac_page->freecount == bin8->evac_page->entrycount - 4);

    /* Some issues currently with freeing trees of depth */
    #if 0
    root = NULL;

    loadNativeRootSet();
    collect();
    #endif

    return 0;
}