#include "stack.h"

thread_local Stack marking_stack = {NULL, NULL, NULL};

void stack_push_slow(Stack* s, void* obj)
{
    StackSegment* xseg = XALLOC_PAGE(StackSegment);
    xseg->data = (void*)((char*)xseg + sizeof(StackSegment));
    xseg->next = s->current;

    s->current = xseg;
    
    s->min = xseg->data;
    s->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - (sizeof(StackSegment) + sizeof(void*)));
    s->top = s->min;

    *(s->top) = obj;
}

void* stack_pop_slow(Stack* s)
{
    void* res = *(s->top);

    if(s->current->next == NULL)
    {
        s->current = NULL;

        s->min = NULL;
        s->max = NULL;
        s->top = NULL;
    }
    else
    {
        StackSegment* xseg = s->current;
        s->current = xseg->next;

        s->min = s->current->data;
        s->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - (sizeof(StackSegment) + sizeof(void*)));
        s->top = s->max;

        XALLOC_FREE_PAGE(xseg);
    }

    return res;
}