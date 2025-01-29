#include "allocator.h"
#include <stdint.h>
#include <stdio.h>

#define CANARY_DEBUG_CHECK

size_t root_count = 0;

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .evacuate_page = NULL, .filled_pages = NULL};

/* Forwarding table to be used with updating pointers after moving to evac page */
Worklist f_table = {.size = 0};

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

    manager->evacuate_page = allocateFreshPage(DEFAULT_ENTRY_SIZE);
    debug_print("Created page for evacuation at %p\n", manager->evacuate_page);

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

// Move object to evacuate_page and reset old metadata
static void* evacuate_object(AllocatorBin *bin, Object* obj, Worklist* forward_table) {
    FreeListEntry* start_of_evac = bin->page_manager->evacuate_page->freelist;
    FreeListEntry* next_evac_freelist_object = start_of_evac->next;

    // Direct copy of data from original page to evacuation page
    void* evac_obj_base = memcpy(start_of_evac, BLOCK_START_FROM_OBJ(obj), REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE));

    //update freelist of evac page, memcpy was destroying pointers in our freelist for evac page so I had to manually store and assign after memcpy
    bin->page_manager->evacuate_page->freelist = next_evac_freelist_object;

    debug_print("[DEBUG] Object moved from %p to %p in evac page\n", obj, evac_obj_base);

    Object* evac_obj_data = (Object*)(OBJ_START_FROM_BLOCK(evac_obj_base));
    MetaData* evac_obj_meta = META_FROM_OBJECT(evac_obj_data);
    
    // Insert evacuated object into forward table and set index in meta
    evac_obj_meta->forward_index = forward_table->size;
    add_to_worklist(forward_table, evac_obj_data);

    // Update the freelist of the original page to point to the newly freed block
    // I suspect I could handle this freelist stuff within my rebuild freelist method.
    FreeListEntry* new_freelist_entry = (FreeListEntry*)(BLOCK_START_FROM_OBJ(obj));
    new_freelist_entry->next = bin->freelist;
    bin->freelist = new_freelist_entry;

    //Have to manually set freelist of bin and page -- not totally sure why.
    bin->page->freelist = bin->freelist;

    MetaData* old_metadata = (MetaData*)((char*)bin->freelist + ALLOC_DEBUG_CANARY_SIZE);
    RESET_METADATA_FOR_OBJECT(old_metadata);

    return evac_obj_data;
}

/* Helper to update an objects children pointers */
void update_children_pointers(Object* obj, Worklist* worklist, Worklist* forward_table) {
    for (int i = 0; i < obj->num_children; i++) {
        Object* oldest = remove_oldest_from_worklist(worklist);
        obj->children[i] = forward_table->data[META_FROM_OBJECT(oldest)->forward_index];
    }
}

void evacuate(Worklist *marked_nodes_list, AllocatorBin *bin) {
    Worklist worklist;
    initialize_worklist(&worklist);

    Worklist* forward_table = &f_table;
    if(!forward_table) {
        initialize_worklist(forward_table);
    }

    while(!is_worklist_empty(marked_nodes_list)) {
        Object* obj = remove_from_worklist(marked_nodes_list);
        
        if(META_FROM_OBJECT(obj)->isroot == false) {
            if(obj->num_children == 0) {
                add_to_worklist(&worklist, (Object*)evacuate_object(bin, obj, forward_table));
            } else {
                // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
                update_children_pointers(obj, &worklist, forward_table);
                add_to_worklist(&worklist, (Object*)evacuate_object(bin, obj, forward_table));
            }
        } else{
            // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
            update_children_pointers(obj, &worklist, forward_table);
            add_to_worklist(&worklist, obj); //Difference is we dont evac a root
        }
    }
}

void rebuild_freelist(PageInfo* page) {
    page->freelist = NULL;
    page->freecount = 0;

    for(size_t i = 0; i < page->entrycount; i++) {
        FreeListEntry* new_freelist_entry = (FreeListEntry*)(PAGE_OFFSET(page) + (i * REAL_ENTRY_SIZE(page->entrysize)));
        MetaData* meta = (MetaData*)((char*)new_freelist_entry + ALLOC_DEBUG_CANARY_SIZE);

        if(meta->isalloc == false) {
            new_freelist_entry->next = page->freelist;
            page->freelist = new_freelist_entry;

            page->freecount++;
        }
    }
}

// Just run through all pages and flag nodes that arent marked as non allocated --- may not be necessary
void clean_nonref_nodes() {
    PageInfo* current_page = p_mgr.all_pages;

    while(current_page != NULL) {
        for(uint16_t i = 0; i < current_page->entrycount; i++) {
            Object* current_object = (Object*)(PAGE_OFFSET(current_page) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) +
                (i * REAL_ENTRY_SIZE(current_page->entrysize)));
            MetaData* current_object_meta = META_FROM_OBJECT(current_object);

            debug_print("[DEBUG] Checking object at %p is not allocated and not marked\n", current_object);

            if(current_object_meta->ismarked == false && current_object_meta->isalloc == true) {
                debug_print("[DEBUG] Found non marked object at %p\n", current_object);
                RESET_METADATA_FOR_OBJECT(current_object_meta);
            }
        }
        rebuild_freelist(current_page);
        current_page = current_page->next;
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
        Object* obj = remove_oldest_from_worklist(&worklist);

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

    for(size_t i = 0; i < marked_nodes_list.size; i++) {
        debug_print("node %p\n", marked_nodes_list.data[i]);
    }

    // Now that we have constructed a BFS order work list we can evacuate non root nodes
    evacuate(&marked_nodes_list, &a_bin);

    clean_nonref_nodes();
}