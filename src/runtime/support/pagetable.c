#include "pagetable.h"

void** pagetable_root;

void pagetable_init() {
    pagetable_root = (void**)xallocAllocatePage();
    xmem_pageclear(pagetable_root);
}

void pagetable_insert(void* addr) {
    uint64_t address = (uint64_t)addr;
    uint64_t index1 = (address >> 36) & 0xFFF; // Bits 47-36
    uint64_t index2 = (address >> 24) & 0xFFF; // Bits 35-24
    uint64_t index3 = (address >> 12) & 0xFFF; // Bits 23-12
    uint64_t index4 = address & 0xFFF;         // Bits 11-0

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
    level4[index4] = (void*)1; // Mark the page as present   
}

bool pagetable_query(void* addr) {
    uint64_t address = (uint64_t)addr;
    uint64_t index1 = (address >> 36) & 0xFFF; // Bits 47-36
    uint64_t index2 = (address >> 24) & 0xFFF; // Bits 35-24
    uint64_t index3 = (address >> 12) & 0xFFF; // Bits 23-12
    uint64_t index4 = address & 0xFFF;         // Bits 11-0

    void** level1 = pagetable_root;
    if (!level1[index1]) return false;

    void** level2 = (void**)level1[index1];
    if (!level2[index2]) return false;

    void** level3 = (void**)level2[index2];
    if (!level3[index3]) return false;

    void** level4 = (void**)level3[index3];
    return level4[index4] != NULL; // Check if the page is present
}