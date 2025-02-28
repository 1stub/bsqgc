#include "allocator.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CANARY_DEBUG_CHECK

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin8 = {.freelist = NULL, .entrysize = 8, .roots_count = 0, .old_roots_count = 0, .alloc_page = NULL, .evac_page = NULL, .page_manager = &p_mgr8};
AllocatorBin a_bin16 = {.freelist = NULL, .entrysize = 16, .roots_count = 0, .old_roots_count = 0, .alloc_page = NULL, .evac_page = NULL, .page_manager = &p_mgr16};

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

    /* Perhaps macro some of this repition? */
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
    PageInfo* page = getPageFromManager(alloc->page_manager, alloc->entrysize);
    /* Fetch a page from our manager and insert into alloc_page list */
    INSERT_PAGE_IN_LIST(alloc->alloc_page, page);

    alloc->freelist = alloc->alloc_page->freelist;
}

void getFreshPageForEvacuation(AllocatorBin* alloc) 
{
    PageInfo* page = getPageFromManager(alloc->page_manager, alloc->entrysize);

    INSERT_PAGE_IN_LIST(alloc->evac_page, page);

    /* Need to update freelist? */
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

/* Following 3 methods verify integrity of canaries */
bool verifyCanariesInBlock(char* block, uint16_t entry_size)
{
    uint64_t* pre_canary = (uint64_t*)(block);
    uint64_t* post_canary = (uint64_t*)(block + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);

    debug_print("[CANARY_CHECK] Verifying canaries for block at %p\n", block);
    debug_print("\tPre-canary value: %lx\n", *pre_canary);
    debug_print("\tPost-canary value: %lx\n", *post_canary);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        debug_print("[ERROR] Canary corruption detected at block %p\n", block);
        return false;
    }
    return true;
}

void verifyCanariesInPage(PageInfo* page)
{
    FreeListEntry* list = page->freelist;
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    debug_print("[CANARY_CHECK] Verifying canaries for page at %p\n", page);

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = PAGE_MASK_EXTRACT_DATA(list) + (i * REAL_ENTRY_SIZE(page->entrysize));
        debug_print("\tChecking block %d at address %p\n", i, block_address);
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tBlock %d metadata state: isalloc=%d\n", i, metadata->isalloc);

        if (metadata->isalloc) {
            debug_print("\tAllocated block detected, verifying canaries...\n");
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }
    debug_print("\n");

    debug_print("[CANARY_CHECK] Verifying freelist for page at %p\n", page);
    while(list){
        debug_print("\tChecking freelist block at %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tFreelist block metadata state: isalloc=%d\n", metadata->isalloc);

        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated at %p\n", list);
            assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    // Make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);

    debug_print("\n");
}

void verifyAllCanaries()
{
    for(int i = 0; i < NUM_BINS; i++) {
        AllocatorBin* bin = getBinForSize(8 * (1 << i));
        PageInfo* alloc_page = bin->alloc_page;
        PageInfo* evac_page = bin->evac_page;

        debug_print("[CANARY_CHECK] Verifying all pages in bin\n");

        while (alloc_page) {
            debug_print("[CANARY_CHECK] Verifying canaries in alloc page at %p\n", alloc_page);
            verifyCanariesInPage(alloc_page);
            alloc_page = alloc_page->next;
        }

        while (evac_page) {
            debug_print("[CANARY_CHECK] Verifying canaries in evac page at %p\n", evac_page);
            verifyCanariesInPage(evac_page);
            evac_page = evac_page->next;
        }
    }
}

