#include "threadinfo.h"

#include <stdio.h>

thread_local size_t tl_id;
thread_local void** native_stack_base;
thread_local void** native_stack_contents;
thread_local struct RegisterContents native_register_contents;

#define PTR_IN_RANGE(V) ((MIN_ALLOCATED_ADDRESS <= V) && (V <= MAX_ALLOCATED_ADDRESS))
#define PTR_NOT_IN_STACK(BASE, CURR, V) ((((void*)V) < ((void*)CURR)) || (((void*)BASE) < ((void*)V)))
#define IS_ALIGNED(V) (((uintptr_t)(V) % sizeof(void*)) == 0)

/* Was originally ..._contents.##R but preprocessor was not happy */
#define PROCESS_REGISTER(BASE, CURR, R)                                       \
    register void* R asm(#R);                                                 \
    native_register_contents.R = NULL;                                        \
    if(PTR_IN_RANGE(R) && PTR_NOT_IN_STACK(BASE, CURR, R)) { native_register_contents.R = R; }

void initializeStartup()
{
    mtx_init(&g_lock, mtx_plain);
}

void initializeThreadLocalInfo(void* caller_rbp)
{
    int lckok = mtx_lock(&g_lock);
    assert(lckok == thrd_success);

    tl_id = tl_id_counter++;
    xallocInitializePageManager(tl_id);

    int unlckok = mtx_unlock(&g_lock);
    assert(unlckok == thrd_success);
    
    /**
    * Here is what is happening. When we enter this method, we are setting 
    * native_stack_base to be the base pointer for
    * the frame corresponding to this specific method. This causes a new frame 
    * to get pushed onto the stack, rbp becoming rbp for
    * this method and stored, then popped off. This misses the original base 
    * BEFORE we call this method, that base
    * being what we are actually intereseted in.
    **/
    native_stack_base = caller_rbp;
}

void loadNativeRootSet()
{
    native_stack_contents = (void**)xallocAllocatePage();
    xmem_pageclear(native_stack_contents);

    //this code should load from the asm stack pointers and copy the native stack into the roots memory
    #ifdef __x86_64__
        register void* rbp asm("rbp");
        void** current_frame = rbp;
        int i = 0;
        
        /* Walk the stack */
        while (current_frame <= native_stack_base) {
            assert( IS_ALIGNED(current_frame) );
            
            /* Walk entire frame looking for valid pointers */
            void** it = current_frame;
            void* potential_ptr = *it;
            if (PTR_IN_RANGE(potential_ptr) && PTR_NOT_IN_STACK(native_stack_base, current_frame, potential_ptr)) {
                debug_print("potential pointer %p\n", potential_ptr);
                native_stack_contents[i++] = potential_ptr;
            }
            it--;
            
            current_frame++;
        }
    

        /* Check contents of registers */
        PROCESS_REGISTER(native_stack_base, current_frame, rax)
        PROCESS_REGISTER(native_stack_base, current_frame, rbx)
        PROCESS_REGISTER(native_stack_base, current_frame, rcx)
        PROCESS_REGISTER(native_stack_base, current_frame, rdx)
        PROCESS_REGISTER(native_stack_base, current_frame, rsi)
        PROCESS_REGISTER(native_stack_base, current_frame, rdi)
        PROCESS_REGISTER(native_stack_base, current_frame, r8)
        PROCESS_REGISTER(native_stack_base, current_frame, r9)
        PROCESS_REGISTER(native_stack_base, current_frame, r10)
        PROCESS_REGISTER(native_stack_base, current_frame, r11)
        PROCESS_REGISTER(native_stack_base, current_frame, r12)
        PROCESS_REGISTER(native_stack_base, current_frame, r13)
        PROCESS_REGISTER(native_stack_base, current_frame, r14)
        PROCESS_REGISTER(native_stack_base, current_frame, r15)
    #else
        #error "Architecture not supported"
    #endif
}

void unloadNativeRootSet()
{
    xallocFreePage(native_stack_contents);
}

/* Walks from rsp to native stack base, not base to rsp*/
#if 0

        register void* rsp asm("rsp");
        void** current_frame = rsp;
        int i = 0;

        while (current_frame < native_stack_base) {
            void* potential_ptr = *(current_frame + 1);
            if (PTR_IN_RANGE(potential_ptr) & PTR_NOT_IN_STACK(native_stack_base, current_frame, potential_ptr)) {
                native_stack_contents[i++] = potential_ptr;
            }
            current_frame++;
        }

#endif