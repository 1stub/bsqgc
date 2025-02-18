#include "../stack_test.h" 

int main()
{
    initializeStartup();
    initializeThreadLocalInfo();

    initializeAllocatorBin(ListNode.type_size);
    initializeAllocatorBin(Empty.type_size);

    test_stack_walk();
    collect();
    
    test_stack_walk();
    collect();

    return 0;
}
