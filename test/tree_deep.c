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

int main(int argc, char** argv) {
    initializeStartup();

    register void* rbp asm("rbp");
    initializeThreadLocalInfo(rbp);

    AllocatorBin* bin16 = getBinForSize(16);
    AllocatorBin* bin8 = getBinForSize(8);

    void** root = (void**)allocate(bin16, &TreeNode);

    root[0] = (void**)allocate(bin16, &TreeNode);
    root[1] = (void**)allocate(bin16, &TreeNode);

    ((void**)root[0])[0] = allocate(bin8, &Empty);
    ((void**)root[0])[1] = allocate(bin8, &Empty);

    ((void**)root[1])[0] = allocate(bin8, &Empty);
    ((void**)root[1])[1] = allocate(bin8, &Empty);

    debug_print("root[0] left ptr %p\n", ((void**)root[0])[0]);
    debug_print("root[0] right ptr %p\n", ((void**)root[0])[1]);

    debug_print("root[1] left ptr %p\n", ((void**)root[1])[0]);
    debug_print("root[1] right ptr %p\n", ((void**)root[1])[1]);

    loadNativeRootSet();
    collect();

    debug_print("root[0] left ptr %p\n", ((void**)root[0])[0]);
    debug_print("root[0] right ptr %p\n", ((void**)root[0])[1]);

    debug_print("root[1] left ptr %p\n", ((void**)root[1])[0]);
    debug_print("root[1] right ptr %p\n", ((void**)root[1])[1]);

    //assert(bin16->evac_page->freecount == bin16->evac_page->entrycount - 2);
    //assert(bin8->evac_page->freecount == bin8->evac_page->entrycount - 4);

    root = NULL;

    loadNativeRootSet();
    collect();

    //assert(bin16->evac_page->freecount == bin16->evac_page->entrycount);
    //assert(bin8->evac_page->freecount == bin8->evac_page->entrycount);

    return 0;
}