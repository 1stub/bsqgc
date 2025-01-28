#include "allocator.h"
#include <stdio.h>

#define CANARY_DEBUG_CHECK

size_t root_count = 0;

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .evacuate_page = NULL, .filled_pages = NULL};

/* Our stack of roots to be marked after allocations finish */
Object* root_stack[MAX_ROOTS];

/* To be used with updating pointers after moving to evac page */
Worklist* forward_table;

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
        alloc->page->next = alloc->page_manager->filled_pages;
        alloc->page_manager->filled_pages = alloc->page;
    }
    PageInfo* new_page = allocateFreshPage(alloc->entrysize);

    if(alloc->page == NULL) {
        alloc->page = new_page;
    } else {
        alloc->page->next = new_page;
        alloc->page = alloc->page->next;
    }

    alloc->page->pagestate = AllocPageInfo_ActiveAllocation;

    /* add new page to end of all pages list */
    if(alloc->page_manager->all_pages == NULL) {
        alloc->page_manager->all_pages = alloc->page;
    } else {
        PageInfo* current  = alloc->page_manager->all_pages;
        while(current->next != NULL) {
            current = current->next;
        }
        current->next = alloc->page;
    }

    alloc->freelist = alloc->page->freelist;
    
    alloc->page->next = NULL;
}

PageManager* initializePageManager(uint16_t entry_size)
{    
    PageManager* manager = &p_mgr;
    if (manager == NULL) {
        return NULL;
    }

    return manager;
}

AllocatorBin* initializeAllocatorBin(uint16_t entrysize)
{
    AllocatorBin* bin = &a_bin;
    if(bin == NULL) return NULL;

    bin->page_manager = &p_mgr;
    return bin;
}

bool isRoot(void* obj) 
{
    /** 
    * TODO: Actually implement logic for determining whether a pointer is root,
    * this would be logic that comes into play AFTER we implement type systems.
    * --- I think ;)
    **/
    return true; // For now, assume all objects are valid pointers
}

/* Worklist helpers */
static void initialize_worklist(Worklist* worklist) 
{
    worklist->size = 0;
}

#if 0
static Object* worklist_top(Worklist* worklist)
{
    assert(worklist->size > 0);

    if(worklist->size > 0) {
        return worklist->data[worklist->size - 1];
    } else {
        return worklist->data[0];
    }
}
#endif

static bool add_to_worklist(Worklist* worklist, Object* obj) 
{
    if (worklist->size >= WORKLIST_CAPACITY) {
        /* Worklist is full */
        debug_print("Worklist overflow!\n");
        return false;
    }
    worklist->data[worklist->size++] = obj;
    return true;
}

// Dont need this methid quite yet
static Object* remove_from_worklist(Worklist* worklist) 
{
    if (worklist->size == 0) {
        return NULL;
    }
    return worklist->data[--worklist->size]; //prefix decrement crucial here
}

static bool is_worklist_empty(Worklist* worklist) 
{
    return worklist->size == 0;
}





// Move obj to evacuate_page
static void evacuate_object(Object* obj) {
    //MetaData* metadata = META_FROM_OBJECT(obj);

}

void evacuate(Worklist *marked_nodes_list, AllocatorBin *bin) {
    while(!is_worklist_empty(marked_nodes_list)) {
        Object* obj = remove_from_worklist(marked_nodes_list);
        evacuate_object(obj);
    }
}

/* Algorithm 2.2 from The Gargage Collection Handbook */
void mark_from_roots()
{
    Worklist marked_nodes_list, worklist;
    initialize_worklist(&marked_nodes_list);
    initialize_worklist(&worklist);

    /* Add all root objects to the worklist */
    for (size_t i = 0; i < root_count; i++)
    {
        Object* root = root_stack[i];
        MetaData* meta = META_FROM_OBJECT(root);
        if (root != NULL && !meta->ismarked) 
        {
            meta->ismarked = true;
            if (!add_to_worklist(&worklist, root)) 
            {
                return ; // Abort marking if worklist overflows
            }
        }
    }
    root_count = 0;

    /* Process the worklist in a BFS manner */
    while (!is_worklist_empty(&worklist)) 
    {
        Object* obj = remove_from_worklist(&worklist);

        for (size_t i = 0; i < obj->num_children; i++) 
        {
            Object* child = obj->children[i];
            if (child != NULL) 
            {
                MetaData* child_meta = META_FROM_OBJECT(child);
                if (!child_meta->ismarked) 
                {
                    child_meta->ismarked = true;
                    if (!add_to_worklist(&worklist, child)) 
                    {
                        return; // Abort marking if worklist overflows
                    }
                }
            }
        }
        // We finished processing this node, add to mark list
        add_to_worklist(&marked_nodes_list,  obj);
    }

    debug_print("Size of marked work list %li\n", marked_nodes_list.size);

    // Now that we have constructed a BFS order work list we can evacuate non root nodes
    evacuate(&marked_nodes_list, &a_bin);
}

bool verifyCanariesInBlock(char* block, uint16_t entry_size)
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

void verifyCanariesInPage(PageInfo* page)
{
    FreeListEntry* list = page->freelist;
    char* base_address = (char*)page + sizeof(PageInfo);
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        debug_print("Checking block: %p\n", block_address);
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("Metadata state: isalloc=%d\n", metadata->isalloc);

        if (metadata->isalloc) {
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }

    debug_print("\n");

    while(list){
        debug_print("Checking freelist block: %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("Metadata state: isalloc=%d\n", metadata->isalloc);
        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated\n");
            //assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    // Make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);

    debug_print("\n");
}

void verifyAllCanaries(AllocatorBin* bin)
{
    PageInfo* current_page = bin->page_manager->all_pages;

    while (current_page) {
        debug_print("PageManager all_pages head address: %p\n", current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }

}