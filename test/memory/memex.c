#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    test_stack_walk();

    mark_and_evacuate();
    
    return 0;
}
