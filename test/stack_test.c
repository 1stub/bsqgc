#include "stack_test.h"

int test_stack_walk() {
    /* We assign these global pointers to hold addreses of stack variables, keeping them alive */
    AllocatorBin* bin = initializeAllocatorBin(sizeof(int));

    /* I am thinking using out allocate method is better here than malloc since the objects now have metadata */
    int* local_0 = allocate(bin, NULL);
    int* local_1 = allocate(bin, NULL);
    int* local_2 = allocate(bin, NULL);
    int* local_3 = allocate(bin, NULL);
    int* local_4 = allocate(bin, NULL);
    int* local_5 = allocate(bin, NULL);

    *local_0 = 0xDEADBEF0;
    *local_1 = 0xDEADBEF1;
    *local_2 = 0xDEADBEF2;
    *local_3 = 0xDEADBEF3;
    *local_4 = 0xDEADBEF4;
    *local_5 = 0xDEADBEF5;

    return 0;
}