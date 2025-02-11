#include "test.h"

#if 0
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

    debug_print("[CANARY_CHECK] Verifying canaries for block at %p\n", (void*)block);
    debug_print("\tPre-canary value: %lx\n", *pre_canary);
    debug_print("\tPost-canary value: %lx\n", *post_canary);

    if (*pre_canary != ALLOC_DEBUG_CANARY_VALUE || *post_canary != ALLOC_DEBUG_CANARY_VALUE)
    {
        debug_print("[ERROR] Canary corruption detected at block %p\n", (void*)block);
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

    debug_print("[CANARY_CHECK] Verifying canaries for page at %p\n", (void*)page);

    for (uint16_t i = 0; i < page->entrycount; i++) {
        char* block_address = base_address + (i * REAL_ENTRY_SIZE(page->entrysize));
        debug_print("\tChecking block %d at address %p\n", i, block_address);
        MetaData* metadata = (MetaData*)(block_address + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tBlock %d metadata state: isalloc=%d\n", i, metadata->isalloc);

        if (metadata->isalloc) {
            debug_print("\tAllocated block detected, verifying canaries...\n");
            alloced_blocks++;
            assert(verifyCanariesInBlock(block_address, page->entrysize));
        }
    }
    debug_print("\n");

    debug_print("[CANARY_CHECK] Verifying freelist for page at %p\n", (void*)page);
    while(list){
        debug_print("\tChecking freelist block at %p\n", (void*)list);
        MetaData* metadata = (MetaData*)((char*)list + ALLOC_DEBUG_CANARY_SIZE);
        debug_print("\tFreelist block metadata state: isalloc=%d\n", metadata->isalloc);

        if(metadata->isalloc){
            debug_print("[ERROR] Block in free list was allocated at %p\n", (void*)list);
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

    debug_print("[CANARY_CHECK] Verifying all pages in bin\n");

    while (current_page) {
        debug_print("[CANARY_CHECK] Verifying canaries in page at %p (all_pages)\n", (void*)current_page);
        verifyCanariesInPage(current_page);
        current_page = current_page->next;
    }

    while (evac_page) {
        debug_print("[CANARY_CHECK] Verifying canaries in page at %p (evacuate_page)\n", (void*)evac_page);
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

void test_mark_single_object(AllocatorBin* bin) 
{
    Object* obj = create_root(bin);

    Object* child = create_child(bin, obj);

    mark_from_roots(bin);

    assert_all_marked(obj);

    MetaData* reset_child_meta = META_FROM_OBJECT(child);
    // This checks that the metadata at original location of child is reset, meaning allocations can occur here again,
    // And that our object was successfully moved to evac page
    assert( reset_child_meta->ismarked == false );

    debug_print("[SUCCESS] Test Case 1 Passed: Single object marked successfully.\n\n");
}
/* Maybe have these args allow us to easily modify graph? */
void test_mark_object_graph(AllocatorBin *bin, int num_roots, int num_children_per_node, int max_depth)
{
    /** 
    * TODO: Allow these args (num_* and max_depth) to actually be used so we can 
    * test a bunch of weirdly sized graphs and do more stress testing on the
    * allocation and current gc code.
    **/

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

    mark_from_roots(bin);

    assert_all_marked(obj1);
    assert_all_marked(obj2);
    assert_all_marked(obj3);

    assert( rdm_md->ismarked == false);
    assert( META_FROM_OBJECT(random_unmarked_child)->ismarked == false);

    debug_print("[SUCCESS] Test Case 2 Passed: Object graph marked successfully.\n\n");
}

void test_canary_failure(AllocatorBin *bin)
{
    uint64_t* canary_cobber = (uint64_t*)allocate(bin, NULL);

    debug_print("[DEBUG] Allocated test object at address %p\n", canary_cobber);

    /* Write some random data to pre canary */
    canary_cobber[-3] = 0xBADBADBADBADBADB;

    /* If we dont check canaries here we will this object will cause testing evac to fail */
    verifyAllCanaries(bin);
}

void test_evacuation(AllocatorBin* bin) {
    PageInfo* cur_alloc_page = bin->page_manager->all_pages;
    PageInfo* cur_evac_page = bin->page_manager->evacuate_page;

    // Lets make sure only roots are in our allocate pages
    while(cur_alloc_page) {
        for(uint16_t i = 0; i < cur_alloc_page->entrycount; i++) {
            Object* obj = OBJECT_AT(cur_alloc_page, i);
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

                for (int j = 0; j < obj->num_children; j++) {
                    assert(obj->children[j] != NULL);  // Ensure child is not NULL
                }
            }
        }

        debug_print("\n");
        cur_alloc_page = cur_alloc_page->next;
    }

    // Now lets check that no roots made it to evac page
    while(cur_evac_page) {
        for(uint16_t i = 0; i < cur_evac_page->entrycount; i++) {
            Object* obj = OBJECT_AT(cur_evac_page, i);
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

                for (int j = 0; j < obj->num_children; j++) {
                    assert(obj->children[j] != NULL);  // Ensure child is not NULL
                }
            }
        }

        debug_print("\n");
        cur_evac_page = cur_evac_page->next;
    }
}

#endif

#define NUM_ELEMENTS 4096

void arraylist_test() {
    struct ArrayList list;
    arraylist_initialize(&list);

    /* Array created so references to objects can be inserted in ArrayList */
    int test_data[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        test_data[i] = i + 1; 
    }

    for(int val = (NUM_ELEMENTS / 2); val < NUM_ELEMENTS; val++) {
        arraylist_push_tail(list, &test_data[val]);
        debug_print("pushed %i at tail %p\n", test_data[val], list.tail);
    }

    #if 0
    for(int val = 0; val < (NUM_ELEMENTS / 2); val++) {
        arraylist_push_head(list, &test_data[val]);
        debug_print("pushed %i at head %p\n", test_data[val], list.head);
    }

    for(int val = 0; val < (NUM_ELEMENTS / 2); val++) {
        void* old_tail = list.tail;
        void* addr = arraylist_pop_tail(int, list);
        debug_print("tail contains %i at %p stored in page tail at %p\n", *(int*)addr, addr, old_tail);
    }
    #endif

    for(int val = 0; val < (NUM_ELEMENTS / 2); val++) {
        void* old_head = list.head;
        void* addr = arraylist_pop_head(int, list);
        debug_print("head contains %i at %p stored in page head at %p\n", *(int*)addr, addr, old_head);
    }
}

void worklist_test() {
    struct WorkList list;
    worklist_initialize(&list);

    /* Array created so references to objects can be inserted in WorkList */
    int test_data[NUM_ELEMENTS];
    for (int i = 0; i < NUM_ELEMENTS; i++) {
        test_data[i] = i + 1; 
    }

    for(int val = (NUM_ELEMENTS / 2); val < NUM_ELEMENTS; val++) {
        worklist_push(list, &test_data[val]);
        debug_print("pushed %i at tail %p\n", test_data[val], list.tail);
    }

    for(int val = 0; val < (NUM_ELEMENTS / 2); val++) {
        void* old_head = list.head;
        void* addr = worklist_pop(int, list);
        debug_print("head contains %i at %p stored in page head at %p\n", *(int*)addr, addr, old_head);
    }
}

void run_tests()
{
    xallocInitializePageManager(1);

    //arraylist_test();
    worklist_test();

    printf("Hi, I can still compile!\n");
}