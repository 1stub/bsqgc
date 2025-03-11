#pragma once

#include "../common.h"
#include "../support/arraylist.h"
#include "../support/pagetable.h"
#include "gc.h"

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

//global storage for constant data (and testing support)
//  -- Only a single thread may run while initializing the global roots as they are visible to all threads
//  -- After initialization a GC must be run to promote all values to old ref-count space
//  -- TODO: when we add multi-threading we need to use the special root-ref tag for these roots as well -- then we can skip re-scanning these after the promotion collection

class GlobalDataStorage
{
public:
    void** native_global_storage;
    void** native_global_storage_end;

    GlobalDataStorage() noexcept : native_global_storage(nullptr), native_global_storage_end(nullptr) { }

    static GlobalDataStorage g_global_data;

    void initialize(size_t numbytes, void** data) noexcept
    {
        this->native_global_storage = data;
        this->native_global_storage_end = (void**)((uint8_t*)data + numbytes);
    }
};

struct FreeListEntry
{
   FreeListEntry* next;
};
static_assert(sizeof(FreeListEntry) <= sizeof(MetaData), "BlockHeader size is not 8 bytes");

class PageInfo
{
public:
    FreeListEntry* freelist; //allocate from here until nullptr
    PageInfo* next;
    PageInfo* left; //left pointer in bst
    PageInfo* right; //right pointer in bst

    uint8_t* data; //start of the data block

    uint16_t allocsize; //size of the alloc entries in this page (excluding metadata)
    uint16_t realsize; //size of the alloc entries in this page (including metadata and other stuff)
    
    uint16_t entrycount; //max number of objects that can be allocated from this Page
    uint16_t freecount;

    float approx_utilization;

    static PageInfo* initialize(void* block, uint16_t allocsize, uint16_t realsize) noexcept;

    void rebuild() noexcept;

    static inline PageInfo* extractPageFromPointer(void* p) noexcept {
        return (PageInfo*)((uintptr_t)(p) & PAGE_ADDR_MASK);
    }

    static inline size_t getIndexForObjectInPage(void* p) noexcept {
        const PageInfo* page = extractPageFromPointer(p);
        
        return (size_t)((uint8_t*)p - page->data) / (size_t)page->realsize;
    }

    static inline MetaData* getObjectMetadataAligned(void* p) noexcept {
        const PageInfo* page = extractPageFromPointer(p);
        size_t idx = (size_t)((uint8_t*)p - page->data) / (size_t)page->realsize;

#ifdef ALLOC_DEBUG_CANARY
        return (MetaData*)(page->data + idx * page->realsize + ALLOC_DEBUG_CANARY_SIZE);
#else
        return (MetaData*)(page->data + idx * page->realsize);
#endif
    }

    inline MetaData* getMetaEntryAtIndex(size_t idx) const noexcept {
#ifdef ALLOC_DEBUG_CANARY
        return (MetaData*)(this->data + idx * this->realsize + ALLOC_DEBUG_CANARY_SIZE);
#else
        return (MetaData*)(this->data + idx * this->realsize);
#endif
    }

    inline FreeListEntry* getFreelistEntryAtIndex(size_t idx) const noexcept {
        return (FreeListEntry*)(this->data + idx * this->realsize);
    }

    static void initializeWithDebugInfo(void* mem, TypeInfoBase* type) noexcept
    {
        uint64_t* pre = (uint64_t*)mem;
        *pre = ALLOC_DEBUG_CANARY_VALUE;

        uint64_t* post = (uint64_t*)((uint8_t*)mem + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + type->type_size);
        *post = ALLOC_DEBUG_CANARY_VALUE;
    }
};

class GlobalPageGCManager
{
private:
    PageInfo* empty_pages;
    PageTableInUseInfo pagetable;

public:
    static GlobalPageGCManager g_gc_page_manager;

    GlobalPageGCManager() noexcept : empty_pages(nullptr) { }

    PageInfo* allocateFreshPage(uint16_t entrysize, uint16_t realsize) noexcept;

    bool pagetable_query(void* addr) const noexcept
    {
        return this->pagetable.pagetable_query(addr);
    }

    void addNewPage(PageInfo* newPage) noexcept
    {
        newPage->next = empty_pages;  
        empty_pages = newPage;        
    }
};

#ifndef ALLOC_DEBUG_CANARY
#define SETUP_ALLOC_LAYOUT_GET_META_PTR(BASEALLOC) (MetaData*)((uint8_t*)(BASEALLOC))
#define SETUP_ALLOC_LAYOUT_GET_OBJ_PTR(BASEALLOC) (void*)((uint8_t*)(BASEALLOC) + sizeof(MetaData))

