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

/* Whenever a pages freelist contains all blocks after cleanup add page here */
Stack backstore_pages;

/* Whenever a root is created it is inserted into this list */
ArrayList root_list;

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

// TODO : Move GC code into own file, this fella got big fast.
// TODO : Add concrete() collection method, trying to keep stages seperate.
// TODO : Looks like we will need some data structure to monitor old roots
// and a seperate to keep track of currently live roots (connected to live objects)

/**
* OVERVIEW OF GC CODE: 
*   
* This generational garbage collector is designed to have a compacted young space
* and a reference counted old space. The specifics of both have not been perfectly
* implemented thus far, but much of the core logic is present.
*
* EVACUATION:
*    - "evacuate_object(...)" handles movement of objects to evacuation page.
*       The data is directly memcpy'd into the evacuation page, where a forward
*       index is stored in both evac page and old page meta data to allow for easy
*       parent pointer updates. The old location of any object evacuated will be reset
*       in the pending_resets stack.
*    - "finalize_metadata_reset(...)" simply resets all metadata to a state allowing insertion
*       back onto free list.
*    - "update_children_pointers(...)" takes in a parent object and iterates through all children
*       looking for forward table entries. It will then use the childs forward index to update
*       its pointer to said child after it has been evacuated since it will reside in new memory.
*    - "evacuate(...)" iterates through the stack of roots generated in mark_from_roots(...) and
*       handles evacuation, updating parent pointers, and processing of root nodes. It is effectively
*       the glue for the previous three methods.
*
* MARKING: 
*    - "rebuild_freelist(...)" takes in a page and looks for any either non allocated or non referenced       
*       non root objects and puts their slot back onto the freelist.
*    - "clean_nonref_nodes(...)" interates through all pages and looks for nodes that need to have their
*       metadata reset so it can be visible to the freelist. May not be necessary.
*    - "mark_from_roots(...)" starts by iterating through our root stack and inserting all items onto a
*       work list. It then iterates through all roots children, and children children, ... looking to 
*       set the mark bit on all nodes reachable. Those not marked will be caught and returned to the
*       freelist when cleaned. It returns a BFS list of all roots and children visible for usage in 
*       evacuation. After marking is done it evacuates and cleans up any junk left over. This is the main
*       method used for actually collecting. 
**/

// Move object to evacuate_page and reset old metadata
static void* evacuate_object(AllocatorBin *bin, Object* obj, ArrayList* forward_table, Stack* pending_resets) {
    // No need to evacuate old objects
    if(META_FROM_OBJECT(obj)->isyoung == false) return NULL;
    if(META_FROM_OBJECT(obj)->forward_index != MAX_FWD_INDEX) return obj;

    // Insertion into forward table delayed until post evacuation 
    META_FROM_OBJECT(obj)->forward_index = forward_table->size;

    // Now we proceed with evacuation of young objects
    FreeListEntry* start_of_evac = bin->page_manager->evacuate_page->freelist;
    FreeListEntry* next_evac_freelist_object = start_of_evac->next;

    // Direct copy of data from original page to evacuation page
    void* evac_obj_base = memcpy(start_of_evac, BLOCK_START_FROM_OBJ(obj), REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE));

    //update freelist of evac page, memcpy was destroying pointers in our freelist for evac page so I had to manually store and assign after memcpy
    bin->page_manager->evacuate_page->freelist = next_evac_freelist_object;
    update_evacuation_freelist(bin);

    debug_print("[DEBUG] Object moved from %p to %p in evac page\n", obj, evac_obj_base);

    // Now that our object has been moved, we update our forward table with ptr to its new data
    Object* evac_obj_data = (Object*)(OBJ_START_FROM_BLOCK(evac_obj_base));
    add_to_list(forward_table, evac_obj_data);
    
    // Insert evacuated object into forward table and set index in meta
    add_to_list(forward_table, evac_obj_data);
    s_push(pending_resets, obj); //delayed updates

    // Decrementing reference to indicate that this instance is no longer used
    decrement_ref_count(obj);

    return evac_obj_data;
}

void finalize_metadata_reset(Stack* to_be_reset) {
    while(!s_is_empty(to_be_reset)) {
        MetaData* needs_reset = META_FROM_OBJECT((Object*)s_pop(to_be_reset));
        RESET_METADATA_FOR_OBJECT(needs_reset);
    }
}

