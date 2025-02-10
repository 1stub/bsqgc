#include "arraylist.h"

void arraylist_push_head_slow(struct ArrayList* al, void* obj) 
{

}

void arraylist_push_tail_slow(struct ArrayList* al, void* obj)
{ 
    struct ArrayListSegment* xseg = XALLOC_PAGE(struct ArrayListSegment);
    xseg->data = (void*)((char*)xseg + sizeof(struct ArrayListSegment));
    xseg->next = al->current;

    al->current = xseg;

    al->min = xseg->data;
    al->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - sizeof(struct ArrayListSegment) + sizeof(void*));
    al->tail = al->min;

    *(al->tail) = obj;
}

void* arraylist_pop_head_slow(struct ArrayList* al)
{ 
    void* res = *(al->tail);

    return res;
}

void* arraylist_pop_tail_slow(struct ArrayList* al)
{ 
    void* res = *(al->tail);

    if(al->current->next == NULL)
    {
        al->current = NULL;
        
        al->min = NULL;
        al->max = NULL;
        al->head = NULL;
        al->tail = NULL;
    }
    else
    {
        struct ArrayListSegment* xseg = al->current;
        al->current = xseg->next;

        al->min = al->current->data;
        al->max = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - sizeof(struct ArrayListSegment) + sizeof(void*));
        al->head = al->min;
        al->tail = al->max;

        XALLOC_FREE_PAGE(xseg);
    }

    return res;
}
    