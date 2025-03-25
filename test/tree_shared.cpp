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

struct TypeInfoBase TreeNode1Type = {
    .type_id = 2,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "10",  
    .typekey = "TreeNode1Type"
};

struct TreeNode3Value {
    TreeNode3Value* n1;
    TreeNode3Value* n2;
    TreeNode3Value* n3;
    int64_t val;
};

struct TreeNode1Value {
    TreeNode3Value* n1;
    int64_t val;
};

GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);
GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);

//
//Make tree recursively (for now)
//
TreeNode3Value* makeSharedTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr; 
    }

    TreeNode3Value* n = AllocType(TreeNode3Value, alloc4, &TreeNode3Type);
    n->val = val;

    n->n1 = makeSharedTree(depth - 1, val + 1);
    n->n2 = makeSharedTree(depth - 1, val + 1);
    n->n3 = makeSharedTree(depth - 1, val + 1);

    return n;
}

std::string printtree(void* node) {
    if (node == nullptr) {
        return "null"; 
    }

    std::string addr = "xx"; 
    std::string nodeStr;

    std::string childStrs;
    TypeInfoBase* type = GC_TYPE(node);
    if(type == &TreeNode1Type) {
        TreeNode1Value* n = static_cast<TreeNode1Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n->val) + "]";
        childStrs += printtree(n->n1);
    }
    else if(type == &TreeNode3Type) {
        TreeNode3Value* n = static_cast<TreeNode3Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n->val) + "]";
        childStrs += printtree(n->n1) + ", ";
        childStrs += printtree(n->n2) + ", ";
        childStrs += printtree(n->n3);
    }

    return nodeStr + ", " + childStrs;
}

uint64_t find_size_bytes(TreeNode3Value* n) 
{
    if(n == nullptr) {
        return 0;
    }

    return TreeNode3Type.type_size + 
           find_size_bytes(n->n1) + 
           find_size_bytes(n->n2) + 
           find_size_bytes(n->n3);
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Create one tree, then have one of its sub trees
//be pointed to my a root object, keeping it alive
//after bigger trees root is dropped
//
int main(int argc, char **argv)
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;
    gtl_info.disable_stack_refs_for_tests = true;

    GCAllocator* allocs[2] = { &alloc2, &alloc4 };
    gtl_info.initializeGC<2>(allocs);

    int depth = 10;
    TreeNode3Value* root1 = makeSharedTree(depth, 2);
    garray[0] = root1;
    TreeNode1Value* root2 = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
    root2->val = 2; //we started with 2 as value for root1
    garray[1] = root2;

    //keep this subtree alive
    root2->n1 = root1->n1->n1;

    auto root1_init = printtree(root1);
    auto root2_init = printtree(root2);

    collect();

    //drop root1
    garray[0] = nullptr;
    
    collect();

    auto root2_final = printtree(root2);
    assert(root2_init == root2_final);

    uint64_t subtree_size_bytes = find_size_bytes(root2->n1);

    //We should not lose root2's tree
    uint64_t expected_root2_final_bytes = subtree_size_bytes + TreeNode1Type.type_size;
    uint64_t final = gtl_info.total_live_bytes;
    assert(final == expected_root2_final_bytes);

    garray[1] = nullptr;

    //We have a pretty big tree here, so we need lots of collections to clear out pending decs
    collect();
    collect();
    collect();
    collect();
    collect();
    collect();
    collect();

    assert(gtl_info.total_live_bytes == 0);

    return 0;
}