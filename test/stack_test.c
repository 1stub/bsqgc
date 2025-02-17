#include "stack_test.h"

int test_stack_walk() {
    /* We assign these global pointers to hold addreses of stack variables, keeping them alive */
    AllocatorBin* bin16 = initializeAllocatorBin(ListNode.type_size);
    AllocatorBin* bin8 = initializeAllocatorBin(Empty.type_size);

    char try_to_make_weird_stack_pointer_offsets = 'a';
    debug_print("%i\n", try_to_make_weird_stack_pointer_offsets);

    void* e = allocate(bin8, &Empty);
    void* ln = allocate(bin16, &ListNode);

    /* First slot of ln stores some data */
    *((int64_t*)ln) = 0xDEADBEF0;

    /* data ln points to stores some stuff */
    *((int64_t*)e) = 0xDEADBEF1;

    /* Second slot of ln stores a pointer to e */
    void** second_slot = (void**)((int64_t*)ln + 1);
    *second_slot = e;

    debug_print("First slot of ln: 0x%lx\n", *((int64_t*)ln));
    debug_print("Second slot of ln: %p (should be equal to e: %p)\n", *second_slot, e);

    return 0;
}