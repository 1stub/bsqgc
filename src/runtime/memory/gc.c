#include "gc.h"

/* Forwarding table to be used with updating pointers after moving to evac page */
ArrayList f_table = {.size = 0};

/* Whenever a root is created it is inserted into this list */
ArrayList root_list;

void collect() {

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
