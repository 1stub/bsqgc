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

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);

        if (metadata->isalloc) {
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }
}

static void verifyAllCanaries(PageManager* page_manager)
{
    PageInfo* current_page = page_manager->all_pages;

    printf("PageManager all_pages head address: %p\n", current_page);

    while (current_page) {
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }
}
#endif

void runTests(){
    uint16_t entry_size = 16;

    PageManager* pm = initializePageManager(entry_size);
    assert(pm != NULL);
    
    AllocatorBin* bin = initializeAllocatorBin(entry_size, pm);
    assert(bin != NULL);

    //create n objs where the last clobbers a canary
    int num_objs = 256;
    for( ; num_objs > 0; num_objs--){
        MetaData* metadata;

        void* raw_obj = allocate(bin, &metadata);
        assert(raw_obj != NULL);

        uint64_t* obj = (uint64_t*)raw_obj;

        printf("Object allocated at address: %p\n", obj);
        
        if(num_objs == 1){
            //now lets try putting something "malicious" at this addr...
            #ifdef CANARY_DEBUG_CHECK
            uint16_t overflow_size = entry_size;
            for (uint16_t i = 0; i < overflow_size; i++){
                obj[i] = 0xBAD0000000000000;
            }
            #else
            //doesnt seem to work for destroying data at the canary header
            obj[-2] = 0xBADAAAAAAAAAAAAA;
            #endif
        }else{
            *obj = ALLOC_DEBUG_MEM_INITIALIZE_VALUE;
        }

        printf("Data stored at %p: %lx\n", obj, *obj); 
        
        //assert(validate(obj, bin, metadata)); 
        printf("\n");
    }

    #ifdef ALLOC_DEBUG_CANARY
    verifyAllCanaries(pm);
    #endif
}

#if 0
void* fool_alloc(Page* p, size_t size) {
    if(((char*)p->block_list_head + (int)size) - (char*)(p->block_list_head) > PAGE_SIZE){

#ifdef DEBUG_LOG
    printf("Not enough free space to allocate given size\n\n");
#endif 
        return NULL;
    }
    if(!size){

#ifdef DEBUG_LOG
    printf("Invalid size attempted to be allocated\n\n");
#endif 
        return NULL;
    }

    block_header* head = p->block_list_tail; //ptr to the last block allocd
    int n_blocks = (size + (BLOCK_SIZE - sizeof(block_header)) - 1) 
        / (BLOCK_SIZE - sizeof(block_header)); //round up to nearest block size

#ifdef DEBUG_LOG
    printf("For size %i data needed %i blocks\n", (int)size, n_blocks);
#endif
    
    //First we need to make sure there is enough contiguous space to allocate our blocks
    int contig_free = 0; 
    int start_index = -1;
    for(int i = 0; i < NUM_BLOCKS_PER_PAGE; i++){
        if(p->free_list[i]){
            //set our starting index to the first element we find that is free
            if(contig_free == 0){
                start_index = i; 
            }

            contig_free++;
            if(contig_free == n_blocks){
                break;
            }
        }else{
            contig_free = 0;
        }
    } 

    if(contig_free < n_blocks){

#ifdef DEBUG_LOG
    printf("Unable to find enough contiguous blocks to be allocated\n\n");
#endif
        return NULL;
    }

    //now that we have found a chunk of space fitting our size we can actually
    //link the blocks together
    void* data_bp = 
        (char*)p->block_list_head + (NUM_BLOCKS_PER_PAGE * sizeof(block_header));
    block_header* last_block;
    block_header* first_block;
    for(int i = 0; i < n_blocks; i++){
        p->free_list[start_index + i] = 0;
        block_header* b = (block_header*)((char*)head + ((start_index + i) * BLOCK_SIZE));
        
        if(i == 0){
            first_block = b;
        
        }
        b->data = (char*)data_bp + (i * (BLOCK_SIZE - sizeof(block_header)));
        b->next = (block_header*)((char*)b + sizeof(block_header));

#ifdef DEBUG_LOG
    printf("Allocated block header starting at %p\n", b);
    printf("Allocated block data segment starting at %p\n", b->data);
#endif

        p->block_list_tail = b;

        if(i == (n_blocks - 1)){
            last_block = b;
        }
    }

    p->block_list_tail = last_block->next;

#ifdef DEBUG_LOG
    printf("FINSHED ALLOCATING OBJECT\n\n");
#endif

    return (void*)(first_block->data);
}


void fool_free(Page* p, void* obj) {
    //we will be passed a pointer to the data segment of our block
    //this will need to be converted back into block in the header 
    //portion of our page

    /*
     
    int offset = (char*)obj - (char*)(p->block_list_head);
    int start_index = offset / BLOCK_SIZE;

    //how many blocks are associated with given object
    int allocated_blocks = ((block_header*)obj)->size / BLOCK_SIZE;
    
    while(allocated_blocks){
        p->free_list[start_index + allocated_blocks] = 1;
        allocated_blocks--;
    }

    */
#ifdef DEBUG_LOG
   // printf("Freed block at: %p\n", block);
#endif
}


void destroy_page(Page* p) {

#ifdef DEBUG_LOG
    printf("Destroyed page: %p\n", p);
#endif

    munmap(p, PAGE_SIZE);
}


int main(){
    Page* page = allocate_page();
   
    //we are trying to allocate one 64 byte chunk and another
    //128 byte chunk
    //then 8192 byte block which will fail (for now) since we 
    //exceede a page size
    void* object_one = fool_alloc(page, 64);
    void* object_two = fool_alloc(page, 128);

    void* failed_alloc = fool_alloc(page, 8192);

    int* data = (int*)object_one;
    *data = 0x42;

    printf("Data stored at object one: %i\n", *data);

    //if either are null we should make a new page - fine for now
    if(object_one != NULL && object_two != NULL){
        fool_free(page, object_one);
        fool_free(page, object_two);
    }

    destroy_page(page);
    return 0;
}
#endif