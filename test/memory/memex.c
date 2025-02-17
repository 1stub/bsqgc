#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    test_stack_walk();

    /* Not calling here, appears the compiler is removing some variables that SHOULD be on the stack */
    walk_stack();

    return 0;
}
