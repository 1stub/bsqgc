#include "gc.h"
#include "allocator.h"

/** 
* Not totally confident on determining size or proper data structures for these two.
* It's looking like this is where we need quick sort for sortring based on addreses
* to avoid excessive and slow lookups
**/
#define MAX_PTRS 1024
void* forward_table[MAX_PTRS];
int forward_table_index = 0;

/* This needs to be a actual SET!!!! */
void* roots_set[MAX_PTRS];
int roots_set_index = 0;

void collect() 
{
    mark_and_evacuate();
}

static void update_evacuation_freelist(AllocatorBin *bin) {
    if (bin->page_manager->evacuate_page->freelist == NULL) {
        bin->page_manager->evacuate_page->next = allocateFreshPage(bin->page_manager->evacuate_page->entrysize);
        bin->page_manager->evacuate_page = bin->page_manager->evacuate_page->next;
        bin->page_manager->evacuate_page->next = NULL;
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
void update_references() 
{
    struct WorkList worklist;
    worklist_initialize(&worklist);

    for(int i = 0; i < roots_set_index; i++) {
        void* root = roots_set[i];
        worklist_push(worklist, root);
    }

    while(!worklist_is_empty(&worklist)) {
        void* addr = worklist_pop(void, worklist);
        struct TypeInfoBase* addr_type = GC_TYPE( addr );

        for (size_t i = 0; i < addr_type->slot_size; i++) {
            char mask = *(addr_type->ptr_mask) + i;

            if(mask == PTR_MASK_NOP) {
                // Nothing to do, not a pointer
            } 
            else if (mask == PTR_MASK_PTR) {
                void* ref = *(void**)((char*)addr + i * sizeof(void*)); //hmmm...

                /* Need to update pointers from this old reference now, put on worklist */
                worklist_push(worklist, ref);

                /* If forward index is set, set old location to be non alloc and query forward table */
                uint32_t fwd_index = GC_FWD_INDEX(ref);
                if(fwd_index != MAX_FWD_INDEX) {
                    debug_print("old ref %p\n", ref);
                    GC_IS_ALLOCATED(ref) = false;
                    ref = forward_table[fwd_index];
                    debug_print("update reference to %p\n", ref);
                }
            } 
            else {
                // Do nothing
            }
        }
    }
}

void rebuild_freelist()
{
    /* Very naive approach to looping through our bins */
    struct Stack bins =  {NULL, NULL, NULL, NULL};
    stack_push(AllocatorBin, bins, getBinForSize(8));
    stack_push(AllocatorBin, bins, getBinForSize(16));

    while(!stack_empty(bins)) {
        AllocatorBin* bin = stack_pop(AllocatorBin, bins);
        PageInfo* cur = bin->page_manager->all_pages;

        while(cur) {
            FreeListEntry* last_freelist_entry = NULL;
            bool first_nonalloc_block = true;
            cur->freecount = 0;
        
            for (size_t i = 0; i < cur->entrycount; i++) {
                FreeListEntry* new_freelist_entry = FREE_LIST_ENTRY_AT(cur, i);
                void* obj = OBJ_START_FROM_BLOCK(new_freelist_entry); 
        
                /* Add non allocated OR old non roots with a ref count of 0 */
                if (!GC_IS_ALLOCATED(obj) || (!GC_IS_YOUNG(obj) && GC_REF_COUNT(obj) == 0 && !GC_IS_ROOT(obj))) {
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

            /**
            * Currently I have no idea why I need to manually set the bins page here,
            * I feel like manually doing this setting could create some really weird
            * problems when running larger tests. It seems that modifying the current 
            * page in bin through our page manager does not properly reflect in
            * bin->page/freelist itself. hmm...
            **/

            /* Now update the current page for bin if there are freeblocks */
            if(cur->freecount > 0) {
                bin->page = cur;
                bin->freelist = cur->freelist;
            }

            debug_print("[DEBUG] Freelist %p rebuild. Page contains %i allocated blocks.\n", cur->freelist, cur->entrycount - cur->freecount);

            cur = cur->next;
        }

    }
}

/* Move non root young objects to evacuation page then update roots */
void evacuate() 
{
    while(!stack_empty(marking_stack)) {
        void* old_addr = stack_pop(void, marking_stack);
        AllocatorBin* bin = getBinForSize( GC_TYPE(old_addr)->type_size );

        /* We evacuate non root young marked objects */
        if(!GC_IS_ROOT(old_addr) && GC_IS_YOUNG(old_addr) && GC_IS_MARKED(old_addr)) {
            // Check if the current evacuation page's freelist is exhausted
            if (bin->page_manager->evacuate_page->freelist == NULL) {
                update_evacuation_freelist(bin);
            }
            
            FreeListEntry* base = bin->page_manager->evacuate_page->freelist;
            bin->page_manager->evacuate_page->freelist = base->next;

            set_canaries(base, bin->entrysize);
        
            void* new_addr = copy_object_data(old_addr, base, bin->entrysize);

            /* Set objects old locations forward index to be found when updating references */
            MetaData* old_addr_meta = GC_GET_META_DATA_ADDR(old_addr);
            old_addr_meta->forward_index = forward_table_index;
            forward_table[forward_table_index++] = new_addr;

            debug_print("Moved %p to %p\n", old_addr, new_addr);
        }
    }

    update_references();
    rebuild_freelist();
}

void walk_stack(struct Stack* marked_nodes, struct WorkList* worklist) 
{
    loadNativeRootSet();

    void** cur_stack = native_stack_contents;
    int i = 0;

    while(cur_stack[i]) {
        bool canupdate = true;
        void* addr = cur_stack[i];
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

            /* Actual marking logic */
            if(GC_IS_ALLOCATED(addr) && (GC_TYPE(addr)->ptr_mask != LEAF_PTR_MASK) && canupdate) {
                if(GC_REF_COUNT(addr) > 0) continue;

                if(!GC_IS_YOUNG(addr)) {
                    /* Need some way to handle old roots */
                    i++;
                    continue;
                }

                /* If we have a potential pointer with no references and its not marked, set mark bit and set as root */
                if (GC_REF_COUNT(addr) == 0 && !GC_IS_MARKED(addr)) {
                    GC_IS_MARKED(addr) = true;
                    GC_IS_ROOT(addr) = true;
                    worklist_push(*worklist, addr);
                    stack_push(void, *marked_nodes, addr);
                    roots_set[roots_set_index++] = addr;
                    debug_print("Found a root at %p storing 0x%x\n", addr, *(int*)addr);
                }

            }
        }

        i++;
    }

    /* This will need to actually modify our marked nodes stack */
    pagetable_query(native_register_contents.rax);
    pagetable_query(native_register_contents.rbx);
    pagetable_query(native_register_contents.rcx);
    pagetable_query(native_register_contents.rdx);
    pagetable_query(native_register_contents.rsi);
    pagetable_query(native_register_contents.rdi);
    pagetable_query(native_register_contents.r8);
    pagetable_query(native_register_contents.r9);
    pagetable_query(native_register_contents.r10);
    pagetable_query(native_register_contents.r11);
    pagetable_query(native_register_contents.r12);
    pagetable_query(native_register_contents.r13);
    pagetable_query(native_register_contents.r14);
    pagetable_query(native_register_contents.r15);

    unloadNativeRootSet();
}

/* Algorithm 2.2 from The Gargage Collection Handbook */
void mark_and_evacuate()
{
    struct WorkList worklist;
    worklist_initialize(&worklist);

    walk_stack(&marking_stack, &worklist);

    /* Process the worklist in a BFS manner */
    while (!worklist_is_empty(&worklist)) {
        void* parent_ptr = worklist_pop(void, worklist);
        struct TypeInfoBase* parent_type = GC_TYPE( parent_ptr );
        debug_print("parent pointer at %p\n", parent_ptr);
        
        for (size_t i = 0; i < parent_type->slot_size; i++) {
            char mask = *(parent_type->ptr_mask) + i;

            if(mask == PTR_MASK_NOP) {
                // Nothing to do, not a pointer
            } 
            else if (mask == PTR_MASK_PTR) {
                void* child = *(void**)((char*)parent_ptr + i * sizeof(void*)); //hmmm...
                debug_print("pointer slot points to %p\n", child);

                /* Valid child pointer, so mark and increment ref count then push to mark stack. Explore its pointers */
                if(child != NULL && !GC_IS_MARKED(child)) {
                    increment_ref_count(child);
                    GC_IS_MARKED(child) = true;
                    worklist_push(worklist, child);
                    stack_push(void, marking_stack, child);
                }
            } 
            else {
                // Do nothing
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