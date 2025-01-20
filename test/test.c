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

    markFromRoots();
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

    markFromRoots();

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

void runTests()
{
    PageManager* pm = setup_pagemgr();
    AllocatorBin* bin = setup_bin(pm);
    test_mark_single_object(bin, pm);
    test_mark_object_graph(bin, pm);

    verifyAllCanaries(bin);
}