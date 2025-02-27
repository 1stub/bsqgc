#include "pagetable.h"

#define LEVEL1_SHIFT 36
#define LEVEL2_SHIFT 24
#define LEVEL3_SHIFT 12
#define LEVEL_MASK 0xFFF
#define PAGE_PRESENT 1

void** pagetable_root;

void pagetable_init() {
    pagetable_root = (void**)xallocAllocatePage();
    xmem_pageclear(pagetable_root);
}

void pagetable_insert(void* addr) {
    uintptr_t address = (uintptr_t)addr;
    uintptr_t index1 = (address >> LEVEL1_SHIFT) & LEVEL_MASK; // Bits 47-36
    uintptr_t index2 = (address >> LEVEL2_SHIFT) & LEVEL_MASK; // Bits 35-24
    uintptr_t index3 = (address >> LEVEL3_SHIFT) & LEVEL_MASK; // Bits 23-12
    uintptr_t index4 = address & LEVEL_MASK;                   // Bits 11-0

    void** level1 = pagetable_root;
    if (!level1[index1]) {
        level1[index1] = (void**)xallocAllocatePage();
        xmem_pageclear(level1[index1]);
    }

    void** level2 = (void**)level1[index1];
    if (!level2[index2]) {
        level2[index2] = (void**)xallocAllocatePage();
        xmem_pageclear(level2[index2]);
    }

    void** level3 = (void**)level2[index2];
    if (!level3[index3]) {
        level3[index3] = (void**)xallocAllocatePage();
        xmem_pageclear(level3[index3]);
    }

    void** level4 = (void**)level3[index3];
    level4[index4] = (void*)PAGE_PRESENT;  
}

bool pagetable_query(void* addr) {
    uintptr_t address = (uintptr_t)addr;
    uintptr_t index1 = (address >> LEVEL1_SHIFT) & LEVEL_MASK;  // Bits 47-36
    uintptr_t index2 = (address >> LEVEL2_SHIFT) & LEVEL_MASK;  // Bits 35-24
    uintptr_t index3 = (address >> LEVEL3_SHIFT) & LEVEL_MASK;  // Bits 23-12
    uintptr_t index4 = (address & PAGE_ADDR_MASK) & LEVEL_MASK; // Bits 11-0

    void** level1 = pagetable_root;
    if (!level1[index1]) return false;

    void** level2 = (void**)level1[index1];
    if (!level2[index2]) return false;

    void** level3 = (void**)level2[index2];
    if (!level3[index3]) return false;

    void** level4 = (void**)level3[index3];
    return level4[index4] == (void*)PAGE_PRESENT;
}