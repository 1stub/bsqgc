#include "gc.h"
#include "allocator.h"

#include <stdlib.h> //qsort

void* forward_table[MAX_ROOTS];
size_t forward_table_index = 0;

void update_references(AllocatorBin* bin);
void rebuild_freelist(AllocatorBin* bin);
void compare_roots_and_oldroots(AllocatorBin* bin);
void process_decs(AllocatorBin* bin);

/* use in sorting old/new roots and using two pointer walk to find existence in old roots */
int compare(const void* a, const void* b) 
{
    return ((char*)a - (char*)b);
}

void collect() 
{
    /* Before we mark and evac we populate old roots list, clearing roots list */
    for(int i = 0; i < NUM_BINS; i++) {
        AllocatorBin* bin = getBinForSize(8 * (1 << i));

        for(size_t i = 0; i < bin->roots_count; i++) {
            bin->old_roots[bin->old_roots_count++] = bin->roots[i];
            debug_print("Insertion into old roots\n");
        }
        bin->roots_count = 0;
        qsort(bin->old_roots, bin->old_roots_count, sizeof(void*), compare);
    }

    mark_and_evacuate();

    /** Now we need to do our decs - 
    * If we discover an object who has ref count of zero, 
    * was in our old root set, ans is not in current roots
    * he is elligible for deletion 
    * (he would also be old but this check is trivial)
    **/

    for(int i = 0; i < NUM_BINS; i++) {
        AllocatorBin* bin = getBinForSize(8 * (1 << i));

        compare_roots_and_oldroots(bin);
        process_decs(bin);

        update_references(bin);
        rebuild_freelist(bin);        
    }
}

/**
* This method is designed to walk the roots and oldroots set for each bin,
* finding those who need decs
**/
void compare_roots_and_oldroots(AllocatorBin* bin) 
{
    /* First we need to sort the roots we find */
    qsort(bin->roots, bin->roots_count, sizeof(void*), compare);
    
    size_t roots_idx = 0;
    size_t oldroots_idx = 0;

    while(oldroots_idx < bin->old_roots_count) {
        char* cur_oldroot = bin->old_roots[oldroots_idx];
        char* cur_root = bin->roots[roots_idx];
        if(cur_root < cur_oldroot) {
            roots_idx++;
        } else if(cur_oldroot < cur_root) {
            worklist_push(bin->pending_decs, bin->old_roots[oldroots_idx]);
            debug_print("old root %p not in current roots (current at %p)\n", cur_oldroot, cur_root);
            oldroots_idx++;
        } else {
            roots_idx++;
            oldroots_idx++;
        }
    }
    bin->old_roots_count = 0;
}

