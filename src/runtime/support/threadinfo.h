#pragma once 

#include "../common.h"
#include "xalloc.h"

thread_local size_t tl_id;

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

//This needs to be initialized on thread creation
thread_local void** native_stack_base;
thread_local void** native_stack_contents;
thread_local struct RegisterContents native_register_contents;

void initializeStartup();
void initializeThreadLocalInfo();

void loadNativeRootSet();
void unloadNativeRootSet();