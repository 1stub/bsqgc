#include "xalloc.h"

typedef struct {
    struct XAllocPage* next;
} XAllocPage;

typedef struct {
    XAllocPage* freelist;
#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
    void* next_page_addr;
#endif
} XAllocPageManager;

thread_local XAllocPageManager tl_xalloc_page_manager;

void xallocInitializePageManager(size_t tcount)
{
    tl_xalloc_page_manager.freelist = NULL;

#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
    tl_xalloc_page_manager.next_page_addr = (void*)((char*)XALLOC_BASE_ADDRESS + (XALLOC_ADDRESS_SPAN * tcount));
#endif
}

void* xallocAllocatePage()
{
    if(tl_xalloc_page_manager.freelist == NULL)
    {
#ifndef ALLOC_DEBUG_MEM_DETERMINISTIC
        tl_xalloc_page_manager.freelist = mmap(NULL, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
#else
        tl_xalloc_page_manager.freelist = mmap(tl_xalloc_page_manager.next_page_addr, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0);
        tl_xalloc_page_manager.next_page_addr = (void*)((char*)tl_xalloc_page_manager.next_page_addr + BSQ_BLOCK_ALLOCATION_SIZE);
#endif
        assert(tl_xalloc_page_manager.freelist != MAP_FAILED);
    }

    XAllocPage* xpage = tl_xalloc_page_manager.freelist;
    tl_xalloc_page_manager.freelist = xpage->next;

#ifdef ALLOC_DEBUG_MEM_INITIALIZE
        memset(xpage, 0, BSQ_BLOCK_ALLOCATION_SIZE);
#endif

    return (void*)xpage;
}

void xallocFreePage(void* page)
{
    XAllocPage* xpage = (XAllocPage*)page;

#ifdef ALLOC_DEBUG_MEM_INITIALIZE
    memset(xpage, 0, BSQ_BLOCK_ALLOCATION_SIZE);
#endif

    xpage->next = tl_xalloc_page_manager.freelist;
    tl_xalloc_page_manager.freelist = xpage;
}