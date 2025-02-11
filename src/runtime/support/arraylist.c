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

    /* Case when no pages have been linked */
    xseg->prev = NULL;
    if(al->head_segment == NULL && al->tail_segment == NULL) {
        xseg->next = NULL;
        al->head_segment = xseg;
        al->tail_segment = xseg;

        al->tail = xseg->data;
        al->head = xseg->data;
    }
    else {
        xseg->next = al->head_segment;
        if(al->head_segment != NULL) {
            al->head_segment->prev = xseg;
        }

        al->head_segment = xseg;
        al->head = (void*)((char*)xseg->data + BSQ_BLOCK_ALLOCATION_SIZE - sizeof(struct ArrayListSegment) - sizeof(void*)); // max itself cannot be inserted to, need slot just before
    }

    *(al->head) = obj;
}

void arraylist_push_tail_slow(struct ArrayList* al, void* obj)
{ 
    struct ArrayListSegment* xseg = XALLOC_PAGE(struct ArrayListSegment);
    debug_print("NEW PAGE!!!!!!\n");
    xseg->data = (void*)((char*)xseg + sizeof(struct ArrayListSegment));

    /* Case when no pages have been linked */
    xseg->next = NULL;
    if(al->head_segment == NULL && al->tail_segment == NULL) {
        xseg->prev = NULL;
        al->head_segment = xseg;
        al->tail_segment = xseg;

        al->tail = xseg->data;
        al->head = xseg->data;
    }
    else {
        xseg->prev = al->tail_segment;
        if(al->tail_segment != NULL) {
            al->tail_segment->next = xseg;
        }
        al->tail_segment = xseg;

        al->tail = xseg->data;
    }

    *(al->tail) = obj;
}

void* arraylist_pop_head_slow(struct ArrayList* al)
{ 
    void* res = *(al->head);

    if(al->head_segment->next == NULL && al->head_segment->prev == NULL) {
        al->tail_segment = NULL;
        al->head_segment = NULL;
        al->head = NULL;
        al->tail = NULL;
    }
    else 
    {
        struct ArrayListSegment* xseg = al->head_segment;
        al->head_segment = al->head_segment->next;
        al->head = GET_MIN(al->head_segment);
        XALLOC_FREE_PAGE(xseg);
        debug_print("FREE PAGE!!!!\n");
    }

    return res;
}

void* arraylist_pop_tail_slow(struct ArrayList* al)
{ 
    void* res = *(al->tail);

    /* The tail segment was the only segment so just reset everything */
    if(al->tail_segment->prev == NULL && al->tail_segment->next == NULL)
    {
        al->tail_segment = NULL;
        al->head_segment = NULL;
        al->head = NULL;
        al->tail = NULL;
    }
    else
    {
        struct ArrayListSegment* old_tail_segment = al->tail_segment;
        al->tail_segment = al->tail_segment->prev;
        al->tail = (void*)(GET_MAX(al->tail_segment));
        XALLOC_FREE_PAGE(old_tail_segment);
        debug_print("FREE PAGE!!!!\n");
    }

    return res;
}
    