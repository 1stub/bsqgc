#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    test_stack_walk();

    collect();
    
    return 0;
}
