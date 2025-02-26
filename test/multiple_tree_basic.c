#include "../src/runtime/memory/gc.h"

struct TypeInfoBase Empty = {
    .type_id = 0,
    .type_size = 8,
    .slot_size = 1,
    .ptr_mask = NULL,
    .typekey = "Empty"};

struct TypeInfoBase TreeNode = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "11",
    .typekey = "TreeNode"};

/**
 * Same as tree_basic.c however we now make NUM_TREES trees of same design
 **/
#define NUM_TREES 4

/* Right now we just make 4 manually lol ^^*/

/*
set watch point for each &root in stack, then checkin thread info,
*/

int main(int argc, char **argv)
{
    initializeStartup();

    register void* rbp asm("rbp");
    initializeThreadLocalInfo(rbp);

    AllocatorBin *bin16 = getBinForSize(16);
    AllocatorBin *bin8 = getBinForSize(8);

    void **root = (void **)allocate(bin16, &TreeNode);
    root[0] = allocate(bin8, &Empty);
    root[1] = allocate(bin8, &Empty);

    void **root1 = (void **)allocate(bin16, &TreeNode);
    root1[0] = allocate(bin8, &Empty);
    root1[1] = allocate(bin8, &Empty);

    void **root2 = (void **)allocate(bin16, &TreeNode);
    root2[0] = allocate(bin8, &Empty);
    root2[1] = allocate(bin8, &Empty);

    void **root3 = (void **)allocate(bin16, &TreeNode);
    root3[0] = allocate(bin8, &Empty);
    root3[1] = allocate(bin8, &Empty);

    /* Start off by loading roots to decrease amount of junk on the stack */
    loadNativeRootSet();
    collect();

    debug_print("%p %p %p\n", root, root[0], root[1]);
    debug_print("%p %p %p\n", root1, root1[0], root1[1]);
    debug_print("%p %p %p\n", root2, root2[0], root2[1]);
    debug_print("%p %p %p\n", root3, root3[0], root3[1]);

    assert(bin16->alloc_page == NULL); // Returned to PageMangager
    assert(bin8->evac_page->freecount == bin8->evac_page->entrycount - 8);

    assert(bin16->evac_page == NULL);
    assert(bin8->alloc_page == NULL);

    assert(GC_IS_YOUNG(root[0]) == false);
    assert(GC_IS_YOUNG(root[1]) == false);

    assert(GC_IS_YOUNG(root1[0]) == false);
    assert(GC_IS_YOUNG(root1[1]) == false);

    assert(GC_IS_YOUNG(root2[0]) == false);
    assert(GC_IS_YOUNG(root2[1]) == false);

    assert(GC_IS_YOUNG(root3[0]) == false);
    assert(GC_IS_YOUNG(root3[1]) == false);

    assert(GC_REF_COUNT(root[0]) == 1);
    assert(GC_REF_COUNT(root[1]) == 1);

    assert(GC_REF_COUNT(root1[0]) == 1);
    assert(GC_REF_COUNT(root1[1]) == 1);

    assert(GC_REF_COUNT(root2[0]) == 1);
    assert(GC_REF_COUNT(root2[1]) == 1);

    assert(GC_REF_COUNT(root3[0]) == 1);
    assert(GC_REF_COUNT(root3[1]) == 1);

    return 0;
}