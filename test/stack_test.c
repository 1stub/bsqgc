#include "stack_test.h"

int global_var = 42;
int *global_ptr = NULL;

int test_stack_walk() {
    int local_var = 10;
    int *local_ptr = malloc(sizeof(int));
    global_ptr = malloc(sizeof(int));

    printf("Address of global_var: %p\n", (void *)&global_var);
    printf("Address of global_ptr: %p\n", (void *)&global_ptr);
    printf("Address of local_var: %p\n", (void *)&local_var);
    printf("Address of local_ptr: %p\n", (void *)&local_ptr);
    printf("Value of local_ptr: %p\n", (void *)local_ptr);
    printf("Value of global_ptr: %p\n", (void *)global_ptr);

    /* pretend these mallocs are still in use :) */

    return 0;
}