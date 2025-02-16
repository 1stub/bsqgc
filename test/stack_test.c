#include "stack_test.h"

int test_stack_walk() {
    /* We assign these global pointers to hold addreses of stack variables, keeping them alive */
    AllocatorBin* bin = initializeAllocatorBin(sizeof(int*));

    int try_to_make_weird_stack_pointer_offsets = 12;
    debug_print("%i\n", try_to_make_weird_stack_pointer_offsets);

    /* Allocate 10 local variables using the allocator */
    int* local_0 = allocate(bin, NULL);
    int* local_1 = allocate(bin, NULL);
    int* local_2 = allocate(bin, NULL);
    int* local_3 = allocate(bin, NULL);
    int* local_4 = allocate(bin, NULL);
    int* local_5 = allocate(bin, NULL);
    int* local_6 = allocate(bin, NULL);
    int* local_7 = allocate(bin, NULL);
    int* local_8 = allocate(bin, NULL);
    int* local_9 = allocate(bin, NULL);

    /* Initialize the local variables with unique values */
    *local_0 = 0xDEADBEF0;
    *local_1 = 0xDEADBEF1;
    *local_2 = 0xDEADBEF2;
    *local_3 = 0xDEADBEF3;
    *local_4 = 0xDEADBEF4;
    *local_5 = 0xDEADBEF5;
    *local_6 = 0xDEADBEF6;
    *local_7 = 0xDEADBEF7;
    *local_8 = 0xDEADBEF8;
    *local_9 = 0xDEADBEF9;

    /* Print the addresses of the allocated objects */
    printf("allocated object at %p\n", local_0);
    printf("allocated object at %p\n", local_1);
    printf("allocated object at %p\n", local_2);
    printf("allocated object at %p\n", local_3);
    printf("allocated object at %p\n", local_4);
    printf("allocated object at %p\n", local_5);
    printf("allocated object at %p\n", local_6);
    printf("allocated object at %p\n", local_7);
    printf("allocated object at %p\n", local_8);
    printf("allocated object at %p\n", local_9);

    walk_stack();

    return 0;
}