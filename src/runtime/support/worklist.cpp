#include "worklist.h"

void worklist_push_slow(struct WorkList* l, void* obj) 
{
    struct WorkListSegment* xseg = XALLOC_PAGE(struct WorkListSegment);
    xseg->data = (void*)((char*)xseg + sizeof(struct WorkListSegment));

    /* Case when no pages have been linked */
    xseg->next = NULL;
    if(l->head_segment == NULL && l->tail_segment == NULL) {
        l->head_segment = xseg;
        l->tail_segment = xseg;

        l->tail = xseg->data;
        l->head = xseg->data;
    }
    else {
        if(l->tail_segment != NULL) {
            l->tail_segment->next = xseg;
        }
        l->tail_segment = xseg;

        l->tail = xseg->data;
    }

    *(l->tail) = obj;

}

void* worklist_pop_slow(struct WorkList* l) {
    void* res = *(l->head);

    /* Only segment, reset everything */
    if(l->head_segment->next == NULL) {
        l->tail_segment = NULL;
        l->head_segment = NULL;
        l->head = NULL;
        l->tail = NULL;
    }
    else 
    {
        struct WorkListSegment* xseg = l->head_segment;
        l->head_segment = l->head_segment->next;
        l->head = GET_MIN_FOR_SEGMENT(l->head_segment, WL_SEG_SIZE);
        XALLOC_FREE_PAGE(xseg);
        debug_print("FREE PAGE!!!!\n");
    }

    return res;
}

bool worklist_is_empty(struct WorkList* l) {
    return l->head == NULL || l->tail == NULL;
}