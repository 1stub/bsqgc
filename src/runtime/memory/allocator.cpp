#include "allocator.h"

PageInfo* PageInfo::initialize(void* block, uint16_t allocsize) noexcept
{
    PageInfo* pp = (PageInfo*)block;

    pp->freelist = nullptr;
    pp->next = nullptr;

    pp->data = ((uint8_t*)block + sizeof(PageInfo));
    pp->allocsize = allocsize;
    pp->realsize = REAL_ENTRY_SIZE(allocsize);
    pp->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - (pp->data - (uint8_t*)pp)) / pp->realsize;
    pp->freecount = pp->entrycount;
    pp->pagestate = PageStateInfo_GroundState;

    FreeListEntry* current = pp->freelist;

    for(int i = 0; i < pp->entrycount - 1; i++) {
        current->next = (FreeListEntry*)((char*)current + pp->realsize);
        current = current->next;
    }
    current->next = nullptr;
}

GlobalPageGCManager GlobalPageGCManager::g_gc_page_manager;

PageInfo* GlobalPageGCManager::allocateFreshPage(uint16_t entrysize) noexcept
{
    GC_MEM_LOCK_ACQUIRE();

    PageInfo* pp = nullptr;
    if(this->empty_pages != nullptr) {
        void* page = this->empty_pages;
        this->empty_pages = this->empty_pages->next;

        pp = PageInfo::initialize(page, entrysize);
    }
    else {
#ifndef ALLOC_DEBUG_MEM_DETERMINISTIC
        void* page = mmap(NULL, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
#else
        ALLOC_LOCK_ACQUIRE();

        void* page = (XAllocPage*)mmap(GlobalThreadAllocInfo::s_current_page_address, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, 0, 0);
        GlobalThreadAllocInfo::s_current_page_address = (void*)((uint8_t*)GlobalThreadAllocInfo::s_current_page_address + BSQ_BLOCK_ALLOCATION_SIZE);

        ALLOC_LOCK_RELEASE();    
#endif

        assert(page != MAP_FAILED);
        this->pagetable.pagetable_insert(page);

        pp = PageInfo::initialize(page, entrysize);
    }

    GC_MEM_LOCK_RELEASE();
    return pp;
}

void getFreshPageForAllocator(AllocatorBin* alloc)
{   
    PageInfo* page = getPageFromManager(alloc->page_manager, alloc->entrysize);
    // Fetch a page from our manager and insert into alloc_page list
    INSERT_PAGE_IN_LIST(alloc->alloc_page, page);

    alloc->freelist = alloc->alloc_page->freelist;
}

void getFreshPageForEvacuation(AllocatorBin* alloc) 
{
    PageInfo* page = getPageFromManager(alloc->page_manager, alloc->entrysize);

    INSERT_PAGE_IN_LIST(alloc->evac_page, page);

    //Need to update freelist?
}

/*
//Following 3 methods verify integrity of canaries
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
*/