#define SET_ALLOC_LAYOUT_HANDLE_CANARY(BASEALLOC, T)
#else
#define SETUP_ALLOC_LAYOUT_GET_META_PTR(BASEALLOC) (MetaData*)((uint8_t*)(BASEALLOC) + ALLOC_DEBUG_CANARY_SIZE)
#define SETUP_ALLOC_LAYOUT_GET_OBJ_PTR(BASEALLOC) (void*)((uint8_t*)(BASEALLOC) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData))

#define SET_ALLOC_LAYOUT_HANDLE_CANARY(BASEALLOC, T) PageInfo::initializeWithDebugInfo(BASEALLOC, T)
#endif

#define SETUP_ALLOC_INITIALIZE_FRESH_META(META, T) *(META) = { .type=(T), .isalloc=true, .isyoung=true, .ismarked=false, .isroot=false, .forward_index=MAX_FWD_INDEX, .ref_count=0 }
#define SETUP_ALLOC_INITIALIZE_CONVERT_OLD_META(META, T) *(META) = { .type=(T), .isalloc=true, .isyoung=false, .ismarked=false, .isroot=false, .forward_index=MAX_FWD_INDEX, .ref_count=0 }

#define AllocType(T, A, L) (T*)(A.allocate(L))

#define CALC_APPROX_UTILIZATION(P) 1.0f - ((float)P->freecount / (float)P->entrycount)

#define NUM_LOW_UTIL_BUCKETS 12
#define NUM_HIGH_UTIL_BUCKETS 6

#define IS_LOW_UTIL(U) (U >= 0.01f && U < 0.60f)
#define IS_HIGH_UTIL(U) (U > 0.60f && U <= 0.90f)
#define IS_FULL(U) (U > 0.90f)

//Find proper bucket based on increments of 0.05f
#define GET_BUCKET_INDEX(U, N, I)                   \
do {                                                \
    float tmp_util = 0.0f;                          \
    for (int i = 0; i < N; i++) {                   \
        float new_tmp_util = tmp_util + 0.05f;      \
        if (U > tmp_util && U <= new_tmp_util) {    \
            I = i;                                  \
            break;                                  \
        }                                           \
        tmp_util = new_tmp_util;                    \
    }                                               \
} while (0)

class GCAllocator
{
private:
    FreeListEntry* freelist;
    FreeListEntry* evacfreelist;

    PageInfo* alloc_page; // Page in which we are currently allocating from
    PageInfo* evac_page; // Page in which we are currently evacuating from

    //should match sizes in the page infos
    uint16_t allocsize; //size of the alloc entries in this page (excluding metadata)
    uint16_t realsize; //size of the alloc entries in this page (including metadata and other stuff)

    PageInfo* pendinggc_pages; // Pages that are pending GC

    // Each "bucket" is a binary tree storing 5% of variance in approx_utiliation
    PageInfo* low_utilization_buckets[NUM_LOW_UTIL_BUCKETS]; // Pages with 1-60% utilization (does not hold fully empty)
    PageInfo* high_utilization_buckets[NUM_HIGH_UTIL_BUCKETS]; // Pages with 61-90% utilization 

    PageInfo* filled_pages; // Pages with over 90% utilization (no need for buckets here)
    //completely empty pages go back to the global pool

    void (*collectfp)();

    PageInfo* getFreshPageForAllocator() noexcept
    {
        //find lowest util bucket with stuff, get lowest util page from bucket
        PageInfo* page = findLowestUtilPage(low_utilization_buckets, NUM_LOW_UTIL_BUCKETS);
        if(page == nullptr) {
            page = GlobalPageGCManager::g_gc_page_manager.allocateFreshPage(this->allocsize, this->realsize);
        }

        return page;
    }

    PageInfo* getFreshPageForEvacuation() noexcept
    {
        //Try to grab high util, if fails go to low, fall thoguh making fresh page
        PageInfo* page = findLowestUtilPage(high_utilization_buckets, NUM_HIGH_UTIL_BUCKETS);
        if(page == nullptr) {
            page = findLowestUtilPage(low_utilization_buckets, NUM_LOW_UTIL_BUCKETS);
        }
        if(page == nullptr) {
            page = GlobalPageGCManager::g_gc_page_manager.allocateFreshPage(this->allocsize, this->realsize);
        }

        return page;
    }

    void allocatorRefreshPage() noexcept
    {
        if(this->alloc_page == nullptr) {
            this->alloc_page = this->getFreshPageForAllocator();
        }
        else {
            //rotate collection pages
            processPage(this->alloc_page);
            this->alloc_page = nullptr;

            //use BSQ_COLLECTION_THRESHOLD; NOTE ONLY INCREMENT when we have a full page
            static int filled_pages_count = 0;
            filled_pages_count++;

            //check if we need to collect and do so
            if(filled_pages_count == BSQ_COLLECTION_THRESHOLD) {
                collect();
                filled_pages_count = 0;
            }
        
            //get the new page
            this->alloc_page = this->getFreshPageForAllocator();
        }

        this->freelist = this->alloc_page->freelist;
    }

