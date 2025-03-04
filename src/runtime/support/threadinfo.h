#pragma once 

#include "xalloc.h"
#include "arraylist.h"

#define InitBSQMemoryTheadLocalInfo { ALLOC_LOCK_ACQUIRE(); gtl_info.initialize(GlobalThreadAllocInfo::s_thread_counter++, __builtin_frame_address(0)) ALLOC_LOCK_RELEASE(); }

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

    ////
    //Mark Phase information
    void** native_stack_base; //the base of the native stack
    void** native_stack_contents; //the contents of the native stack extracted in the mark phase
    RegisterContents native_register_contents; //the contents of the native registers extracted in the mark phase

    //We assume that the roots always fit in a single page block
    size_t roots_count;
    void** roots;
    
    size_t old_roots_count;
    void** old_roots;

    size_t forward_table_index = 0;
    void** forward_table;

    ArrayList<void*> marking_stack; //the stack structure used to walk the heap in the mark phase
    ArrayList<void*> pending_young; //the list of young objects that need to be processed
    ArrayList<void*> pending_decs; //the list of objects that need to be decremented 

    size_t max_decrement_count;

    BSQMemoryTheadLocalInfo() noexcept : tl_id(0), native_stack_base(nullptr), native_stack_contents(nullptr), roots_count(0), roots(nullptr), old_roots_count(0), old_roots(nullptr), forward_table_index(0), forward_table(nullptr), marking_stack(), pending_young(), pending_decs(), max_decrement_count(BSQ_INITIAL_MAX_DECREMENT_COUNT) { }

    void initialize(size_t tl_id, void** caller_rbp) noexcept;

    void loadNativeRootSet() noexcept;
    void unloadNativeRootSet() noexcept;
};

extern thread_local BSQMemoryTheadLocalInfo gtl_info;