/* Helper to update an objects children pointers */
void update_children_pointers(Object* obj, ArrayList* forward_table) {
    for (int i = 0; i < obj->num_children; i++) {
        Object* oldest = obj->children[i];

        if(META_FROM_OBJECT(oldest)->forward_index != MAX_FWD_INDEX) {
            obj->children[i] = forward_table->data[META_FROM_OBJECT(oldest)->forward_index];
        } else {
            obj->children[i] = oldest; // No evacuated object, keep original (may never occur)
        }
    }
}

void evacuate(Stack *marked_nodes_list, AllocatorBin *bin) {
    ArrayList worklist;
    Stack pending_resets;
    initialize_list(&worklist);
    stack_init(&pending_resets);

    /**
    * pending_resets contains old locations in memory of objects we evacuate into evac page.
    * This is necessary so we can control when the blocks in pending_resets are returned back 
    * to the free list of their respective page. This enables us to use metadata in these old
    * locations to grab forward indexs and update parents child pointers.
    **/

    ArrayList* forward_table = &f_table;
    if(!forward_table) {
        initialize_list(forward_table);
    }

    while(!s_is_empty(marked_nodes_list)) {
        Object* obj = (Object*)s_pop(marked_nodes_list);
        
        if(META_FROM_OBJECT(obj)->isroot == false) {
            if(obj->num_children == 0) {
                evacuate_object(bin, obj, forward_table, &pending_resets);
            } else {
                // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
                update_children_pointers(obj, forward_table);
                evacuate_object(bin, obj, forward_table, &pending_resets);
            }
        } else{
            // If the object we are trying to evacuate has children we will first update its children pointers then evacuate.
            update_children_pointers(obj, forward_table);
        }
    }

    finalize_metadata_reset(&pending_resets);
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
                last_freelist_entry = page->freelist;
                first_nonalloc_block = false;
            } else {
                last_freelist_entry->next = new_freelist_entry;
                new_freelist_entry->next = NULL; 
                last_freelist_entry = new_freelist_entry;
            }
            
            page->freecount++;
        }
    }

    /* If our page is completly clean rotate into backstore_pages */
    if(page->freecount == page->entrycount) {
        debug_print("[DEBUG] Copied empty page into backstore_pages\n");
        s_push(&backstore_pages, page);
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

            /* Clear objects that are either not marked or non roots with zero ref count */
            if((current_object_meta->ismarked == false && current_object_meta->isalloc == true)
                || (current_object_meta->isroot == false && current_object_meta->ref_count == 0)) {
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

    /* Add all root objects to the worklist that need processing (young roots pretty much)*/
    debug_print("[DEBUG] Root list contains %li roots.\n", get_list_size(&root_list));
    for (uint16_t i = root_list.head; i < root_list.tail; i++) {
        Object* root = root_list.data[i];
        MetaData* meta = META_FROM_OBJECT(root);

        /* We want to skip objects with ref count > 0 */
        if(meta->ref_count > 0) continue;

        /* If an object is old we should not mark children */
        if (meta->ref_count == 0 && !meta->ismarked && meta->isyoung) {
            meta->ismarked = true;
            add_to_list(&worklist, root);
        }
    }
    root_count = 0;

    /* Process the worklist in a BFS manner */
    while (!is_list_empty(&worklist)) {
        Object* parent = remove_head_from_list(&worklist);

        for (size_t i = 0; i < parent->num_children; i++) {
            Object* child = parent->children[i];

            /* We increment the childs ref count since the parent points to it */
            increment_ref_count(child);

            if (child != NULL) {
                MetaData* child_meta = META_FROM_OBJECT(child);

                /* We should only process young objects */
                if (!child_meta->ismarked && child_meta->isyoung) 
                {
                    child_meta->ismarked = true;
                    add_to_list(&worklist, child);
                }
            }
        }
        /* We finished processing this node, add to mark list */
        s_push(&marked_nodes_stack,  parent);
    }
    debug_print("Size of marked work list %li\n", marked_nodes_stack.size);

    /* Make sure objects are in correct order in marked nodes list */
    for(size_t i = 0; i < marked_nodes_stack.size; i++) {
        debug_print("node %p\n", marked_nodes_stack.data[i]);
    }

    evacuate(&marked_nodes_stack, bin);

    clean_nonref_nodes(bin);
}
