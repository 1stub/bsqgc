#include "arraylist.h"
#include "xalloc.h"

/* Really this is just important so our head and tail are both initialized before adding elements */
void arraylist_initialize(struct ArrayList* al)
{
    al->head = NULL;
    al->tail = NULL;
    al->head_segment = NULL;
    al->tail_segment = NULL;
}

void arraylist_push_head_slow(struct ArrayList* al, void* obj) 
{
    /* A tad "weirder" than adding a tail. We neeed to make a new page and set our head to its max element */
    struct ArrayListSegment* xseg = XALLOC_PAGE(struct ArrayListSegment);
    debug_print("NEW PAGE!!!!!!\n");
    xseg->data = (void*)((char*)xseg + sizeof(struct ArrayListSegment));

    xseg->prev = NULL;
    xseg->next = al->head_segment;
    if(al->head_segment != NULL) {
        al->head_segment->prev = xseg;
    }

    al->head_segment = xseg;

    al->head = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - sizeof(struct ArrayListSegment) - sizeof(void*)); // max itself cannot be inserted to, need slot just before

    *(al->head) = obj;
}

void arraylist_push_tail_slow(struct ArrayList* al, void* obj)
{ 
    struct ArrayListSegment* xseg = XALLOC_PAGE(struct ArrayListSegment);
    debug_print("NEW PAGE!!!!!!\n");
    xseg->data = (void*)((char*)xseg + sizeof(struct ArrayListSegment));

    xseg->prev = al->tail_segment;
    xseg->next = NULL;
    if(al->tail_segment != NULL) {
        al->tail_segment->next = xseg;
    }
    al->tail_segment = xseg;

    al->tail = xseg->data;

    *(al->tail) = obj;
}

#if 0
void* arraylist_pop_head_slow(struct ArrayList* al)
{ 
    void* res = *(al->head);

    if(al->current->next == NULL) {
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
        debug_print("FREE PAGE!!!!\n");
    }

    return res;
}

void* arraylist_pop_tail_slow(struct ArrayList* al)
{ 
    void* res = *(al->tail);

    struct ArrayListSegment* tail_seg = (struct ArrayListSegment*)((uintptr_t)res & PAGE_ADDR_MASK);

    /* The tail segment was the only segment so just reset everything */
    if(tail_seg->prev == NULL)
    {
        al->current = NULL;
        
        al->min = NULL;
        al->max = NULL;
        al->head = NULL;
        al->tail = NULL;
    }
    else
    {
        XALLOC_FREE_PAGE(tail_seg);
        debug_print("FREE PAGE!!!!\n");
    }

    return res;
}
#endif
    