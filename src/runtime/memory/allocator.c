#include "allocator.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CANARY_DEBUG_CHECK

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin8 = {.freelist = NULL, .entrysize = 8, .alloc_page = NULL, .evac_page = NULL, .page_manager = &p_mgr8};
AllocatorBin a_bin16 = {.freelist = NULL, .entrysize = 16, .alloc_page = NULL, .evac_page = NULL, .page_manager = &p_mgr16};

/* Each AllocatorBin needs its own page manager */
PageManager p_mgr8 = {.low_utilization_pages = NULL, .mid_utilization_pages = NULL, .high_utilization_pages = NULL, .filled_pages = NULL, .empty_pages = NULL};
PageManager p_mgr16 = {.low_utilization_pages = NULL, .mid_utilization_pages = NULL, .high_utilization_pages = NULL, .filled_pages = NULL, .empty_pages = NULL};

static void setup_freelist(PageInfo* pinfo, uint16_t entrysize) {
    FreeListEntry* current = pinfo->freelist;

    for(int i = 0; i < pinfo->entrycount - 1; i++) {
        current->next = (FreeListEntry*)((char*)current + REAL_ENTRY_SIZE(entrysize));
        current = current->next;
    }
    current->next = NULL;
}

static PageInfo* initializePage(void* page, uint16_t entrysize)
{
    debug_print("New page!\n");

    PageInfo* pinfo = (PageInfo*)page;
    pinfo->freelist = (FreeListEntry*)((char*)page + sizeof(PageInfo));
    pinfo->entrysize = entrysize;
    pinfo->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - sizeof(PageInfo)) / REAL_ENTRY_SIZE(entrysize);
    pinfo->freecount = pinfo->entrycount;
    pinfo->pagestate = PageStateInfo_GroundState;

    setup_freelist(pinfo, pinfo->entrysize);

    return pinfo;
}

/**
* Go into a bins page manager and grab us a page. This will need some rework in the future
* to where we can decide more specifically what page utilizaiton levels we want for a specific
* purpose. Ex if we want an evac page we may be more inclined to grab a full page. Or an alloc
* page could be more useful to be mostly empty since most objects on it will die.
**/
PageInfo* getPageFromManager(PageManager* pm, uint16_t entrysize) 
{
    PageInfo* page = NULL;

    if(pm->empty_pages != NULL) {
        page = pm->empty_pages;
        pm->empty_pages = pm->empty_pages->next;
    }
    else if(pm->low_utilization_pages != NULL) {
        page = pm->low_utilization_pages;
        pm->low_utilization_pages = pm->low_utilization_pages->next;
    }
    else if(pm->mid_utilization_pages != NULL) {
        page = pm->mid_utilization_pages;
        pm->mid_utilization_pages = pm->mid_utilization_pages->next;
    } 
    else if(pm->high_utilization_pages != NULL) {
        page = pm->high_utilization_pages;
        pm->high_utilization_pages = pm->high_utilization_pages->next;
    }
    else {
        page = allocateFreshPage(entrysize);
    }

    return page;
}

/**
* Insert into our mulitlevel page manager. We do not need to insert into page manager here
* because we are assuming this page will directly be needed for allocation. No point in 
* inserting into empty_pages just to have to go and grab it right away. Saves pointer overhead.
**/
PageInfo* allocateFreshPage(uint16_t entrysize)
{
#ifdef ALLOC_DEBUG_MEM_DETERMINISTIC
    static void* old_base = GC_ALLOC_BASE_ADDRESS;
    void* page = mmap((void*)old_base, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    old_base += BSQ_BLOCK_ALLOCATION_SIZE;
#else
    void* page = mmap(NULL, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
#endif

    assert(page != MAP_FAILED);

    if(!pagetable_root) {
        pagetable_init();
    }
    pagetable_insert(page);

    return initializePage(page, entrysize);
}

void getFreshPageForAllocator(AllocatorBin* alloc)
{   
    /* Fetch a page from our manager and insert into alloc_page list */
    if(alloc->alloc_page != NULL) {
        alloc->alloc_page->next = getPageFromManager(alloc->page_manager, alloc->entrysize);
        alloc->alloc_page = alloc->alloc_page->next;
        alloc->alloc_page->next = NULL;
    } 
    else {
        alloc->alloc_page = getPageFromManager(alloc->page_manager, alloc->entrysize);
        alloc->alloc_page->next = NULL;
    }

    alloc->freelist = alloc->alloc_page->freelist;
}

void getFreshPageForEvacuation(AllocatorBin* alloc) 
{
    if(alloc->evac_page != NULL) {
        alloc->evac_page->next = getPageFromManager(alloc->page_manager, alloc->entrysize);
        alloc->evac_page = alloc->evac_page->next;
        alloc->evac_page->next = NULL;
    }
    else {
        alloc->evac_page = getPageFromManager(alloc->page_manager, alloc->entrysize);
        alloc->evac_page->next = NULL;
    }
}

AllocatorBin* getBinForSize(uint16_t entrytsize)
{
    switch(entrytsize){
        case 8: {
            return &a_bin8;
        }
        case 16: {
            return &a_bin16;
        }
        default: return NULL;
    }

    return NULL;
}
