#include "allocator.h"

#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

#define CANARY_DEBUG_CHECK

static PageInfo* initializePage(void* page, uint16_t entrysize)
{
    printf("New page!\n");

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
        printf("Current freelist pointer: %p\n", current);
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
        alloc->page->pagestate = AllocPageInfo_ActiveAllocation;
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
    PageManager* manager = (PageManager*)malloc(sizeof(PageManager));
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

    AllocatorBin* bin = (AllocatorBin*)malloc(sizeof(AllocatorBin));
    if (bin == NULL) {
        return NULL;
    }

    bin->entrysize = entrysize;
    bin->page_manager = page_manager;

    bin->page = allocateFreshPage(entrysize);
    bin->freelist = bin->page->freelist;

    bin->page->next = page_manager->all_pages;
    page_manager->all_pages = bin->page;

    return bin;
}

#ifdef ALLOC_DEBUG_CANARY
static inline bool verifyCanariesInBlock(char* block, uint16_t entry_size)
{
    uint64_t* pre_canary = (uint64_t*)(block);
    uint64_t* post_canary = (uint64_t*)(block + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        printf("[ERROR] Canary corrupted at block %p\n", (void*)block);
        printf("Data in pre-canary: %lx, data in post-canary: %lx\n", *pre_canary, *post_canary);
        return false;
    }
    return true;
}

static void verifyCanariesInPage(PageInfo* page)
{
    char* base_address = (char*)page + sizeof(PageInfo);
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);

        if (metadata->isalloc) {
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }else{
            free_blocks++;
        }
    }
    //make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);
}

static void verifyAllCanaries(PageManager* page_manager)
{
    PageInfo* current_page = page_manager->all_pages;

    while (current_page) {
        printf("PageManager all_pages head address: %p\n", current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }
}

void runTests(){
    uint16_t entry_size = 16;

    PageManager* pm = initializePageManager(entry_size);
    assert(pm != NULL);
    
    AllocatorBin* bin = initializeAllocatorBin(entry_size, pm);
    assert(bin != NULL);

    int num_objs = 256;
    for( ; num_objs > 0; num_objs--){
        MetaData* metadata;

        void* raw_obj = allocate(bin, &metadata);
        assert(raw_obj != NULL);

        uint64_t* obj = (uint64_t*)raw_obj;

        printf("Object allocated at address: %p\n", obj);
        
        uint16_t overflow_size = 3; //3 always writes to either pre or post
        
        //this value doesnt matter, just need to destroy a canary somewhere
        if(num_objs == entry_size + 1){
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

        printf("Data stored at %p: %lx\n\n", obj, *obj);         
    }
    verifyAllCanaries(pm);
}
#endif



