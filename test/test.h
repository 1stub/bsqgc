#define pragma once

#include "../src/runtime/memory/allocator.h"
#include "../src/runtime/memory/gc.h"

/**
 * Our varrying tests for properly marking objects 
 **/
void test_mark_single_object(AllocatorBin* bin);
void test_mark_object_graph(AllocatorBin *bin, int num_roots, int num_children_per_node, int max_depth);
void test_canary_failure(AllocatorBin* bin);
void test_evacuation(AllocatorBin* bin);
void test_ref_count(AllocatorBin* bin);

/**
 * Traverse pages and freelists ensuring no canaries are clobbered and that
 * our freelists contain no already allocated objects.
 **/
#ifdef ALLOC_DEBUG_CANARY
void verifyAllCanaries(AllocatorBin* bin);
void verifyCanariesInPage(PageInfo* page);
bool verifyCanariesInBlock(char* block, uint16_t entry_size);
#endif

/**
 * Run all tests together
 **/
extern void run_tests();
