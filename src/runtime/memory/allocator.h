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

    static PageInfo* initialize(void* block, uint16_t allocsize) noexcept;

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

#define SETUP_META_FLAGS_FRESH_ALLOC(M, T) *(M) = { .isalloc=true, .isyoung=true, .ismarked=false, .isroot=false, .forward_index=MAX_FWD_INDEX, .ref_count=0, .type=(T) }

class GlobalPageGCManager
{
private:
    PageInfo* empty_pages;
    PageTableInUseInfo pagetable;

public:
    static GlobalPageGCManager g_gc_page_manager;

    GlobalPageGCManager() noexcept : empty_pages(nullptr) { }

    PageInfo* GlobalPageGCManager::allocateFreshPage(uint16_t entrysize) noexcept;
};

template <size_t ALLOC_SIZE>
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

//template <size_t ALLOC_SIZE>
class AllocatorBin
{
private:
    FreeListEntry* freelist;
    uint16_t entrysize;

    PageInfo* alloc_page; // Page in which we are currently allocating from
    PageInfo* evac_page; // Page in which we are currently evacuating from

    BinPageGCManager<ALLOC_SIZE> page_manager;

    void* setupSlowPath(FreeListEntry* ret)
    {
        uint64_t* pre = (uint64_t*)ret;
        *pre = ALLOC_DEBUG_CANARY_VALUE;

        uint64_t* post = (uint64_t*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + alloc->entrysize);
        *post = ALLOC_DEBUG_CANARY_VALUE;

        return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
    }

public:
    AllocatorBin() noexcept : freelist(nullptr), entrysize(ALLOC_SIZE), alloc_page(nullptr), evac_page(nullptr) { }

    inline void* allocate(TypeInfoBase* type)
    {
        assert(alloc->entrysize == type->type_size);

        if(alloc->freelist == NULL) {
            this->getFreshPageForAllocator();
        }

        FreeListEntry* ret = this->freelist;
        this->freelist = this->freelist->next;
        this->alloc_page->freecount--;

#ifndef ALLOC_DEBUG_CANARY
        void* obj = (void*)((uint8_t*)ret + sizeof(MetaData));
#else
        void* obj = this->setupSlowPath(ret);
#endif

        MetaData* mdata = (MetaData*)((char*)obj - sizeof(MetaData));
        SETUP_META_FLAGS(mdata, type);

        return (void*)obj;
    }
};
