#include "allocator.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CANARY_DEBUG_CHECK

size_t root_count = 0;

/* Static declarations of our allocator bin and page manager structures */
AllocatorBin a_bin = {.freelist = NULL, .entrysize = DEFAULT_ENTRY_SIZE, .page = NULL, .page_manager = NULL};
PageManager p_mgr = {.all_pages = NULL, .evacuate_page = NULL, .filled_pages = NULL};

/* Forwarding table to be used with updating pointers after moving to evac page */
ArrayList f_table = {.size = 0};

/* Our stack of roots to be marked after allocations finish */
Object* root_stack[MAX_ROOTS];

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

    // Add new page to all pages list
    new_page->next = alloc->page_manager->all_pages;
    alloc->page_manager->all_pages = new_page;

    // Make sure the current alloc page points to this entry in the list
    alloc->page = new_page;
    alloc->freelist = new_page->freelist;
}

AllocatorBin* initializeAllocatorBin(uint16_t entrysize)
{
    AllocatorBin* bin = &a_bin;
    if(bin == NULL) return NULL;

    bin->page_manager = &p_mgr;
    bin->page_manager->evacuate_page = allocateFreshPage(DEFAULT_ENTRY_SIZE);

    getFreshPageForAllocator(bin);

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

static void update_evacuation_freelist(AllocatorBin *bin) {
    if (bin->page_manager->evacuate_page->freelist == NULL) {
        bin->page_manager->evacuate_page->next = allocateFreshPage(bin->page_manager->evacuate_page->entrysize);
        bin->page_manager->evacuate_page = bin->page_manager->evacuate_page->next;
        bin->page_manager->evacuate_page->next = NULL;
    }
}

// Move object to evacuate_page and reset old metadata
static void* evacuate_object(AllocatorBin *bin, Object* obj, ArrayList* forward_table) {
    FreeListEntry* start_of_evac = bin->page_manager->evacuate_page->freelist;
    FreeListEntry* next_evac_freelist_object = start_of_evac->next;

    // Direct copy of data from original page to evacuation page
    void* evac_obj_base = memcpy(start_of_evac, BLOCK_START_FROM_OBJ(obj), REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE));

    //update freelist of evac page, memcpy was destroying pointers in our freelist for evac page so I had to manually store and assign after memcpy
    bin->page_manager->evacuate_page->freelist = next_evac_freelist_object;
    update_evacuation_freelist(bin);

    debug_print("[DEBUG] Object moved from %p to %p in evac page\n", obj, evac_obj_base);

    Object* evac_obj_data = (Object*)(OBJ_START_FROM_BLOCK(evac_obj_base));
    MetaData* evac_obj_meta = META_FROM_OBJECT(evac_obj_data);
    
    // Insert evacuated object into forward table and set index in meta
    evac_obj_meta->forward_index = forward_table->size;
    add_to_list(forward_table, evac_obj_data);

    MetaData* old_metadata = META_FROM_OBJECT(obj);
    RESET_METADATA_FOR_OBJECT(old_metadata);

    return evac_obj_data;
}

/* Helper to update an objects children pointers */
void update_children_pointers(Object* obj, ArrayList* worklist, ArrayList* forward_table) {
    for (int i = 0; i < obj->num_children; i++) {
        Object* oldest = remove_head_from_list(worklist);
        obj->children[i] = forward_table->data[META_FROM_OBJECT(oldest)->forward_index];
    }
}

void evacuate(Stack *marked_nodes_list, AllocatorBin *bin) {
    ArrayList worklist;
    initialize_list(&worklist);

    ArrayList* forward_table = &f_table;
    if(!forward_table) {
        initialize_list(forward_table);
    }

    while(!s_is_empty(marked_nodes_list)) {
        Object* obj = s_pop(marked_nodes_list);
        
        if(META_FROM_OBJECT(obj)->isroot == false) {
            if(obj->num_children == 0) {
                add_to_list(&worklist, (Object*)evacuate_object(bin, obj, forward_table));
            } else {
                // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
                update_children_pointers(obj, &worklist, forward_table);
                add_to_list(&worklist, (Object*)evacuate_object(bin, obj, forward_table));
            }
        } else{
            // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
            update_children_pointers(obj, &worklist, forward_table);
            add_to_list(&worklist, obj); //Difference is we dont evac a root
        }
    }
}

