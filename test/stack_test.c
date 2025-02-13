#include "stack_test.h"

int* global_ptr_0 = NULL;
int* global_ptr_1 = NULL;
int* global_ptr_2 = NULL;
int* global_ptr_3 = NULL;
int* global_ptr_4 = NULL;
int* global_ptr_5 = NULL;

void print_global_addresses() {
    printf("global_ptr_0 %p\n", global_ptr_0);
    printf("global_ptr_1 %p\n", global_ptr_1);
    printf("global_ptr_2 %p\n", global_ptr_2);
    printf("global_ptr_3 %p\n", global_ptr_3);
    printf("global_ptr_4 %p\n", global_ptr_4);
    printf("global_ptr_5 %p\n\n", global_ptr_5);

}

int test_stack_walk() {
    /* We assign these global pointers to hold addreses of stack variables, keeping them alive */
    int* local_0 = malloc(sizeof(int));
    int* local_1 = malloc(sizeof(int));
    int* local_2 = malloc(sizeof(int));

    *local_0 = 0xDEADBEF0;
    *local_1 = 0xDEADBEF1;
    *local_2 = 0xDEADBEF2;

    int* local_3 = malloc(sizeof(int));
    int* local_4 = malloc(sizeof(int));
    int* local_5 = malloc(sizeof(int));

    *local_3 = 0xDEADBEF3;
    *local_4 = 0xDEADBEF4;
    *local_5 = 0xDEADBEF5;

    global_ptr_3 = local_3;
    global_ptr_4 = local_4;
    global_ptr_5 = local_5;
    

    global_ptr_0 = local_0;
    global_ptr_1 = local_1;
    global_ptr_2 = local_2;

    //print_global_addresses();

    walk_stack();

    return 0;
}

int do_stuff() {


    return 1;
}