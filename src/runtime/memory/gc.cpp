
#include "allocator.h"
#include "gc.h"

//TODO: remove dependency on cstdlib -- use our own quicksort
#include <cstdlib>

// Used to determine if a pointer points into the data segment of an object
#define POINTS_TO_DATA_SEG(P) P >= (void*)PAGE_FIND_OBJ_BASE(P) && P < (void*)((char*)PAGE_FIND_OBJ_BASE(P) + PAGE_MASK_EXTRACT_PINFO(P)->entrysize)

// After we evacuate an object we need to update the original metadata
#define RESET_METADATA_FOR_OBJECT(meta) (meta) = { .type=nullptr, .isalloc=false, .isyoung=false, .ismarked=false, .isroot=false, .forward_indexMAX_FWD_INDEX, .ref_count=0 }

#define INC_REF_COUNT(O) (GC_REF_COUNT(O)++)
#define DEC_REF_COUNT(O) (GC_REF_COUNT(O)--)

int compare(const void* a, const void* b) 
{
    return ((char*)a - (char*)b);
}

void reprocessPageInfo(PageInfo* page) noexcept
{
    //This should not be called on pages that are (1) active allocators or evacuators or (2) pending collection pages

    //
    //TODO: we need to reprocess the page info here and get it in the correct list of pages
    //
}

void computeDeadRootsForDecrement(BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    //First we need to sort the roots we find
    qsort(tinfo.roots, tinfo.roots_count, sizeof(void*), compare);
    
    size_t roots_idx = 0;
    size_t oldroots_idx = 0;

    while(oldroots_idx < tinfo.old_roots_count) {
        void* cur_oldroot = tinfo.old_roots[oldroots_idx];
        
        if(roots_idx >= tinfo.roots_count) {
            //was dropped from roots
            tinfo.pending_decs.push_back(cur_oldroot);
            oldroots_idx++;
        }
        else {
            void* cur_root = tinfo.roots[roots_idx];

            if(cur_root < cur_oldroot) {
                //new root in current
                roots_idx++;
            } else if(cur_oldroot < cur_root) {
                //was dropped from roots
                tinfo.pending_decs.push_back(cur_oldroot);
                oldroots_idx++;
            } else {
                //in both lists
                roots_idx++;
                oldroots_idx++;
            }
        }
    }

    tinfo.old_roots_count = 0;
}

void processDecrements(BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    GC_REFCT_LOCK_ACQUIRE();

    size_t deccount = 0;
    while(!tinfo.pending_decs.isEmpty() && deccount < tinfo.max_decrement_count) {
        void* obj = tinfo.pending_decs.pop_front();
        deccount++;

        // Skip if the object is already freed
        if (!GC_IS_ALLOCATED(obj)) {
            continue;
        }

        // Decrement ref counts of objects this object points to
        const TypeInfoBase* type_info = GC_TYPE(obj);

        if(type_info->ptr_mask != LEAF_PTR_MASK) {
            for (size_t i = 0; i < type_info->slot_size; i++) {
                char mask = *((type_info->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* child = *(void**)((char*)obj + i * sizeof(void*));
                    if (child != NULL) {
                        DEC_REF_COUNT(child);

                        // If the child's ref count drops to zero, add it to the pending_decs list
                        if (GC_REF_COUNT(child) == 0) {
                            tinfo.pending_decs.push_back(child);
                        }
                    }
                }
            }
        }

        // Put object onto its pages freelist by masking to the page itself then pushing to front of list 
        PageInfo* objects_page = PageInfo::extractPageFromPointer(obj);
        FreeListEntry* entry = (FreeListEntry*)((uint8_t*)obj - sizeof(MetaData));
        entry->next = objects_page->freelist;
        objects_page->freelist = entry;

        // Mark the object as unallocated
        GC_IS_ALLOCATED(obj) = false;

        objects_page->freecount++;
        //
        //TODO: once we have heapified the lists we can compare the computed capcity with the capcity in the heap and (if we are over a threshold) and call reprocessPageInfo
        //
    }

    GC_REFCT_LOCK_RELEASE();

    //
    //TODO: we want to do a bit of PID controller here on the max decrement count to ensure that we eventually make it back to stable but keep pauses small
    //
}

//Starting from roots update pointers using forward table
void update_references(BSQMemoryTheadLocalInfo& tinfo) noexcept
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
                //This nesting is not ideal, but ok for now
                char mask = *((addr_type->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* ref = *(void**)((char*)addr + i * sizeof(void*)); //hmmm...

                    //If forward index is set, set old location to be non alloc and query forward table
                    uint32_t fwd_index = GC_FWD_INDEX(ref);
                    if(fwd_index != MAX_FWD_INDEX) {
                        GC_IS_ALLOCATED(ref) = false;
                        ((void**)addr)[i] = forward_table[fwd_index]; 

                        debug_print("update reference from %p to %p\n", ref, ((void**)addr)[i]);

                        //We need to explore these new evacuated pointers
                        worklist_push(worklist, ((void**)addr)[i]);
                    }
                }
            }
        }
    }
}