void rebuild_freelist(AllocatorBin* bin, PageInfo* page) {
    page->freelist = NULL;
    page->freecount = 0;
    
    FreeListEntry* last_freelist_entry = NULL;
    bool first_nonalloc_block = true;

    for (size_t i = 0; i < page->entrycount; i++) {
        FreeListEntry* new_freelist_entry = FREE_LIST_ENTRY_AT(page, i);
        MetaData* meta = META_FROM_FREELIST_ENTRY(new_freelist_entry);

        if (!meta->isalloc) {
            if(first_nonalloc_block) {
                page->freelist = new_freelist_entry;
                page->freelist->next = NULL;
                first_nonalloc_block = false;
            } else {
                last_freelist_entry->next = new_freelist_entry;
                new_freelist_entry->next = NULL; 
            }
            last_freelist_entry = new_freelist_entry;
            
            page->freecount++;
        }
    }

    /**
    * Currently I have no idea why I need to manually set the bins page here,
    * I feel like manually doing this setting could create some really weird
    * problems when running larger tests. It seems that modifying the current 
    * page in bin through our page manager does not properly reflect in
    * bin->page/freelist itself. hmm...
    **/

    /* Now update the current page for bin if there are freeblocks */
    if(page->freecount > 0) {
        bin->page = page;
        bin->freelist = page->freelist;
    }

    debug_print("[DEBUG] Rebuilt freelist for page %p with %d free blocks\n", page, page->freecount);
}

// Just run through all pages and flag nodes that arent marked as non allocated --- may not be necessary
void clean_nonref_nodes(AllocatorBin* bin) {
    PageInfo* current_page = bin->page_manager->all_pages;

    while(current_page != NULL) {
        for(uint16_t i = 0; i < current_page->entrycount; i++) {
            Object* current_object = OBJECT_AT(current_page, i);
            MetaData* current_object_meta = META_FROM_OBJECT(current_object);

            if(current_object_meta->ismarked == false && current_object_meta->isalloc == true) {
                RESET_METADATA_FOR_OBJECT(current_object_meta);
            }
        }

        debug_print("[DEBUG] bin->freelist before rebuilding: %p\n", bin->freelist);
        rebuild_freelist(bin, current_page);
        debug_print("[DEBUG] bin->freelist after rebuilding: %p\n", bin->freelist);
        current_page = current_page->next;
    }
}

/* Algorithm 2.2 from The Gargage Collection Handbook */
void mark_from_roots(AllocatorBin* bin)
{
    Stack marked_nodes_stack;
    ArrayList worklist;

    stack_init(&marked_nodes_stack);
    initialize_list(&worklist);

    /* Add all root objects to the worklist */
    for (size_t i = 0; i < root_count; i++)
    {
        Object* root = root_stack[i];
        MetaData* meta = META_FROM_OBJECT(root);
        if (root != NULL && !meta->ismarked) 
        {
            meta->ismarked = true;
            add_to_list(&worklist, root);
        }
    }
    root_count = 0;

    /* Process the worklist in a BFS manner */
    while (!is_list_empty(&worklist)) 
    {
        Object* obj = remove_head_from_list(&worklist);

        for (size_t i = 0; i < obj->num_children; i++) 
        {
            Object* child = obj->children[i];
            if (child != NULL) 
            {
                MetaData* child_meta = META_FROM_OBJECT(child);
                if (!child_meta->ismarked) 
                {
                    child_meta->ismarked = true;
                    add_to_list(&worklist, child);
                }
            }
        }
        // We finished processing this node, add to mark list
        s_push(&marked_nodes_stack,  obj);
    }
    debug_print("Size of marked work list %li\n", marked_nodes_stack.size);

    /* Make sure objects are in correct order in marked nodes list */
    for(size_t i = 0; i < marked_nodes_stack.size; i++) {
        debug_print("node %p\n", marked_nodes_stack.data[i]);
    }

    /** 
    * Since this algorithm generates marked_nodes_list in BFS manner, we can 
    * take a much easier and faster approach to updating parent pointers correctly
    **/
    evacuate(&marked_nodes_stack, bin);

    clean_nonref_nodes(bin);
}
