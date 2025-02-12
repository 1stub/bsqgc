#include "stack_test.h"

int global_var = 0xDEADBAD1;

int do_stuff() {
    int* ptr = &global_var;
    printf("other function pointer to global addr %p\n", ptr);

    return 0;
}

int test_stack_walk() {
    int local_var = 10;
    int* local_ptr = malloc(sizeof(int));
    *local_ptr = 0xDEADBAD2;

    int* global_ptr = malloc(sizeof(int));
    *global_ptr = 0xDEADBAD3;

    printf("Address of global_var: %p\n", (void *)&global_var);
    printf("Address of global_ptr: %p\n", (void *)global_ptr);
    printf("Address of local_var: %p\n", (void *)&local_var);
    printf("Address of local_ptr: %p\n", (void *)local_ptr);

    /* pretend these mallocs are still in use :) */
    return 0;
}