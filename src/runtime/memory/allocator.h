#pragma once

#include "../common.h"
#include "../support/arraylist.h"
#include "../support/pagetable.h"

//Can also use other values like 0xFFFFFFFFFFFFFFFFul
#define ALLOC_DEBUG_MEM_INITIALIZE_VALUE 0x0ul

//Must be multiple of 8
#define ALLOC_DEBUG_CANARY_SIZE 16
#define ALLOC_DEBUG_CANARY_VALUE 0xDEADBEEFDEADBEEFul

#ifdef MEM_STATS
#define ENABLE_MEM_STATS
#define MEM_STATS_OP(X) X
#define MEM_STATS_ARG(X) X
#else
#define MEM_STATS_OP(X)
#define MEM_STATS_ARG(X)
#endif

// Allows us to correctly determine pointer offsets
#ifdef ALLOC_DEBUG_CANARY
#define REAL_ENTRY_SIZE(ESIZE) (ALLOC_DEBUG_CANARY_SIZE + ESIZE + sizeof(MetaData) + ALLOC_DEBUG_CANARY_SIZE)
#else
#define REAL_ENTRY_SIZE(ESIZE) (ESIZE + sizeof(MetaData))
#endif

////////////////////////////////
//Memory allocator

struct FreeListEntry
{
   FreeListEntry* next;
};
static_assert(sizeof(FreeListEntry) <= sizeof(MetaData), "BlockHeader size is not 8 bytes");

typedef uint16_t PageStateInfo;
#define PageStateInfo_GroundState 0x0
#define AllocPageInfo_ActiveAllocation 0x1
#define AllocPageInfo_ActiveEvacuation 0x2

class PageInfo
{
public:
    FreeListEntry* freelist; //allocate from here until nullptr
    PageInfo* next;

    uint8_t* data; //start of the data block

    uint16_t allocsize; //size of the alloc entries in this page (excluding metadata)
    uint16_t realsize; //size of the alloc entries in this page (including metadata and other stuff)
    
    uint16_t entrycount; //max number of objects that can be allocated from this Page
    uint16_t freecount;

    PageStateInfo pagestate;

    static PageInfo* initialize(void* block, uint16_t allocsize, uint16_t realsize) noexcept;

    static inline constexpr PageInfo* extractPageFromPointer(void* p) noexcept {
        return (PageInfo*)((uintptr_t)(p) & PAGE_ADDR_MASK);
    }

    static inline constexpr size_t getIndexForObjectInPage(void* p) noexcept {
        const PageInfo* page = extractPageFromPointer(p);
        
        return (size_t)(page->data - (uint8_t*)p) / (size_t)page->realsize;
    }

    static inline constexpr MetaData* getObjectMetadataAligned(void* p) noexcept {
        const PageInfo* page = extractPageFromPointer(p);
        size_t idx = (size_t)(page->data - (uint8_t*)p) / (size_t)page->realsize;

#ifdef ALLOC_DEBUG_CANARY
        return (MetaData*)(page->data + idx * page->realsize + ALLOC_DEBUG_CANARY_SIZE);
#else
        return (MetaData*)(page->data + idx * page->realsize);
#endif
    }

    inline constexpr FreeListEntry* getFreeListEntryAtIndex(size_t index) const noexcept {
        return (FreeListEntry*)(this->data + index * realsize);
    }
};

#define GC_GET_META_DATA_ADDR_STD(O) GC_GET_META_DATA_ADDR(O)
#define GC_GET_META_DATA_ADDR_ROOTREF(R) PageInfo::extractObjectMetadataAligned(R)

class GlobalPageGCManager
{
private:
    PageInfo* empty_pages;
    PageTableInUseInfo pagetable;

public:
    static GlobalPageGCManager g_gc_page_manager;

    GlobalPageGCManager() noexcept : empty_pages(nullptr) { }

    PageInfo* GlobalPageGCManager::allocateFreshPage(uint16_t entrysize, uint16_t realsize) noexcept;
};

template <size_t ALLOC_SIZE, size_t REAL_SIZE>
class BinPageGCManager
{
private:
    PageInfo* low_utilization_pages; // Pages with 1-30% utilization (does not hold fully empty)
    PageInfo* mid_utilization_pages; // Pages with 31-60% utilization
    PageInfo* high_utilization_pages; // Pages with 61-90% utilization 

