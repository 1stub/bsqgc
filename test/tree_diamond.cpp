#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>

struct TypeInfoBase TreeNode2Type = {
    .type_id = 1,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "110",  
    .typekey = "TreeNode2Type"
};

struct TypeInfoBase TreeNode1Type = {
    .type_id = 2,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "10",  
    .typekey = "TreeNode1Type"
};

struct TreeNode2Value {
    void* left;
    void* right;
    int64_t val;
};

struct TreeNode1Value {
    void* next;
    int64_t val;
};

GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);
GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);

std::string printtree(void* node) 
{
    if (node == nullptr) {
        return "null";
    }

    std::string addr = "xx"; 
    std::string nodeStr;

    std::string childStrs;
    TypeInfoBase* type = GC_TYPE(node);

    if (type == &TreeNode1Type) {
        TreeNode1Value* n1 = static_cast<TreeNode1Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n1->val) + "]";
        childStrs += printtree(n1->next);
    }
    else if (type == &TreeNode2Type) {
        TreeNode2Value* n2 = static_cast<TreeNode2Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n2->val) + "]";
        childStrs += printtree(n2->left) + ", " + printtree(n2->right);
    }
    else {
        return "null";
    }

    return nodeStr + ", " + childStrs;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Simple diamond structure tree
//
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;
    gtl_info.disable_stack_refs_for_tests= true;

    GCAllocator* allocs[2] = { &alloc2, &alloc3 };
    gtl_info.initializeGC<2>(allocs);

    TreeNode2Value* root = AllocType(TreeNode2Value, alloc3, &TreeNode2Type);
    root->val = 1;
    garray[0] = root;

    TreeNode1Value* l = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
    l->val = 2;
    root->left = l;

    TreeNode1Value* r = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
    r->val = 2;
    root->right = r;

    TreeNode1Value* end = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
    end->val = 3;
    end->next = nullptr;

    l->next = end;
    r->next = end;

    //root node has two pointers, rest have one
    uint64_t init_total_bytes = 24 + 16 + 16 + 16;

    auto t1_start = printtree(root);
    
    collect();

    auto t1_end = printtree(root);
    assert(t1_start == t1_end);

    assert(init_total_bytes == gtl_info.total_live_bytes);

    garray[0] = nullptr;

    collect();

    assert(gtl_info.total_live_bytes == 0);

    return 0;
}