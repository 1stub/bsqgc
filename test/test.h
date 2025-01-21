#define pragma once

#include "../src/runtime/memory/allocator.h"

/**
 * Our varrying tests for properly marking objects 
 **/
void test_mark_single_object(AllocatorBin* bin, PageManager* pm);
void test_mark_object_graph(AllocatorBin* bin, PageManager* pm);
void test_mark_cyclic_graph(AllocatorBin* bin, PageManager* pm);
void test_canary_failure(AllocatorBin* bin, PageManager* pm);

/**  
* Lets add some more tests for lots of roots spanning multiple pages with (max?) children,
* the introduction of cycles, and lots of objects not conntected to a root!
**/

/**
 * Run all tests together
 **/
extern void run_tests();
