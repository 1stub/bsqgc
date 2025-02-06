#include "stack.h"

thread_local struct Stack marking_stack = {NULL, NULL, NULL, NULL};

void stack_push_slow(struct Stack* s, void* obj)
{
    struct StackSegment* xseg = XALLOC_PAGE(struct StackSegment);
    xseg->data = (void*)((char*)xseg + sizeof(struct StackSegment));
    xseg->next = s->current;

    s->current = xseg;
    
    s->min = xseg->data;
    s->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - (sizeof(struct StackSegment) + sizeof(void*)));
    s->top = s->min;

    *(s->top) = obj;
}

void* stack_pop_slow(struct Stack* s)
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
        struct StackSegment* xseg = s->current;
        s->current = xseg->next;

        s->min = s->current->data;
        s->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - (sizeof(struct StackSegment) + sizeof(void*)));
        s->top = s->max;

        XALLOC_FREE_PAGE(xseg);
    }

    return res;
}