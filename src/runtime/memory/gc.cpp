
#include "allocator.h"
#include "gc.h"

//TODO: remove dependency on cstdlib -- use our own quicksort
#include <cstdlib>

thread_local GCAllocator* g_gcallocs[BSQ_MAX_ALLOC_SLOTS];

// Used to determine if a pointer points into the data segment of an object
#define POINTS_TO_DATA_SEG(P) P >= (void*)PAGE_FIND_OBJ_BASE(P) && P < (void*)((char*)PAGE_FIND_OBJ_BASE(P) + PAGE_MASK_EXTRACT_PINFO(P)->entrysize)

// After we evacuate an object we need to update the original metadata
#define RESET_METADATA_FOR_OBJECT(meta) (meta) = { .type=nullptr, .isalloc=false, .isyoung=false, .ismarked=false, .isroot=false, .forward_indexMAX_FWD_INDEX, .ref_count=0 }

#define INC_REF_COUNT(O) (++GC_REF_COUNT(O))
#define DEC_REF_COUNT(O) (--GC_REF_COUNT(O))

int compare(const void* a, const void* b) 
{
    return ((char*)a - (char*)b);
}

GCAllocator* getAllocatorForPageSize(PageInfo* page) noexcept {
    return g_gcallocs[page->allocsize >> 3];
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
        void* obj = (void**)tinfo.pending_decs.pop_front();
        deccount++;

        // Skip if the object is already freed
        if (!GC_IS_ALLOCATED(obj)) {
            continue;
        }

        // Decrement ref counts of objects this object points to
        const TypeInfoBase* type_info = GC_TYPE(obj);

        if(type_info->ptr_mask != LEAF_PTR_MASK) {
            const char* ptr_mask = type_info->ptr_mask;
            void** slots = (void**)obj;
            while(*ptr_mask != '\0') {
                char mask = *(ptr_mask++);

                if (mask == PTR_MASK_PTR) {    
                    if(DEC_REF_COUNT(*slots) == 0) {
                        tinfo.pending_decs.push_back(*slots);
                    }
                }

                slots++;
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

//Update pointers using forward table
void updatePointers(void** obj, const BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    GC_INVARIANT_CHECK(!GC_IS_YOUNG(obj) && !GC_IS_MARKED(obj));
    TypeInfoBase* type_info = GC_TYPE(obj);

    if(type_info->ptr_mask != LEAF_PTR_MASK) {
        const char* ptr_mask = type_info->ptr_mask;
        void** slots = (void**)obj;

        while(*ptr_mask != '\0') {
            char mask = *(ptr_mask++);

            if (mask == PTR_MASK_PTR) {
                uint32_t fwd_index = GC_FWD_INDEX(*slots);
                if(fwd_index != MAX_FWD_INDEX) {
                    *slots = tinfo.forward_table[fwd_index]; 
                }

                INC_REF_COUNT(*slots);
            }

            slots++;
        }
    }
}

//Move non root young objects to evacuation page (as needed) then forward pointers and inc ref counts
void processMarkedYoungObjects(BSQMemoryTheadLocalInfo& tinfo) noexcept 
{
    GC_REFCT_LOCK_ACQUIRE();

    while(!tinfo.pending_young.isEmpty()) {
        void* obj = tinfo.pending_young.pop_front();
        TypeInfoBase* type_info = GC_TYPE(obj);
        GC_INVARIANT_CHECK(GC_IS_YOUNG(obj) && GC_IS_MARKED(obj));

        if(GC_IS_ROOT(obj)) {
            updatePointers((void**)obj, tinfo);
            GC_CLEAR_YOUNG_MARK(GC_GET_META_DATA_ADDR(obj));
        }
        else {
            //If we are not a root we want to evacuate
            GCAllocator* gcalloc = getAllocatorForPageSize(PageInfo::extractPageFromPointer(obj));
            GC_INVARIANT_CHECK(gcalloc != nullptr);
        
            void* newobj = gcalloc->allocateEvacuation(type_info);
            xmem_copy(obj, newobj, type_info->slot_size);
            updatePointers((void**)newobj, tinfo);

            tinfo.forward_table[tinfo.forward_table_index++] = newobj;
        }
    }

    GC_REFCT_LOCK_RELEASE();
}

void checkPotentialPtr(void* addr, BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    if(GlobalPageGCManager::g_gc_page_manager.pagetable_query(addr)) {
        MetaData* meta = PageInfo::getObjectMetadataAligned(addr);
        
        //Need to verify our object is allocated and not already marked
        if(GC_SHOULD_PROCESS_AS_ROOT(meta)) {
            GC_MARK_AS_ROOT(meta);

            tinfo.roots[tinfo.roots_count++] = addr;
            if(GC_IS_YOUNG(addr)) {
                tinfo.pending_visit.push_back(addr);
            }
        }
    }
}

void walkStack(BSQMemoryTheadLocalInfo& tinfo) noexcept 
{
    tinfo.loadNativeRootSet();

    for(size_t i = 0; i < tinfo.native_stack_count; i++) {
        checkPotentialPtr(tinfo.native_stack_contents[i], tinfo);
    }

    checkPotentialPtr(tinfo.native_register_contents.rax, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.rbx, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.rcx, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.rdx, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.rsi, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.rdi, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r8, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r9, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r10, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r11, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r12, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r13, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r14, tinfo);
    checkPotentialPtr(tinfo.native_register_contents.r15, tinfo);

    tinfo.unloadNativeRootSet();
}

void markAndEvacuate(BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    walkStack(tinfo);

    //Process the walk stack
    while(!tinfo.marking_stack.isEmpty()) {
        void* obj = tinfo.marking_stack.pop_back();

        TypeInfoBase* obj_type = GC_TYPE(obj);
        if(obj_type->ptr_mask != LEAF_PTR_MASK) {
            const char* ptr_mask = obj_type->ptr_mask;
            void** slots = (void**)obj;

            while(*ptr_mask != '\0') {
                char mask = *(ptr_mask++);

                if (mask == PTR_MASK_PTR) {
                    MetaData* meta = GC_GET_META_DATA_ADDR(*slots);
                    if(GC_SHOULD_VISIT(meta)) {
                        GC_MARK_AS_MARKED(meta);
                        tinfo.marking_stack.push_back(*slots);
                    }
                }
            }

            slots++;
        }

        tinfo.pending_young.push_back(obj);
    }

    processMarkedYoungObjects(tinfo);
}


void initializeGC(GCAllocator* allocs...) noexcept
{
    xxxx;
    //set gc allocs array and collect fp
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
