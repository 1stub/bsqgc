#include "../stack_test.h" 


int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    do_stuff();
    test_stack_walk();
    walk_stack();

    return 0;
}
