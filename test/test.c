#include "test.h"

void* create_root(AllocatorBin* bin) 
{
    MetaData* metadata;
    Object* obj = (Object*)allocate(bin, &metadata);
    assert(obj != NULL);
    debug_print("Allcoated root at address : %p\n", obj);

    // For testing we assume all objects created here are roots
    if(isRoot(obj)){
        if(root_count < MAX_ROOTS){
            root_stack[root_count] = obj;
            root_count++;
        }
    }

    metadata->isroot = true;

    return (void*)obj; //maybe no need for void*?
}

Object* create_child(AllocatorBin* bin, Object* parent)
{
    MetaData* metadata;
    Object* child = (Object*)allocate(bin, &metadata);
    assert(child != NULL);
    debug_print("Allcoated child at address : %p\n", child);

    parent->children[parent->num_children] = child;
    parent->num_children++;

    return child;
}

/** 
* I suspect it would be better to just use direct references to our static
* bin and page manager from allocator rather than calling initialize methods,
* however it does not appear to have any negative impact
**/
AllocatorBin* setup_bin(PageManager* pm)
{
    AllocatorBin* bin = initializeAllocatorBin(DEFAULT_ENTRY_SIZE, pm);
    assert(bin != NULL);

    return bin;
}

PageManager* setup_pagemgr()
{
    PageManager* pm = initializePageManager(DEFAULT_ENTRY_SIZE);
    assert(pm != NULL);

    return pm;
}

void test_mark_single_object(AllocatorBin* bin, PageManager* pm) 
{
    void* obj = create_root(bin);

    Object* child = create_child(bin, obj);

    mark_from_roots();
    assert( META_FROM_OBJECT(obj)->ismarked == true );
    assert( META_FROM_OBJECT(child)->ismarked == true );

    debug_print("Test Case 1 Passed: Single object marked successfully.\n\n");
}

void test_mark_object_graph(AllocatorBin *bin, PageManager *pm)
{
    Object* obj1 = create_root(bin);
    Object* obj2 = create_root(bin);
    Object* obj3 = create_root(bin);

    assert(obj1 != NULL && obj2 != NULL && obj3 != NULL);

    Object* child1 = create_child(bin, obj1);
    Object* child2 = create_child(bin, obj2);
    Object* child3 = create_child(bin, obj3);

    Object* child_child1 = create_child(bin, child1);
    Object* child_child2 = create_child(bin, child1);
    Object* child_child3 = create_child(bin, child1);

    MetaData* rdm_md;
    Object* random_unmarked_obj = (Object*)allocate(bin, &rdm_md);
    Object* random_unmarked_child = create_child(bin, random_unmarked_obj);

    mark_from_roots();

    assert( META_FROM_OBJECT(obj1)->ismarked == true);
    assert( META_FROM_OBJECT(obj2)->ismarked == true);
    assert( META_FROM_OBJECT(obj3)->ismarked == true);

    assert( META_FROM_OBJECT(child1)->ismarked == true);
    assert( META_FROM_OBJECT(child2)->ismarked == true);
    assert( META_FROM_OBJECT(child3)->ismarked == true);

    assert( META_FROM_OBJECT(child_child1)->ismarked == true);
    assert( META_FROM_OBJECT(child_child2)->ismarked == true);
    assert( META_FROM_OBJECT(child_child3)->ismarked == true);

    assert( rdm_md->ismarked == false);
    assert( META_FROM_OBJECT(random_unmarked_child)->ismarked == false);

    debug_print("Test Case 2 Passed: Object graph marked successfully.\n\n");
}

