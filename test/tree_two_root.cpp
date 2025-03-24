#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>
#include <iostream>

struct TypeInfoBase TreeNode3Type = {
    .type_id = 1,
    .type_size = 32,
    .slot_size = 4,
    .ptr_mask = "1110",  
    .typekey = "TreeNode3Type"
};

struct TreeNodeValue {
    TreeNodeValue* n1;
    TreeNodeValue* n2;
    TreeNodeValue* n3;

    int64_t val;
};

GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);

//
//Make tree recursively (for now)
//
TreeNodeValue* makeSharedTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr; 
    }

    TreeNodeValue* n = AllocType(TreeNodeValue, alloc4, &TreeNode3Type);
    n->val = val;

    n->n1 = makeSharedTree(depth - 1, val + 1);
    n->n2 = makeSharedTree(depth - 1, val + 1);
    n->n3 = makeSharedTree(depth - 1, val + 1);

    return n;
}

std::string printtree(TreeNodeValue* node) {
    if (node == nullptr) {
        return "null"; 
    }

    std::string addr = "xx"; 
    std::string nodeStr = "[" + addr + ", " + std::to_string(node->val) + "]";

    std::string childStrs;
    childStrs += printtree(node->n1) + ", ";
    childStrs += printtree(node->n2) + ", ";
    childStrs += printtree(node->n3);

    return nodeStr + ", " + childStrs;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Create two root objects, collect, then drop one and collect again.
//No objects should be deleted since they still have one root ref
//after dropping the second.
//

//TODO: Calculate memstats for total bytes when rebuilding a page
int main(int argc, char **argv)
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;
    gtl_info.disable_stack_refs_for_tests = true;

    GCAllocator* allocs[1] = { &alloc4 };
    gtl_info.initializeGC<1>(allocs);

    TreeNodeValue* root1 = makeSharedTree(10, 2);
    garray[0] = root1;
    TreeNodeValue* root2 = AllocType(TreeNodeValue, alloc4, &TreeNode3Type);
    root2->val = 2; //we started with 2 as value for root1
    garray[1] = root2;

    root2->n1 = root1->n1;
    root2->n2 = root1->n2;
    root2->n3 = root1->n3;

    auto root1_init = printtree(root1);

    uint64_t init_total_bytes = gtl_info.total_live_bytes;

    collect();

    //drop root1
    root1 = nullptr;
    
    collect();

    auto root2_final = printtree(root2);

    assert(root1_init == root2_final);

    //We should only lose one node for root1
    uint64_t final = gtl_info.total_live_bytes;
    assert(init_total_bytes == (final + TreeNode3Type.type_size));

    return 0;
}