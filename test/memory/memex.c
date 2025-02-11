#include <stdio.h>
#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    printf("hello world\n");

    test_stack_walk();

    walk_stack();

    return 0;
}