//Move non root young objects to evacuation page and update roots
void evacuate(BSQMemoryTheadLocalInfo& tinfo) noexcept 
{
    while(!tinfo.pending_young.isEmpty()) {
        void* obj = tinfo.pending_young.pop_front();
        PageInfo* page = PageInfo::extractPageFromPointer(obj);
        
        GC_INVARIANT_CHECK(GC_IS_YOUNG(obj) && GC_IS_MARKED(obj));

            //Check if our evac page doesnt exist yet or freelist is exhausted
            if (bin->evac_page == NULL || bin->evac_page->freelist == NULL) {
                getFreshPageForEvacuation(bin);
            } 
            
            FreeListEntry* base = bin->evac_page->freelist;
            bin->evac_page->freelist = base->next;

            set_canaries(base, bin->entrysize);
        
            void* new_addr = copy_object_data(old_addr, base, bin->entrysize);
            GC_IS_YOUNG(new_addr) = false; // When an object is evacuated, it is now old (tenured)
            bin->evac_page->freecount--;

            //Set objects old locations forward index to be found when updating references
            GC_IS_ALLOCATED(old_addr) = false;
            GC_FWD_INDEX(old_addr) = forward_table_index;
            forward_table[forward_table_index++] = new_addr;

            debug_print("Moved %p to %p\n", old_addr, new_addr);
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

        //Need to verify our object is allocated and not already marked
        if(GC_IS_ALLOCATED(addr) && !GC_IS_MARKED(addr) && canupdate) {
            GC_IS_MARKED(addr) = true;

            AllocatorBin* bin = getBinForSize( GC_TYPE(addr)->type_size );
            bin->roots[bin->roots_count++] = addr;

            //If it is not a leaf we will need to add to worklist
            if((GC_TYPE(addr)->ptr_mask != LEAF_PTR_MASK) && GC_IS_YOUNG(addr)) {
                worklist_push(*worklist, addr);
            }

            debug_print("Found a root at %p storing 0x%x\n", addr, *(int*)addr);
        }
    }
}

void walk_stack(struct WorkList* worklist) 
{
    void** cur_stack = native_stack_contents;
    int i = 0;

    void* addr;
    while((addr = cur_stack[i])) {
        //debug_print("Checking stack pointer %p\n", addr);
        check_potential_ptr(addr, worklist);
        i++;
    }

    #if 0

    //Funny pointer stuff to iterate through this struct, works since all elements are void*
    for (void** ptr = (void**)&native_register_contents; 
         ptr < (void**)((char*)&native_register_contents + sizeof(native_register_contents)); 
         ptr++) {
        void* register_contents = *ptr;
        debug_print("Checking register %p storing 0x%lx\n", ptr, (uintptr_t)register_contents);

        check_potential_ptr(register_contents, worklist);
    }
    
    #endif
    
    unloadNativeRootSet();
}

//Algorithm 2.2 from The Gargage Collection Handbook
void mark_and_evacuate()
{
    struct WorkList worklist;
    worklist_initialize(&worklist);

    walk_stack(&worklist);

    //Process the worklist in a BFS manner
    while (!worklist_is_empty(&worklist)) {
        void* parent_ptr = worklist_pop(void, worklist);
        struct TypeInfoBase* parent_type = GC_TYPE( parent_ptr );
        debug_print("parent pointer at %p\n", parent_ptr);
        
        if(parent_type->ptr_mask != LEAF_PTR_MASK) {
            for (size_t i = 0; i < parent_type->slot_size; i++) {
                //This nesting is not ideal, but ok for now
                char mask = *((parent_type->ptr_mask) + i);

                if (mask == PTR_MASK_PTR) {
                    void* child = *(void**)((char*)parent_ptr + i * sizeof(void*)); //hmmm...
                    debug_print("pointer slot points to %p\n", child);

                    //Valid child pointer, so mark and increment ref count then push to mark stack. Explore its pointers
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

    evacuate();
}


void collect() noexcept
{   
    //Before we mark and evac we populate old roots list, clearing roots list
    for(int i = 0; i < NUM_BINS; i++) {
        AllocatorBin* bin = getBinForSize(8 * (1 << i));

        for(size_t i = 0; i < bin->roots_count; i++) {
            //Reset marked bit for root to ensure discovery and promote to old space
            GC_IS_MARKED(bin->roots[i]) = false;
            GC_IS_YOUNG(bin->roots[i]) = false;

            bin->old_roots[bin->old_roots_count++] = bin->roots[i];

            debug_print("Insertion into old roots %p\n", bin->roots[i]);
            bin->roots[i] = NULL;
        }
        bin->roots_count = 0;
        qsort(bin->old_roots, bin->old_roots_count, sizeof(void*), compare);
    }
    
    mark_and_evacuate();

    for(int i = 0; i < NUM_BINS; i++) {
        AllocatorBin* bin = getBinForSize(8 * (1 << i));

        compare_roots_and_oldroots(bin);
        process_decs(bin);

        update_references(bin);
        rebuild_freelist(bin);        
    }
}
