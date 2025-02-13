#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    test_stack_walk();

    walk_stack();

    return 0;
}
