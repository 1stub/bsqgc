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
private:
    FreeListEntry* freelist; //allocate from here until nullptr
    uint8_t* data; //start of the data block

    uint16_t allocsize; //size of the alloc entries in this page (excluding metadata)
    uint16_t realsize; //size of the alloc entries in this page (including metadata and other stuff)
    uint16_t entrycount; //max number of objects that can be allocated from this Page

    uint16_t freecount;

    PageStateInfo pagestate;

    PageInfo* next;

public:
    static PageInfo* initialize(void* block, uint16_t allocsize) noexcept
    {
        PageInfo* pp = (PageInfo*)block;

        pp->freelist = nullptr;
        pp->data = ((uint8_t*)block + sizeof(PageInfo));
        pp->allocsize = allocsize;
        pp->realsize = REAL_ENTRY_SIZE(allocsize);
        pp->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - (pp->data - (uint8_t*)pp)) / pp->realsize;
        pp->freecount = pp->entrycount;
        pp->pagestate = PageStateInfo_GroundState;
        pp->next = nullptr;
    }

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
    PageInfo* empty_pages; // Empty pages
    PageInfo* all_pages; // Pages with no free space

public:
    static GlobalPageGCManager g_gc_page_manager;

    GlobalPageGCManager() noexcept : empty_pages(nullptr), all_pages(nullptr) { }
};

template <size_t ALLOC_SIZE>
class BinPageGCManager
{
private:
    PageInfo* low_utilization_pages; // Pages with 1-30% utilization (does not hold fully empty)
    PageInfo* mid_utilization_pages; // Pages with 31-85% utilization
    PageInfo* high_utilization_pages; // Pages with 86-100% utilization 

    PageInfo* filled_pages; // Completely full pages
    //completely empty pages go back to the global pool

public:
    BinPageGCManager() noexcept : low_utilization_pages(nullptr), mid_utilization_pages(nullptr), high_utilization_pages(nullptr), filled_pages(nullptr), empty_pages(nullptr) { }
};

template <size_t ALLOC_SIZE>
class AllocatorBin
{
private:
    FreeListEntry* freelist;
    uint16_t entrysize;

    PageInfo* alloc_page; // Page in which we are currently allocating from
    PageInfo* evac_page; // Page in which we are currently evacuating from

    BinPageGCManager<ALLOC_SIZE> page_manager;

public:
    AllocatorBin() noexcept : freelist(nullptr), entrysize(ALLOC_SIZE), alloc_page(nullptr), evac_page(nullptr) { }
};

void getFreshPageForAllocator(AllocatorBin* alloc);
void getFreshPageForEvacuation(AllocatorBin* alloc); 

PageInfo* getPageFromManager(PageManager* pm, uint16_t entrysize);

AllocatorBin* getBinForSize(uint16_t entrytsize);

PageInfo* allocateFreshPage(uint16_t entrysize);

void verifyAllCanaries();
void verifyCanariesInPage(PageInfo* page);
bool verifyCanariesInBlock(char* block, uint16_t entry_size);

static inline void* setupSlowPath(FreeListEntry* ret, AllocatorBin* alloc){
    uint64_t* pre = (uint64_t*)ret;
    *pre = ALLOC_DEBUG_CANARY_VALUE;

    uint64_t* post = (uint64_t*)((char*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + alloc->entrysize);
    *post = ALLOC_DEBUG_CANARY_VALUE;

    return (void*)((uint8_t*)ret + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
}

static inline void* allocate(AllocatorBin* alloc, struct TypeInfoBase* type)
{
    assert(alloc->entrysize == type->type_size);

    if(alloc->freelist == NULL) {
        getFreshPageForAllocator(alloc);
    }

    FreeListEntry* ret = alloc->freelist;
    alloc->freelist = ret->next;

    #ifndef ALLOC_DEBUG_CANARY
    void* obj = (void*)((uint8_t*)ret + sizeof(MetaData));
    #else
    void* obj = setupSlowPath(ret, alloc);
    #endif

    MetaData* mdata = (MetaData*)((char*)obj - sizeof(MetaData));
    SETUP_META_FLAGS(mdata, type);

    alloc->alloc_page->freecount--;

    debug_print("Allocated object at %p\n", obj);
    return (void*)obj;
}
