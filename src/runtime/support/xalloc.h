
/**
 * This is the memory allocator that we will use for MANUALLY manged (NON-GC) memory that we need when implementing the GC itself and other runtime support.
 * At this point it supports a pool of pages and allocation from this pool.
 * 
 * This is a thread local (static) allocator.
 */
#pragma once

#include "../common.h"

struct XAllocPage {
    XAllocPage* next;
};

//This class is responsible for managing the allocation of pages for support data structures (NOT GC pages)
//All threads will share this pool of pages for their operations
class XAllocPageManager
{
private:
    XAllocPage* freelist;

    void* allocatePage_impl() noexcept;
    void freePage_impl(void* page) noexcept;

    XAllocPageManager() noexcept : freelist(nullptr) {}
public:
    static XAllocPageManager g_page_manager;

    // Gets min and max pointers on a page from any address in the page
    template <typename T>
    inline void** get_min_for_segment(T* p) noexcept
    {
        return (void**)(((uintptr_t)p & PAGE_ADDR_MASK) + sizeof(T));
    }

    template <typename T>
    inline void** get_max_for_segment(T* p) noexcept
    {
        return (void**)(((uintptr_t)p & PAGE_ADDR_MASK) + BSQ_BLOCK_ALLOCATION_SIZE - sizeof(void*));
    }

    template <typename T>
    T* allocatePage() noexcept
    {
        return (T*)xallocAllocatePage_impl();
    }

    template <typename T>
    void freePage(T* page) noexcept
    {
        xallocFreePage_impl((void*)page);
    }
};

