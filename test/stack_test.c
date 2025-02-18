#include "stack_test.h"

int test_stack_walk() {
    AllocatorBin* bin16 = getBinForSize(16);
    AllocatorBin* bin8 = getBinForSize(8);

    char try_to_make_weird_stack_pointer_offsets = 'a';
    debug_print("%i\n", try_to_make_weird_stack_pointer_offsets);

    /* Create the end node (not part of the linked list, but pointed to by the last node) */
    void* end = allocate(bin8, &Empty);
    *((int64_t*)end) = 0xDEADBEF4; // Store some data in the end node

    //void* tobe_collected = allocate(bin8, &Empty);
    //*((int64_t*)tobe_collected) = 0xBADCABF0; 

    /* Create the linked list nodes */
    void* ln0 = allocate(bin16, &ListNode);
    void* ln1 = allocate(bin16, &ListNode);
    void* ln2 = allocate(bin16, &ListNode);
    void* ln3 = allocate(bin16, &ListNode);

    /* Store data in the first slot of each ListNode */
    *((int64_t*)ln0) = 0xDEADBEF0;
    *((int64_t*)ln1) = 0xDEADBEF1;
    *((int64_t*)ln2) = 0xDEADBEF2;
    *((int64_t*)ln3) = 0xDEADBEF3;

    /* Link the nodes together */
    void** second_slot0 = (void**)((int64_t*)ln0 + 1);
    *second_slot0 = ln1; // ln0 points to ln1

    void** second_slot1 = (void**)((int64_t*)ln1 + 1);
    *second_slot1 = ln2; // ln1 points to ln2

    void** second_slot2 = (void**)((int64_t*)ln2 + 1);
    *second_slot2 = ln3; // ln2 points to ln3

    void** second_slot3 = (void**)((int64_t*)ln3 + 1);
    *second_slot3 = end; // ln3 points to end

    /* Debug prints to verify the structure */
    debug_print("Root node (ln0) at %p:\n", ln0);
    debug_print("  First slot: 0x%lx\n", *((int64_t*)ln0));
    debug_print("  Second slot: %p (points to ln1)\n", *second_slot0);

    debug_print("Node ln1 at %p:\n", ln1);
    debug_print("  First slot: 0x%lx\n", *((int64_t*)ln1));
    debug_print("  Second slot: %p (points to ln2)\n", *second_slot1);

    debug_print("Node ln2 at %p:\n", ln2);
    debug_print("  First slot: 0x%lx\n", *((int64_t*)ln2));
    debug_print("  Second slot: %p (points to ln3)\n", *second_slot2);

    debug_print("Node ln3 at %p:\n", ln3);
    debug_print("  First slot: 0x%lx\n", *((int64_t*)ln3));
    debug_print("  Second slot: %p (points to end)\n", *second_slot3);

    debug_print("End node at %p:\n", end);
    debug_print("  First slot: 0x%lx\n", *((int64_t*)end));

    return 0;
}