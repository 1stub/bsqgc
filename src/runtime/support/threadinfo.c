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

void initializeThreadLocalInfo()
{
    int lckok = mtx_lock(&g_lock);
    assert(lckok == thrd_success);

    tl_id = tl_id_counter++;
    xallocInitializePageManager(tl_id);

    int unlckok = mtx_unlock(&g_lock);
    assert(unlckok == thrd_success);
    
    register void* rbp asm("rbp");   
    native_stack_base = rbp;
}

/* Need to discuss specifics of this walking, not totally sure about taking the potential ptr to be cur_frame + 1 */
void loadNativeRootSet()
{
    native_stack_contents = (void**)xallocAllocatePage();
    xmem_pageclear(native_stack_contents);

    //this code should load from the asm stack pointers and copy the native stack into the roots memory
    #ifdef __x86_64__
        /* originally current_frame used rsp */
        register void* rbp asm("rbp");
        register void* rsp asm("rsp");
        void** current_frame = rbp;
        void** end_of_frame = rsp;
        int i = 0;

        /* Walk the stack */
        while (current_frame <= native_stack_base) {            
            assert((uintptr_t)current_frame % 8 == 0);
            assert((uintptr_t)end_of_frame % 8 == 0);

            /* Walk entire frame looking for valid pointers */
            void** it = current_frame;
            while(it > end_of_frame) {            
                void* potential_ptr = *it;
                if (PTR_IN_RANGE(potential_ptr) && PTR_NOT_IN_STACK(native_stack_base, end_of_frame, potential_ptr)) {
                    native_stack_contents[i++] = potential_ptr;
                }
                it--;
            }
            /* Move to the next frame */
            end_of_frame = current_frame + 2; // update frame boundary to just after return of prev frame
            current_frame = *(void**)current_frame; // Move to the next frame
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