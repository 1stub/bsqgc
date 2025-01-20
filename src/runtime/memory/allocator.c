#include "allocator.h"

#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

#define CANARY_DEBUG_CHECK

//for future impls that can have multiple different entry sizes we can create an 
//array of pre defined allocator bins for a given size
#define DEFAULT_ENTRY_SIZE sizeof(Object) 

AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .need_collection = NULL};

Object* root_stack[MAX_ROOTS];

static PageInfo* initializePage(void* page, uint16_t entrysize)
{
    debug_print("New page!\n");

    PageInfo* pinfo = (PageInfo*)page;
    pinfo->freelist = (FreeListEntry*)((char*)page + sizeof(PageInfo));
    pinfo->entrysize = entrysize;
    pinfo->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - sizeof(PageInfo)) / REAL_ENTRY_SIZE(entrysize);
    pinfo->freecount = pinfo->entrycount;
    pinfo->pagestate = PageStateInfo_GroundState;

    FreeListEntry* current = pinfo->freelist;

    #ifndef ALLOC_DEBUG_CANARY
    for(int i = 0; i < pinfo->entrycount - 1; i++) {
        current->next = (FreeListEntry*)((char*)current + entrysize + sizeof(MetaData));
        current = current->next;
    }
    #else
    for(int i = 0; i < pinfo->entrycount - 1; i++) {
        debug_print("Current freelist pointer: %p\n", current);
        current->next = (FreeListEntry*)((char*)current + REAL_ENTRY_SIZE(entrysize));
        current = current->next;
    }
    #endif

    current->next = NULL;

    return pinfo;
}

static PageInfo* allocateFreshPage(uint16_t entrysize)
{
    void* page = mmap(NULL, BSQ_BLOCK_ALLOCATION_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    assert(page != MAP_FAILED);

    return initializePage(page, entrysize);
}

void getFreshPageForAllocator(AllocatorBin* alloc)
{
    if(alloc->page != NULL) {
        //need to rotate our old page into the collector, now alloc->page
        //exists in the needs_collection list
        alloc->page->pagestate = AllocPageInfo_ActiveEvacuation;
        alloc->page->next = alloc->page_manager->need_collection;
        alloc->page_manager->need_collection = alloc->page;
    }
    
    alloc->page->next = allocateFreshPage(alloc->entrysize);

    alloc->page = alloc->page->next;
    alloc->page->pagestate = AllocPageInfo_ActiveAllocation;

    //add new page to head of all pages list
    alloc->page->next = alloc->page_manager->all_pages;
    alloc->page_manager->all_pages = alloc->page;

    alloc->freelist = alloc->page->freelist;
    
    //this would be null already from allocateFreshPage, left incase im incorrect
    //alloc->page->next = NULL;
}

PageManager* initializePageManager(uint16_t entry_size)
{    
    PageManager* manager = &p_mgr;
    if (manager == NULL) {
        return NULL;
    }

    manager->all_pages = NULL;
    manager->need_collection = NULL;

    return manager;
}

AllocatorBin* initializeAllocatorBin(uint16_t entrysize, PageManager* page_manager)
{
    if (entrysize == 0 || page_manager == NULL) {
        return NULL;
    }

    AllocatorBin* bin = &a_bin;

    bin->page_manager = page_manager;

    bin->page = allocateFreshPage(entrysize);
    bin->freelist = bin->page->freelist;

    bin->page->next = page_manager->all_pages;
    page_manager->all_pages = bin->page;

    return bin;
}

void mark(Object* obj)
{
    MetaData* meta = META_FROM_OBJECT(obj);
    if(obj == NULL || meta->ismarked){
        return ;
    }

    meta->ismarked = true;
    for(size_t i = 0; i < obj->num_children; i++){
        mark(obj->children[i]);
    }
}

void markFromRoots()
{
    for(size_t i = 0; i < root_count; i++){
        mark(root_stack[i]);
    }
}

#ifdef ALLOC_DEBUG_CANARY
static inline bool verifyCanariesInBlock(char* block, uint16_t entry_size)
{
    uint64_t* pre_canary = (uint64_t*)(block);
    uint64_t* post_canary = (uint64_t*)(block + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        debug_print("[ERROR] Canary corrupted at block %p\n", (void*)block);
        debug_print("Data in pre-canary: %lx, data in post-canary: %lx\n", *pre_canary, *post_canary);
        return false;
    }
    return true;
}

static void verifyCanariesInPage(AllocatorBin* bin)
{
    PageInfo* page = bin->page;
    FreeListEntry* list = bin->freelist;
    char* base_address = (char*)page + sizeof(PageInfo);
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);

        if (metadata->isalloc) {
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }

    while(list){
        debug_print("Checking block: %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("Metadata state: isalloc=%d\n", metadata->isalloc);
        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated\n");
            assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    //make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);
}

static void verifyAllCanaries(AllocatorBin* bin)
{
    PageInfo* current_page = bin->page_manager->all_pages;

    while (current_page) {
        debug_print("PageManager all_pages head address: %p\n", current_page);
        verifyCanariesInPage(bin);
        current_page = current_page->next;
    }
}

void test_mark_single_object() {
    PageManager* pm = initializePageManager(DEFAULT_ENTRY_SIZE);
    assert(pm != NULL);

    AllocatorBin* bin = initializeAllocatorBin(DEFAULT_ENTRY_SIZE, pm);
    assert(bin != NULL);

    MetaData* metadata;
    Object* obj = (Object*)allocate(bin, &metadata);
    debug_print("Allcoated object at address : %p, %p\n", obj, metadata);
    assert(obj != NULL);

    markFromRoots();
    assert(metadata->ismarked == true);

    verifyAllCanaries(bin);

    debug_print("Test Case 1 Passed: Single object marked successfully.\n");
}
#endif