    void allocatorRefreshEvacuationPage() noexcept
    {
        this->evac_page = this->getFreshPageForEvacuation();
        this->evacfreelist = this->evac_page->freelist;
    }

    inline void insertPageInBucket(PageInfo** bucket, PageInfo* new_page, float n_util, int num_bucket) 
    {
        PageInfo* root = bucket[num_bucket];                                           
        if(root == nullptr) {
            bucket[num_bucket] = new_page;
            new_page->left = nullptr;
            new_page->right = nullptr;

            return ;
        }
    
        PageInfo* current = root;
        while (true) {
            if (n_util < current->approx_utilization) {
                if (current->left == nullptr) {
                    //Insert as the left child
                    current->left = new_page;
                    new_page->left = nullptr;
                    new_page->right = nullptr;
                    break;
                } else {
                    current = current->left;
                }
            } else {
                if (current->right == nullptr) {
                    //Insert as the right child
                    current->right = new_page;
                    new_page->left = nullptr;
                    new_page->right = nullptr;
                    break;
                } else {
                    current = current->right;
                }
            }
        }
    }

    PageInfo* findLowestUtilPage(PageInfo** buckets, int n)
    {
        //it is crucial we remove the page we find here
        PageInfo* p = nullptr;
        PageInfo* parent = nullptr;
        for(int i = 0; i < n; i++) {
            PageInfo* cur = buckets[i];
            if(cur == nullptr) {
                continue;
            }
            while(cur->left != nullptr) {
                parent = cur;
                cur = cur->left;
            }

            p = cur;

            //just to be safe
            cur->left = nullptr;
            cur->right = nullptr;

            if(parent != nullptr) {
                parent->left = nullptr;
                break;
            }
            else if(cur->right != nullptr) {
                buckets[i] = cur->right;
            }
            else {
                buckets[i] = nullptr;
                break;
            }
        }
        return p;
    }

public:
    GCAllocator(uint16_t allocsize, uint16_t realsize, void (*collect)()) noexcept : freelist(nullptr), evacfreelist(nullptr), alloc_page(nullptr), evac_page(nullptr), allocsize(allocsize), realsize(realsize), pendinggc_pages(nullptr), low_utilization_buckets{}, high_utilization_buckets{}, filled_pages(nullptr), collectfp(collect) { }

    inline size_t getAllocSize() const noexcept
    {
        return this->allocsize;
    }

    //simple check to see if a page is in alloc/evac/pendinggc pages
    bool checkNonAllocOrGCPage(PageInfo* p) {
        if(p == alloc_page || p == evac_page) {
            return false;
        }

        PageInfo* cur = pendinggc_pages;
        while(cur != nullptr) {
            if(cur == p) {
                return false;
            }
            cur = cur->next;
        }

        return true;
    }

    inline void* allocate(TypeInfoBase* type)
    {
        assert(type->type_size == this->allocsize);

        if(this->freelist == nullptr) [[unlikely]] {
            this->allocatorRefreshPage();
        }

        void* entry = this->freelist;
        this->freelist = this->freelist->next;
        this->alloc_page->freelist = this->alloc_page->freelist->next;
            
        this->alloc_page->freecount--;

        SET_ALLOC_LAYOUT_HANDLE_CANARY(entry, type);
        SETUP_ALLOC_INITIALIZE_FRESH_META(SETUP_ALLOC_LAYOUT_GET_META_PTR(entry), type);

        return SETUP_ALLOC_LAYOUT_GET_OBJ_PTR(entry);
    }

    inline void* allocateEvacuation(TypeInfoBase* type)
    {
        assert(type->type_size == this->allocsize);

        if(this->evacfreelist == nullptr) [[unlikely]] {
            this->allocatorRefreshEvacuationPage();
        }

        void* entry = this->evacfreelist;
        this->evacfreelist = this->evacfreelist->next;
        this->evac_page->freelist = this->evac_page->freelist->next;

        this->evac_page->freecount--;

        SET_ALLOC_LAYOUT_HANDLE_CANARY(entry, type);
        SETUP_ALLOC_INITIALIZE_CONVERT_OLD_META(SETUP_ALLOC_LAYOUT_GET_META_PTR(entry), type);

        return SETUP_ALLOC_LAYOUT_GET_OBJ_PTR(entry);
    }

    //Take a page (that may be in of the page sets -- or may not -- if it is a alloc or evac page) and move it to the appropriate page set
    void processPage(PageInfo* p) noexcept;

    //process all the pending gc pages, the current alloc page, and evac page -- reset for next round
    void processCollectorPages() noexcept;
};