void process_decs(AllocatorBin* bin) 
{
    while(!worklist_is_empty(&bin->pending_decs)) {
        void* obj = worklist_pop(void, bin->pending_decs);

        // Skip if the object is already freed
        if (!GC_IS_ALLOCATED(obj)) {
            debug_print("object a %p has already been freed\n", obj);
            continue;
        }

        // Decrement ref counts of objects this object points to
        struct TypeInfoBase* type_info = GC_TYPE(obj);

        if(type_info->ptr_mask != LEAF_PTR_MASK) {
            for (size_t i = 0; i < type_info->slot_size; i++) {
                char mask = *((type_info->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* child = *(void**)((char*)obj + i * sizeof(void*));
                    if (child != NULL) {
                        decrement_ref_count(child);

                        // If the child's ref count drops to zero, add it to the pending_decs list
                        if (GC_REF_COUNT(child) == 0) {
                            worklist_push(bin->pending_decs, child);
                        }
                    }
                }
            }
        }

        // Put object onto its pages freelist by masking to the page itself then pusing to front of list 
        PageInfo* objects_page = PAGE_MASK_EXTRACT_PINFO(obj);
        FreeListEntry* entry = (FreeListEntry*)((char*)obj - sizeof(MetaData));
        entry->next = objects_page->freelist;
        objects_page->freelist = entry;

        // Mark the object as unallocated
        GC_IS_ALLOCATED(obj) = false;
        debug_print("Freed object at %p\n", obj);
    }
}

/* Set pre and post canaries in evacuation page if enabled */
#ifdef ALLOC_DEBUG_CANARY
static void set_canaries(void* base, size_t entry_size) 
{
    uint64_t* pre = (uint64_t*)base;
    *pre = ALLOC_DEBUG_CANARY_VALUE;

    uint64_t* post = (uint64_t*)((char*)base + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);
    *post = ALLOC_DEBUG_CANARY_VALUE;
}
#endif

/* Actual moving of pointers over to evacuation page */
static void* copy_object_data(void* old_addr, void* new_base, size_t entry_size) 
{
    void* new_addr;

#ifdef ALLOC_DEBUG_CANARY
    // If canaries are enabled, skip the pre-canary and copy the object data + metadata
    new_addr = (char*)new_base + ALLOC_DEBUG_CANARY_SIZE;
    memcpy(new_addr, (char*)old_addr - sizeof(MetaData), entry_size + sizeof(MetaData));
#else
    // If canaries are not enabled, copy the object data + metadata directly
    new_addr = new_base;
    memcpy(new_addr, (char*)old_addr - sizeof(MetaData), entry_size + sizeof(MetaData));
#endif

    return new_addr;
}

/* Starting from roots update pointers using forward table */
void update_references(AllocatorBin* bin) 
{
    struct WorkList worklist;
    worklist_initialize(&worklist);

    for(size_t i = 0; i < bin->roots_count; i++) {
        worklist_push(worklist, bin->roots[i]);
    }

    while(!worklist_is_empty(&worklist)) {
        void* addr = worklist_pop(void, worklist);
        struct TypeInfoBase* addr_type = GC_TYPE( addr );

        if(addr_type->ptr_mask != LEAF_PTR_MASK) {

            for (size_t i = 0; i < addr_type->slot_size; i++) {
                /* This nesting is not ideal, but ok for now */
                char mask = *((addr_type->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* ref = *(void**)((char*)addr + i * sizeof(void*)); //hmmm...

                    /* Need to update pointers from this old reference now, put on worklist */
                    worklist_push(worklist, ref);

                    /* If forward index is set, set old location to be non alloc and query forward table */
                    uint32_t fwd_index = GC_FWD_INDEX(ref);
                    if(fwd_index != MAX_FWD_INDEX) {
                        debug_print("old ref %p\n", ref);
                        GC_IS_ALLOCATED(ref) = false;
                        ((void**)addr)[i] = forward_table[fwd_index]; //need to be careful with this void** cast
                        debug_print("update reference to %p\n", ref);
                    }
                }
            }
        }
    }
}

/**
* Idea here is after we finish collecting and all freelists that got manipulated a bunch
* have been rebuilt we can return our pages to their managers respectively, going to their
* appropriate utilization lists
**/
void return_to_pmanagers(AllocatorBin* bin) 
{
    PageInfo* page = bin->alloc_page;
    float page_utilization = 1.0f - ((float)page->freecount / page->entrycount);

    /* TODO: make these page insertions nice macros */
    if(page_utilization < 0.01) {
        INSERT_PAGE_IN_LIST(bin->page_manager->empty_pages, page);
    } else if(page_utilization > 0.01 && page_utilization < 0.3) {
        INSERT_PAGE_IN_LIST(bin->page_manager->low_utilization_pages, page);
    } else if(page_utilization > 0.3 && page_utilization < 0.85) {
        INSERT_PAGE_IN_LIST(bin->page_manager->mid_utilization_pages, page);
    } else if(page_utilization > 0.85 && page_utilization < 1.0f) {
        INSERT_PAGE_IN_LIST(bin->page_manager->high_utilization_pages, page);
    } else {
        INSERT_PAGE_IN_LIST(bin->page_manager->filled_pages, page);
    }

    /* Need to update bins alloc page now, since it just got returned */
    bin->alloc_page = bin->alloc_page->next;
    if(bin->alloc_page == NULL) {
        bin->freelist = NULL;
    } else {
        bin->freelist = bin->alloc_page->freelist;
    }
}

/**
* When we find an object that is eligble to be freed, we need to traverse what is points to
* and decrement their refcount. This doesn't happen currently.
**/
void rebuild_freelist(AllocatorBin* bin)
{
    PageInfo* cur = bin->alloc_page;

    while(cur) {
        FreeListEntry* last_freelist_entry = NULL;
        bool first_nonalloc_block = true;
        cur->freecount = 0;
    
        for (size_t i = 0; i < cur->entrycount; i++) {
            FreeListEntry* new_freelist_entry = FREE_LIST_ENTRY_AT(cur, i);
            void* obj = OBJ_START_FROM_BLOCK(new_freelist_entry); 
    
            /* Add non allocated OR old non roots with a ref count of 0 OR not marked, meaning unreachable */
            if (!GC_IS_ALLOCATED(obj) || (!GC_IS_YOUNG(obj) && GC_REF_COUNT(obj) == 0) || !GC_IS_MARKED(obj)) {
                if(first_nonalloc_block) {
                    cur->freelist = new_freelist_entry;
                    cur->freelist->next = NULL;
                    last_freelist_entry = cur->freelist;
                    first_nonalloc_block = false;
                } else {
                    last_freelist_entry->next = new_freelist_entry;
                    new_freelist_entry->next = NULL; 
                    last_freelist_entry = new_freelist_entry;
                }
                
                cur->freecount++;
            }
        }

        debug_print("[DEBUG] Freelist %p rebuild. Page contains %i allocated blocks.\n", cur->freelist, cur->entrycount - cur->freecount);

        /* Not checking canaries currently, good to have though if weird bugs pop up */
        //verifyCanariesInPage(cur);

        /* return cur page to its bins page manager */
        return_to_pmanagers(bin);

        cur = cur->next;
    }
}

/* Move non root young objects to evacuation page then update roots */
void evacuate() 
{
    while(!stack_empty(marking_stack)) {
        void* old_addr = stack_pop(void, marking_stack);
        AllocatorBin* bin = getBinForSize( GC_TYPE(old_addr)->type_size );

        /* Need to evacuate young marked objects */
        if(GC_IS_YOUNG(old_addr) && GC_IS_MARKED(old_addr)) {
            /* Check if our evac page doesnt exist yet or freelist is exhausted */
            if (bin->evac_page == NULL) {
                getFreshPageForEvacuation(bin);
            } else if(bin->evac_page->freelist == NULL) {
                getFreshPageForEvacuation(bin);
            }
            
            FreeListEntry* base = bin->evac_page->freelist;
            bin->evac_page->freelist = base->next;

            set_canaries(base, bin->entrysize);
        
            void* new_addr = copy_object_data(old_addr, base, bin->entrysize);
            GC_IS_YOUNG(new_addr) = false; // When an object is evacuated, it is now old (tenured)
            bin->evac_page->freecount--;

            /* Set objects old locations forward index to be found when updating references */
            MetaData* old_addr_meta = GC_GET_META_DATA_ADDR(old_addr);
            GC_IS_ALLOCATED(old_addr) = false;
            old_addr_meta->forward_index = forward_table_index;
            forward_table[forward_table_index++] = new_addr;

            debug_print("Moved %p to %p\n", old_addr, new_addr);
        }
    }
}

void check_potential_ptr(void* addr, struct WorkList* worklist) 
{
    bool canupdate = true;
    if(pagetable_query(addr)) {
        if (!(PAGE_IS_OBJ_ALIGNED(addr))) {
            if(POINTS_TO_DATA_SEG(addr)){
                debug_print("address %p was not aligned but pointed into data seg\n", addr);
                addr = PAGE_FIND_OBJ_BASE(addr);
                canupdate = true;
            } else {
                debug_print("found pointer into alloc page that did not point into data seg at %p\n", addr);
                canupdate = false;
            }
        }

        /* Need to verify our object is allocated and not already marked */
        if(GC_IS_ALLOCATED(addr) && !GC_IS_MARKED(addr) && canupdate) {
            GC_IS_MARKED(addr) = true;

            AllocatorBin* bin = getBinForSize( GC_TYPE(addr)->type_size );
            bin->roots[bin->roots_count++] = addr;

            /* If it is not a leaf we will need to add to worklist */
            if((GC_TYPE(addr)->ptr_mask != LEAF_PTR_MASK) && GC_IS_YOUNG(addr)) {
                worklist_push(*worklist, addr);
            }

            debug_print("Found a root at %p storing 0x%x\n", addr, *(int*)addr);
        }
    }
}

void walk_stack(struct WorkList* worklist) 
{
    loadNativeRootSet();

    void** cur_stack = native_stack_contents;
    int i = 0;

    void* addr;
    while((addr = cur_stack[i])) {
        //debug_print("Checking stack pointer %p\n", addr);
        check_potential_ptr(addr, worklist);
        i++;
    }

    /* Funny pointer stuff to iterate through this struct, works since all elements are void* */
    for (void** ptr = (void**)&native_register_contents; 
         ptr < (void**)((char*)&native_register_contents + sizeof(native_register_contents)); 
         ptr++) {
        void* register_contents = *ptr;
        //debug_print("Checking register %p storing 0x%lx\n", ptr, (uintptr_t)register_contents);

        check_potential_ptr(register_contents, worklist);
    }


    unloadNativeRootSet();
}

/* Algorithm 2.2 from The Gargage Collection Handbook */
void mark_and_evacuate()
{
    struct WorkList worklist;
    worklist_initialize(&worklist);

    walk_stack(&worklist);

    /* Process the worklist in a BFS manner */
    while (!worklist_is_empty(&worklist)) {
        void* parent_ptr = worklist_pop(void, worklist);
        struct TypeInfoBase* parent_type = GC_TYPE( parent_ptr );
        debug_print("parent pointer at %p\n", parent_ptr);
        
        if(parent_type->ptr_mask != LEAF_PTR_MASK) {
            for (size_t i = 0; i < parent_type->slot_size; i++) {
                /* This nesting is not ideal, but ok for now */
                char mask = *((parent_type->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* child = *(void**)((char*)parent_ptr + i * sizeof(void*)); //hmmm...
                    debug_print("pointer slot points to %p\n", child);

                    /** 
                    * I do wonder how exactly roots pointing to roots are handled here? is this even possible?
                    * it happens (atleast as of 02/25/2025) when running tests even on simple graphs. 
                    * The last allocated object appears on the calling stack which then attemps to be explored
                    * and since it is pointed to BY a root we would need to do ref counts and such, but its already
                    * marked when we are checking for valid root refs in check potential pointer.
                    * 
                    * Quite funky... I suspect the problem is related to HOW i am testing, lesser so the current 
                    * impl but this may just be me being optimistic...
                    **/

                    /* Valid child pointer, so mark and increment ref count then push to mark stack. Explore its pointers */
                    if(!GC_IS_MARKED(child)) {
                        increment_ref_count(child);
                        GC_IS_MARKED(child) = true;
                        worklist_push(worklist, child);
                        stack_push(void, marking_stack, child);
                    }
                }
            }
        }
    }

    /* This clears the marking stack which we dont want, but can be used as a sanity check */
    #if 0
    while(!stack_empty(marking_stack)) {
        debug_print("marked node %p\n", stack_pop(void, marking_stack));
    }
    #endif

    evacuate();
}