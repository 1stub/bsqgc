#pragma once 

#include "allocator.h"

#define InitBSQMemoryTheadLocalInfo() { ALLOC_LOCK_ACQUIRE(); register void** rbp asm("rbp"); gtl_info.initialize(GlobalThreadAllocInfo::s_thread_counter++, rbp); ALLOC_LOCK_RELEASE(); }

#define MARK_STACK_NODE_COLOR_GREY 0
#define MARK_STACK_NODE_COLOR_BLACK 1

struct MarkStackEntry
{
    void* obj;
    uintptr_t color;
};

struct RegisterContents
{
    //Should never have pointers of interest in these
    //void* rbp;
    //void* rsp;

    void* rax;
    void* rbx;
    void* rcx;
    void* rdx;
    void* rsi;
    void* rdi;
    void* r8;
    void* r9;
    void* r10;
    void* r11;
    void* r12;
    void* r13;
    void* r14;
    void* r15;
};

//All of the data that a thread local allocator needs to run it's operations
struct BSQMemoryTheadLocalInfo
{
    size_t tl_id; //ID of the thread

    GCAllocator** g_gcallocs;

    ////
    //Mark Phase information
    void** native_stack_base; //the base of the native stack

    size_t native_stack_count;
    void** native_stack_contents; //the contents of the native stack extracted in the mark phase
    RegisterContents native_register_contents; //the contents of the native registers extracted in the mark phase

    //We assume that the roots always fit in a single page block
    size_t roots_count;
    void** roots;
    
    size_t old_roots_count;
    void** old_roots;

    size_t forward_table_index = 0;
    void** forward_table;

    uint32_t newly_filled_pages_count = 0;

    ArrayList<void*> pending_roots; //the worklist of roots that we need to do visits from
    ArrayList<MarkStackEntry> visit_stack; //stack for doing a depth first visit (and topo organization) of the object graph

    ArrayList<void*> pending_young; //the list of young objects that need to be processed
    ArrayList<void*> pending_decs; //the list of objects that need to be decremented 

    size_t max_decrement_count;

#ifdef BSQ_GC_CHECK_ENABLED
    bool disable_stack_refs_for_tests = false;
#endif

    BSQMemoryTheadLocalInfo() noexcept : tl_id(0), g_gcallocs(nullptr), native_stack_base(nullptr), native_stack_count(0), native_stack_contents(nullptr), roots_count(0), roots(nullptr), old_roots_count(0), old_roots(nullptr), forward_table_index(0), forward_table(nullptr), pending_roots(), visit_stack(), pending_young(), pending_decs(), max_decrement_count(BSQ_INITIAL_MAX_DECREMENT_COUNT) { }

    inline GCAllocator* getAllocatorForPageSize(PageInfo* page) noexcept {
        GCAllocator* gcalloc = this->g_gcallocs[page->allocsize >> 3];
        return gcalloc;
    }

    void initialize(size_t tl_id, void** caller_rbp) noexcept;

    template <size_t NUM>
    void initializeGC(GCAllocator* allocs[NUM]) noexcept
    {
        for(size_t i = 0; i < NUM; i++) {
            GCAllocator* alloc = allocs[i];
            this->g_gcallocs[alloc->getAllocSize() >> 3] = alloc;
        }
    }

    void loadNativeRootSet() noexcept;
    void unloadNativeRootSet() noexcept;
};

extern thread_local BSQMemoryTheadLocalInfo gtl_info;