    PageInfo* filled_pages; // Pages with over 90% utilization
    //completely empty pages go back to the global pool

public:
    BinPageGCManager() noexcept : low_utilization_pages(nullptr), mid_utilization_pages(nullptr), high_utilization_pages(nullptr), filled_pages(nullptr) { }

    PageInfo* getFreshPageForAllocator() noexcept
    {
        PageInfo* page = nullptr;

        if(this->low_utilization_pages != nullptr) {
            page = this->low_utilization_pages;
            this->low_utilization_pages = this->low_utilization_pages->next;
        }
        else if(this->mid_utilization_pages != nullptr) {
            page = this->mid_utilization_pages;
            this->mid_utilization_pages = this->mid_utilization_pages->next;
        } 
        else {
            page = GlobalPageGCManager::g_gc_page_manager.allocateFreshPage(ALLOC_SIZE);
        }

        return page;
    }

    PageInfo* getFreshPageForEvacuation() noexcept
    {
        PageInfo* page = nullptr;

        if(this->high_utilization_pages != nullptr) {
            page = this->high_utilization_pages;
            this->high_utilization_pages = this->high_utilization_pages->next;
        }
        else if(this->mid_utilization_pages != nullptr) {
            page = this->mid_utilization_pages;
            this->mid_utilization_pages = this->mid_utilization_pages->next;
        }
        else {
            page = GlobalPageGCManager::g_gc_page_manager.allocateFreshPage(ALLOC_SIZE);
        }

        return page;
    }
};

#ifndef ALLOC_DEBUG_CANARY
#define SETUP_FRESH_ALLOC_LAYOUT_GET_OBJ_PTR(BASEALLOC, T) (void*)((uint8_t*)(BASEALLOC) + sizeof(MetaData))
#else
#define SETUP_FRESH_ALLOC_LAYOUT_GET_OBJ_PTR(BASEALLOC, T) this->initializeWithDebugInfo(BASEALLOC, T)
#endif

#define SETUP_FRESH_ALLOC_META_FLAGS(BASEALLOC, T) *((MetaData*)((uint8_t*)(BASEALLOC) + sizeof(MetaData))) = { .isalloc=true, .isyoung=true, .ismarked=false, .isroot=false, .forward_index=MAX_FWD_INDEX, .ref_count=0, .type=(T) }

//template <size_t ALLOC_SIZE, size_t REAL_SIZE>
class AllocatorBin
{
private:
    FreeListEntry* freelist;

    PageInfo* alloc_page; // Page in which we are currently allocating from
    PageInfo* evac_page; // Page in which we are currently evacuating from

    PageInfo* pendinggc_pages; // Pages that are pending GC
    BinPageGCManager<ALLOC_SIZE, REAL_SIZE> page_manager;

    void allocatorRefreshPage() noexcept
    {
        xxxx; //use BSQ_COLLECTION_THRESHOLD; NOTE ONLY INCREMENT when we have a full page

        //rotate collection pages

        //check if we need to collect and do so
        
        //get the new page
    }

    void* initializeWithDebugInfo(FreeListEntry* ret, TypeInfoBase* type) noexcept
    {
        uint64_t* pre = (uint64_t*)ret;
        *pre = ALLOC_DEBUG_CANARY_VALUE;

        uint64_t* post = (uint64_t*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + REAL_SIZE);
        *post = ALLOC_DEBUG_CANARY_VALUE;

        SETUP_FRESH_ALLOC_META_FLAGS((uint8_t*)(ret) + ALLOC_DEBUG_CANARY_SIZE, type);
        return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
    }

public:
    AllocatorBin() noexcept : freelist(nullptr), alloc_page(nullptr), evac_page(nullptr), pendinggc_pages(nullptr) { }

    inline void* allocate(TypeInfoBase* type)
    {
        assert(type->type_size == ALLOC_SIZE);

        if(this->freelist == nullptr) [[unlikely]] {
            this->allocatorRefreshPage();
        }

        FreeListEntry* entry = this->freelist;
        this->freelist = this->freelist->next;
        this->alloc_page->freecount--;

        SETUP_FRESH_ALLOC_META_FLAGS(entry, type);
        return SETUP_FRESH_ALLOC_LAYOUT_GET_OBJ_PTR(entry, type);
    }
};
