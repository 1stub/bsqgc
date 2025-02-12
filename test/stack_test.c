#include "stack_test.h"

int global_var = 0xDEADBAD1;
int* globa_ptr_1 = NULL;

static int* local_ptr = NULL;
static int* global_ptr = NULL;
static int* global_ptr_1 = NULL;
static int yes = 0xDEADBAD6;

int test_stack_walk() {
    local_ptr = malloc(sizeof(int));
    *local_ptr = 0xDEADBAD2;

    global_ptr = malloc(sizeof(int));
    *global_ptr = 0xDEADBAD3;

    global_ptr_1 = malloc(sizeof(int));
    *global_ptr_1 = 0xDEADBAD4;

    globa_ptr_1 = &yes;

    printf("Address of global_var: %p\n", (void *)&global_var);
    printf("Address of global_ptr: %p\n", (void *)global_ptr);
    printf("Address of local_ptr: %p\n", (void *)local_ptr);
    printf("Address of global_ptr_1: %p\n", (void *)global_ptr_1);

    printf("Value of global_var: 0x%x\n", global_var);
    printf("Value of *local_ptr: 0x%x\n", *local_ptr);
    printf("Value of *global_ptr: 0x%x\n", *global_ptr);
    printf("Value of *global_ptr_1: 0x%x\n", *global_ptr_1);

    return 0;
}

int do_stuff() {
    int* ptr = &global_var;
    int* ptr1 = malloc(sizeof(int));
    *ptr1 = 0xDEADBAD7;
    printf("ptr addr %p\n", ptr);
    printf("ptr1 addr %p\n", ptr1);

    return 0;
}