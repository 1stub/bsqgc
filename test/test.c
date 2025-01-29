#include "test.h"

Object* create_root(AllocatorBin* bin) 
{
    Object* obj = (Object*)allocate(bin, NULL);
    assert(obj != NULL);
    debug_print("Allcoated root at address : %p\n", obj);

    // For testing we assume all objects created here are roots
    if(isRoot(obj)){
        if(root_count < MAX_ROOTS){
            root_stack[root_count] = obj;
            root_count++;
        }
    }

    MetaData* metadata = META_FROM_OBJECT(obj);
    metadata->isroot = true;

    return obj; 
}

Object* create_child(AllocatorBin* bin, Object* parent)
{
    Object* child = (Object*)allocate(bin, NULL);
    assert(child != NULL);
    debug_print("Allcoated child at address : %p\n", child);

    parent->children[parent->num_children] = child;
    parent->num_children++;

    return child;
}

/* Following 3 methods verify integrity of canaries */
bool verifyCanariesInBlock(char* block, uint16_t entry_size)
{
    uint64_t* pre_canary = (uint64_t*)(block);
    uint64_t* post_canary = (uint64_t*)(block + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData) + entry_size);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        debug_print("[ERROR] Canary corrupted at block %p\n", (void*)block);
        debug_print("Data in pre-canary: %lx, data in post-canary: %lx\n", *pre_canary, *post_canary);
        return false;
    }
    return true;
}

void verifyCanariesInPage(PageInfo* page)
{
    FreeListEntry* list = page->freelist;
    char* base_address = (char*)page + sizeof(PageInfo);
    uint16_t alloced_blocks = 0;
    uint16_t free_blocks = 0;

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        debug_print("Checking block: %p\n", block_address);
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("Metadata state: isalloc=%d\n", metadata->isalloc);

        if (metadata->isalloc) {
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }

    debug_print("\n");

    while(list){
        debug_print("Checking freelist block: %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("Metadata state: isalloc=%d\n", metadata->isalloc);
        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated\n");
            assert(0);
        }
        free_blocks++;
        list = list->next;
    }   

    // Make sure no blocks are lost
    assert((free_blocks + alloced_blocks) == page->entrycount);

    debug_print("\n");
}

void verifyAllCanaries(AllocatorBin* bin)
{
    PageInfo* current_page = bin->page_manager->all_pages;
    PageInfo* evac_page = bin->page_manager->evacuate_page;

    while (current_page) {
        debug_print("PageManager all_pages head address: %p\n", current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }

    while (evac_page) {
        debug_print("PageManager evac_page head address: %p\n", evac_page);
        verifyCanariesInPage(evac_page);
        evac_page = evac_page->next;
    }

}

// Helper function to recursively assert that all objects in the graph are marked
void assert_all_marked(Object* obj) {
    if (obj == NULL) {
        return;
    }

    assert(META_FROM_OBJECT(obj)->ismarked == true);

    for (int i = 0; i < obj->num_children; i++) {
        assert_all_marked(obj->children[i]);
    }
}

void test_mark_single_object(AllocatorBin* bin, PageManager* pm) 
{
    Object* obj = create_root(bin);

    Object* child = create_child(bin, obj);

    mark_from_roots();

    assert_all_marked(obj);

    MetaData* reset_child_meta = META_FROM_OBJECT(child);
    // This checks that the metadata at original location of child is reset, meaning allocations can occur here again,
    // And that our object was successfully moved to evac page
    assert( reset_child_meta->ismarked == false );

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

    create_child(bin, child1);
    create_child(bin, child2);
    create_child(bin, child3);

    Object* random_unmarked_obj = (Object*)allocate(bin, NULL);
    debug_print("[DEBUG] Created non root random object at %p\n", random_unmarked_obj);
    
    Object* random_unmarked_child = create_child(bin, random_unmarked_obj);
    MetaData* rdm_md = META_FROM_OBJECT(random_unmarked_obj);

    mark_from_roots();

    assert_all_marked(obj1);
    assert_all_marked(obj2);
    assert_all_marked(obj3);

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

    Object* random_unmarked_obj = (Object*)allocate(bin, NULL);
    Object* random_unmarked_child = create_child(bin, random_unmarked_obj);
    MetaData* rdm_md = META_FROM_OBJECT(random_unmarked_obj);

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
    uint64_t* canary_cobber = (uint64_t*)allocate(bin, NULL);

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
                (i * REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE)) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
            MetaData* meta = META_FROM_OBJECT(obj);

            debug_print("[DEBUG] Verifying object at %p in alloc page was moved or is root\n", obj);
            
            /** 
            * After evacuation all objects still in alloc pages should only be roots,
            * be allocated, be marked, and have a non set forward index. If there are nonalloc objs
            * that means they have been evacuated and we will not test any of their flags. Those are all
            * set in RESET_META_FOR_OBJ()
            **/
            if(meta->isalloc == true) {
                debug_print("[DEBUG] Verifying flags for %p\n", obj);

                assert(meta->isroot); 
                assert(meta->forward_index == UINT32_MAX); //not forwarded
                assert(meta->ismarked);
                assert(meta->isalloc);
            }
        }

        debug_print("\n");
        cur_alloc_page = cur_alloc_page->next;
    }

    // Now lets check that no roots made it to evac page
    while(cur_evac_page) {
        for(uint16_t i = 0; i < cur_evac_page->entrycount; i++) {
            Object* obj = (Object*)((char*)cur_evac_page + sizeof(PageInfo) + 
                (i * REAL_ENTRY_SIZE(DEFAULT_ENTRY_SIZE)) + ALLOC_DEBUG_CANARY_SIZE + sizeof(MetaData));
            MetaData* meta = META_FROM_OBJECT(obj);
            
            /** 
            * After evacuation all objects moved to evac page should not be roots,
            * be allocated, be marked, and have a set forward index.
            **/

            debug_print("[DEBUG] Verifying object at %p in evacuation page\n", obj);

            if(meta->isalloc == true) {
                debug_print("[DEBUG] Verifying flags for %p\n", obj);
                assert(!meta->isroot); 
                assert(meta->forward_index != UINT32_MAX); //forwarded
                assert(meta->ismarked);
                assert(meta->isalloc);
            }
        }

        debug_print("\n");
        cur_evac_page = cur_evac_page->next;
    }
}

void run_tests()
{
    PageManager* pm = initializePageManager(DEFAULT_ENTRY_SIZE);
    AllocatorBin* bin = initializeAllocatorBin(DEFAULT_ENTRY_SIZE);
    test_mark_single_object(bin, pm);
    test_mark_object_graph(bin, pm);
    ///test_mark_cyclic_graph(bin, pm);
    //test_canary_failure(bin,  pm);
    test_evacuation(bin, pm);

    verifyAllCanaries(bin);
}