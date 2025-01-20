#define pragma once

#include "../src/runtime/memory/allocator.h"

/**
 * Our varrying tests for properly marking objects 
 **/
void test_mark_single_object(AllocatorBin* bin, PageManager* pm);
void test_mark_object_graph(AllocatorBin* bin, PageManager* pm);

/**
 * Run all tests together
 **/
extern void runTests();
