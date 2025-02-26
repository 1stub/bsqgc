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
* Lets delete a root and see if the tree dies too!
**/

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

    root[0] = allocate(bin8, &Empty);
    root[1] = allocate(bin8, &Empty);

    debug_print("%p %p %p\n", root, root[0], root[1]);

    loadNativeRootSet();
    collect();

    root = NULL;

    loadNativeRootSet();
    collect();

    /* Our root has become null, so root[0], root[1], and the root itself should be freed now */
    assert(bin8->evac_page->freecount == bin8->evac_page->entrycount);
    assert(bin16->alloc_page == NULL); //returned to page manager

    return 0;
}