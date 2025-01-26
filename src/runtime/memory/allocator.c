#include "allocator.h"

#define CANARY_DEBUG_CHECK

size_t root_count = 0;

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .evacuate_page = NULL, .filled_pages = NULL};

/* Our stack of roots to be marked after allocations finish */
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
        alloc->page->next = alloc->page_manager->filled_pages;
        alloc->page_manager->filled_pages = alloc->page;
    }
    
    alloc->page->next = allocateFreshPage(alloc->entrysize);

    alloc->page = alloc->page->next;
    alloc->page->pagestate = AllocPageInfo_ActiveAllocation;

    /* add new page to head of all pages list */
    alloc->page->next = alloc->page_manager->all_pages;
    alloc->page_manager->all_pages = alloc->page;

    alloc->freelist = alloc->page->freelist;
    
    alloc->page->next = NULL;
}

PageManager* initializePageManager(uint16_t entry_size)
{    
    PageManager* manager = &p_mgr;
    if (manager == NULL) {
        return NULL;
    }

    manager->all_pages = NULL;
    manager->filled_pages = NULL;
    manager->evacuate_page = NULL;

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

/**
* For now this brute force checks all children of all objects (?)
* to find a given objects parent, if the old child matches, we update to new child
**/
void update_parent_pointers(Object* old_obj, Object* new_obj) {

}

void evacuate(Worklist* marked_nodes_list) 
{
    /** 
    * General approach: Iterate through the entire marked_nodes_list. Every non-root object will
    * be moved to the evacuation pages. For each moved object:
    * - Update its forward_index to reflect its new location in the evacuation pages.
    * - The forward_index stores the address (or index) of the moved object.
    * 
    * After moving the object, we need to update the parent pointers that reference this object:
    * - For each parent of this object, we use the forward_index to find the object's new location 
    *   in the evacuation pages and update the parent's reference (pointer) to the new location.
    **/

    /* Now an even better question is how to efficiently find the parents of this object... */

    while(!is_worklist_empty(marked_nodes_list)) {
        //we will simply memcpy to new evac list

    }
}

/* Algorithm 2.2 from The Gargage Collection Handbook */
void mark_from_roots()
{
    Worklist marked_nodes_list;
    initialize_worklist(&marked_nodes_list);

    /* Add all root objects to the worklist */
    for (size_t i = 0; i < root_count; i++)
    {
        Object* root = root_stack[i];
        MetaData* meta = META_FROM_OBJECT(root);
        if (root != NULL && !meta->ismarked) 
        {
            meta->ismarked = true;
            if (!add_to_worklist(&marked_nodes_list, root)) 
            {
                return ; // Abort marking if worklist overflows
            }
        }
    }
    root_count = 0;

    /* Process the worklist in a BFS manner */
    while (!is_worklist_empty(&marked_nodes_list)) 
    {
        Object* obj = remove_from_worklist(&marked_nodes_list);

        for (size_t i = 0; i < obj->num_children; i++) 
        {
            Object* child = obj->children[i];
            if (child != NULL) 
            {
                MetaData* child_meta = META_FROM_OBJECT(child);
                if (!child_meta->ismarked) 
                {
                    child_meta->ismarked = true;
                    if (!add_to_worklist(&marked_nodes_list, child)) 
                    {
                        return; // Abort marking if worklist overflows
                    }
                }
            }
        }
    }
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

void verifyCanariesInPage(AllocatorBin* bin)
{
    PageInfo* page = bin->page;
    FreeListEntry* list = bin->freelist;
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
            assert(0);
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
        verifyCanariesInPage(bin);
        current_page = current_page->next;
    }

}