void test_mark_cyclic_graph(AllocatorBin* bin, PageManager* pm)
{
    Object* obj1 = create_root(bin);
    Object* obj2 = create_root(bin);
    Object* obj3 = create_root(bin);

    assert(obj1 != NULL && obj2 != NULL && obj3 != NULL);

    Object* child1 = create_child(bin, obj1);
    Object* child2 = create_child(bin, obj2);
    Object* child3 = create_child(bin, obj3);

    Object* child_child1 = create_child(bin, child1);
    Object* child_child2 = create_child(bin, child1);
    Object* child_child3 = create_child(bin, child1);

    // Create cycles
    child1->children[child1->num_children] = obj2; 
    child1->num_children++;

    child2->children[child2->num_children] = obj3; 
    child2->num_children++;

    child3->children[child3->num_children] = obj1; 
    child3->num_children++;

    // Nested cycle
    child_child1->children[child_child1->num_children] = child1; 
    child_child1->num_children++;

    MetaData* rdm_md;
    Object* random_unmarked_obj = (Object*)allocate(bin, &rdm_md);
    Object* random_unmarked_child = create_child(bin, random_unmarked_obj);

    mark_from_roots();

    assert(META_FROM_OBJECT(obj1)->ismarked == true);
    assert(META_FROM_OBJECT(obj2)->ismarked == true);
    assert(META_FROM_OBJECT(obj3)->ismarked == true);
    assert(META_FROM_OBJECT(child1)->ismarked == true);
    assert(META_FROM_OBJECT(child2)->ismarked == true);
    assert(META_FROM_OBJECT(child3)->ismarked == true);
    assert(META_FROM_OBJECT(child_child1)->ismarked == true);
    assert(META_FROM_OBJECT(child_child2)->ismarked == true);
    assert(META_FROM_OBJECT(child_child3)->ismarked == true);

    assert(rdm_md->ismarked == false);
    assert( META_FROM_OBJECT(random_unmarked_child)->ismarked == false);

    debug_print("Test Case 3 Passed: Object graph with cycles marked correctly.\n");
}

void test_canary_failure(AllocatorBin *bin, PageManager *pm)
{
    MetaData* metadata;
    uint64_t* canary_cobber = (uint64_t*)allocate(bin, &metadata);

    debug_print("Allocated test object at address %p\n", canary_cobber);

    /* Write some random data to pre canary */
    canary_cobber[-3] = 0xBADBADBADBADBADB;
}

void test_evacuation(AllocatorBin* bin, PageManager* pm) {
    PageInfo* cur_alloc_page = pm->all_pages;
    PageInfo* cur_evac_page = pm->evacuate_page;

    // Lets make sure only roots are in our allocate pages
    while(cur_alloc_page) {
        for(uint16_t i = 0; i < cur_alloc_page->entrycount; i++) {
            Object* obj = (Object*)((char*)cur_alloc_page + sizeof(PageInfo) + 
                (i * REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE)));
            MetaData* meta = META_FROM_OBJECT(obj);
            
            /** 
            * After evacuation all objects still in alloc pages should only be roots,
            * be allocated, be marked, and have a non set forward index.
            **/
            assert(meta->isroot); 
            assert(meta->forward_index == UINT32_MAX); //not forwarded
            assert(meta->ismarked);
            assert(meta->isalloc);
        }

        cur_alloc_page = cur_alloc_page->next;
    }

    // Now lets check that no roots made it to evac page
    while(cur_evac_page) {
        for(uint16_t i = 0; i < cur_evac_page->entrycount; i++) {
            Object* obj = (Object*)((char*)cur_evac_page + sizeof(PageInfo) + 
                (i * REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE)));
            MetaData* meta = META_FROM_OBJECT(obj);
            
            /** 
            * After evacuation all objects moved to evac page should not be roots,
            * be allocated, be marked, and have a set forward index.
            **/
            assert(!meta->isroot); 
            assert(meta->forward_index != UINT32_MAX); //forwarded
            assert(meta->ismarked);
            assert(meta->isalloc);
        }

        cur_evac_page = cur_evac_page->next;
    }
}

void run_tests()
{
    PageManager* pm = setup_pagemgr();
    AllocatorBin* bin = setup_bin(pm);
    test_mark_single_object(bin, pm);
    test_mark_object_graph(bin, pm);
    test_mark_cyclic_graph(bin, pm);
    test_canary_failure(bin,  pm);
    test_evacuation(bin, pm);

    verifyAllCanaries(bin);
}