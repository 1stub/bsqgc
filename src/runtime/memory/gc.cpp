
#include "allocator.h"
#include "gc.h"
#include "../support/qsort.h"
#include "threadinfo.h"

// Used to determine if a pointer points into the data segment of an object
#define POINTS_TO_DATA_SEG(P) P >= (void*)PAGE_FIND_OBJ_BASE(P) && P < (void*)((char*)PAGE_FIND_OBJ_BASE(P) + PAGE_MASK_EXTRACT_PINFO(P)->entrysize)

// After we evacuate an object we need to update the original metadata
#define RESET_METADATA_FOR_OBJECT(M, FP) *M = { .type=nullptr, .isalloc=false, .isyoung=false, .ismarked=false, .isroot=false, .forward_index=(FP), .ref_count=0 }

#define INC_REF_COUNT(O) (++GC_REF_COUNT(O))
#define DEC_REF_COUNT(O) (--GC_REF_COUNT(O))

void reprocessPageInfo(PageInfo* page, BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    //This should not be called on pages that are (1) active allocators or evacuators or (2) pending collection pages

    //
    //TODO: we need to reprocess the page info here and get it in the correct list of pages
    //

}

void computeDeadRootsForDecrement(BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    //First we need to sort the roots we find
    qsort(tinfo.roots, 0, tinfo.roots_count - 1, tinfo.roots_count);
    
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

                if(*slots != nullptr) {
                    if(mask == PTR_MASK_PTR && DEC_REF_COUNT(*slots) == 0) {
                        tinfo.pending_decs.push_back(*slots);
                    }
                    if(PTR_MASK_STRING_AND_SLOT_PTR_VALUED(mask, *slots) && DEC_REF_COUNT(*slots) == 0) {
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

        //
        //Need to think more about what is actually necessary here - 
        //for some reason my brain just cant quite figure out what
        //exactly i need to do. it appears removing from old location
        //in one of our bst buckets, but idrk. also i discovered
        //that we never actually insert a page into the global page
        //list which is not good. something object could be keeing 
        //everything alive?
        //

        //this is HORRIBY innefficient
        //TODO: FIX THIS PLZ!!!! And do the stuff i wrote above prob
        objects_page->rebuild();
    }

    GC_REFCT_LOCK_RELEASE();

    //
    //TODO: we want to do a bit of PID controller here on the max decrement count to ensure that we eventually make it back to stable but keep pauses small
    //
}

//Update pointers using forward table
void updatePointers(void** obj, const BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    TypeInfoBase* type_info = GC_TYPE(obj);

    if(type_info->ptr_mask != LEAF_PTR_MASK) {
        const char* ptr_mask = type_info->ptr_mask;
        void** slots = (void**)obj;

        while(*ptr_mask != '\0') {
            char mask = *(ptr_mask++);

            if(*slots != nullptr) {
                if((mask == PTR_MASK_PTR) | PTR_MASK_STRING_AND_SLOT_PTR_VALUED(mask, *slots)) {
                    uint32_t fwd_index = GC_FWD_INDEX(*slots);
                    if(fwd_index != MAX_FWD_INDEX) {
                        *slots = tinfo.forward_table[fwd_index]; 
                    }
                    INC_REF_COUNT(*slots);
                }
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
            GCAllocator* gcalloc = tinfo.getAllocatorForPageSize(PageInfo::extractPageFromPointer(obj));
            GC_INVARIANT_CHECK(gcalloc != nullptr);
        
            void* newobj = gcalloc->allocateEvacuation(type_info);
            xmem_copy(obj, newobj, type_info->slot_size);
            updatePointers((void**)newobj, tinfo);

            RESET_METADATA_FOR_OBJECT(GC_GET_META_DATA_ADDR(obj), (uint32_t)tinfo.forward_table_index);
            tinfo.forward_table[tinfo.forward_table_index++] = newobj;
        }
    }

    GC_REFCT_LOCK_RELEASE();
}

void checkPotentialPtr(void* addr, BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    if(GlobalPageGCManager::g_gc_page_manager.pagetable_query(addr)
        && ((uintptr_t)addr & 0xFFF) != 0) {
        MetaData* meta = PageInfo::getObjectMetadataAligned(addr);
        void* obj = (void*)((uint8_t*)meta + sizeof(MetaData));
        
        //Need to verify our object is allocated and not already marked
        if(GC_SHOULD_PROCESS_AS_ROOT(meta)) {
            GC_MARK_AS_ROOT(meta);

            tinfo.roots[tinfo.roots_count++] = obj;
            if(GC_SHOULD_PROCESS_AS_YOUNG(meta)) {
                tinfo.pending_roots.push_back(obj);
            }
        }
    }
}

void walkStack(BSQMemoryTheadLocalInfo& tinfo) noexcept 
{
    //Process global data (TODO -- later have flag to disable this after it is fixed as immortal)
    if(GlobalDataStorage::g_global_data.native_global_storage != nullptr) {
        void** curr = GlobalDataStorage::g_global_data.native_global_storage;
        while(curr < GlobalDataStorage::g_global_data.native_global_storage_end) {
            checkPotentialPtr(*curr, tinfo);
            curr++;
        }
    }

#ifdef BSQ_GC_CHECK_ENABLED
    if(tinfo.disable_stack_refs_for_tests) {
        return;
    }
#endif
    
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

void walkSingleRoot(void* root, BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    while(!tinfo.visit_stack.isEmpty()) {
        MarkStackEntry entry = tinfo.visit_stack.pop_back();
        TypeInfoBase* obj_type = GC_TYPE(entry.obj);

        if((obj_type->ptr_mask == LEAF_PTR_MASK) | (entry.color == MARK_STACK_NODE_COLOR_BLACK)) {
            //no children so do by definition
            tinfo.pending_young.push_back(entry.obj);
        }
        else {
            tinfo.visit_stack.push_back({entry.obj, MARK_STACK_NODE_COLOR_BLACK});

            const char* ptr_mask = obj_type->ptr_mask;
            void** slots = (void**)entry.obj;

            while(*ptr_mask != '\0') {
                char mask = *(ptr_mask++);

                if(*slots != nullptr) {
                    if ((mask == PTR_MASK_PTR) | PTR_MASK_STRING_AND_SLOT_PTR_VALUED(mask, *slots)) {
                        MetaData* meta = GC_GET_META_DATA_ADDR(*slots);
                        if(GC_SHOULD_VISIT(meta)) {
                            GC_MARK_AS_MARKED(meta);
                            tinfo.visit_stack.push_back({*slots, MARK_STACK_NODE_COLOR_GREY});
                        }
                    }
                }

                slots++;
            }
        }
    }
}

void markingWalk(BSQMemoryTheadLocalInfo& tinfo) noexcept
{
    gtl_info.pending_roots.initialize();
    gtl_info.visit_stack.initialize();

    walkStack(tinfo);

    //Process the walk stack
    while(!tinfo.pending_roots.isEmpty()) {
        void* obj = tinfo.pending_roots.pop_front();
        MetaData* meta = GC_GET_META_DATA_ADDR(obj);
        if(GC_SHOULD_VISIT(meta)) {
            GC_MARK_AS_MARKED(meta);

            tinfo.visit_stack.push_back({obj, MARK_STACK_NODE_COLOR_GREY});
            walkSingleRoot(obj, tinfo);
        }
    }

    gtl_info.visit_stack.clear();
    gtl_info.pending_roots.clear();
}

void collect() noexcept
{   
    gtl_info.pending_young.initialize();
    markingWalk(gtl_info);
    processMarkedYoungObjects(gtl_info);
    gtl_info.pending_young.clear();

    xmem_zerofill(gtl_info.forward_table, gtl_info.forward_table_index);
    gtl_info.forward_table_index = 0;

    gtl_info.pending_decs.initialize();
    computeDeadRootsForDecrement(gtl_info);
    processDecrements(gtl_info);
    gtl_info.pending_decs.clear();

    for(size_t i = 0; i < BSQ_MAX_ALLOC_SLOTS; i++) {
        GCAllocator* alloc = gtl_info.g_gcallocs[i];
        if(alloc != nullptr) {
            alloc->processCollectorPages();
        }
    }

    xmem_zerofill(gtl_info.old_roots, gtl_info.old_roots_count);
    gtl_info.old_roots_count = 0;

    for(size_t i = 0; i < gtl_info.roots_count; i++) {
        GC_CLEAR_ROOT_MARK(GC_GET_META_DATA_ADDR(gtl_info.roots[i]));

        gtl_info.old_roots[gtl_info.old_roots_count++] = gtl_info.roots[i];
    }

    xmem_zerofill(gtl_info.roots, gtl_info.roots_count);
    gtl_info.roots_count = 0;
}