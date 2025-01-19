#include "allocator.h"

#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

#define CANARY_DEBUG_CHECK
//for future impls that can have multiple different entry sizes we can create an 
//array of pre defined allocator bins for a given size
#define DEFAULT_ENTRY_SIZE 16 //for now our blocks are all have dataseg of 16 bytes

AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .need_collection = NULL};

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

bool isPtr(void* obj) {
    // TODO: Implement actual logic to determine if obj is a valid pointer
    return true; // For now, assume all objects are valid pointers
}

void mark(void* obj)
{

}

void markFromRoots()
{

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

static void verifyCanariesInPage(PageInfo* page)
{
    FreeListEntry* list = page->freelist;
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
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated");
            assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    //make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);
}

static void verifyAllCanaries(PageManager* page_manager)
{
    PageInfo* current_page = page_manager->all_pages;

    while (current_page) {
        debug_print("PageManager all_pages head address: %p\n", current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }

}

//the following methods are just to verify the functionality of this collector.
//we need to (as of now) be able to create linked objects of varrying depth and breadth,
//verify that no canaries were smashed in the process for each object, and
//starting at a root node (directly accessable in the page) mark all necessary nodes
typedef struct Object{
    void* nextobj;
}Object;

void runTests(){
    PageManager* pm = initializePageManager(DEFAULT_ENTRY_SIZE);
    assert(pm != NULL);
    
    AllocatorBin* bin = initializeAllocatorBin(DEFAULT_ENTRY_SIZE, pm);
    assert(bin != NULL);

    int num_objs = 256;
    for( ; num_objs > 0; num_objs--){
        MetaData* metadata;

        void* raw_obj = allocate(bin, &metadata);
        assert(raw_obj != NULL);

        uint64_t* obj = (uint64_t*)raw_obj;

        debug_print("Object allocated at address: %p\n", obj);
        
        uint16_t overflow_size = 3; //3 always writes to either pre or post
        
        //this value doesnt matter, just need to destroy a canary somewhere
        if(num_objs == DEFAULT_ENTRY_SIZE + 1){
            //now lets try putting something "malicious" at this addr...
            #ifdef CANARY_DEBUG_CHECK
            for (uint16_t i = 0; i < overflow_size; i++){
                obj[i] = 0xBAD0000000000000;
            }
            #else
            obj[-overflow_size] = 0xBADAAAAAAAAAAAAA;
            #endif
        }else{
            *obj = ALLOC_DEBUG_MEM_INITIALIZE_VALUE;
        }

        debug_print("Data stored at %p: %lx\n\n", obj, *obj);         
    }
    verifyAllCanaries(pm);
}
#endif