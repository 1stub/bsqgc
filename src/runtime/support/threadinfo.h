#pragma once 

#include "xalloc.h"
#include "stack.h"
#include "worklist.h"

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

struct BSQMemoryTheadLocalInfo
{
    size_t tl_id;
    void** native_stack_base;

    void** native_stack_contents;
    RegisterContents native_register_contents;

    Stack<void*> marking_stack;

    BSQMemoryTheadLocalInfo() noexcept : tl_id(0), native_stack_base(nullptr), native_stack_contents(nullptr) {}

    void initialize(size_t tl_id, void** caller_rbp) noexcept;

    void loadNativeRootSet() noexcept;
    void unloadNativeRootSet() noexcept;
};

extern thread_local BSQMemoryTheadLocalInfo gtl_info;

