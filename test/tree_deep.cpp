#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>

//had to add extra slot to represent val field
struct TypeInfoBase TreeNodeType = {
    .type_id = 1,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "110",  
    .typekey = "TreeNodeType"
};

struct TreeNodeValue {
    TreeNodeValue* left;
    TreeNodeValue* right;
    int64_t val;
};

GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);

TreeNodeValue* makeTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr; 
    }

    TreeNodeValue* n = AllocType(TreeNodeValue, alloc3, &TreeNodeType);
    n->val = val;

    n->left = makeTree(depth - 1, val);
    n->right = makeTree(depth - 1, val);

    return n;
}

std::string printtree(TreeNodeValue* node) {
    if (node == nullptr) {
        return "null"; 
    }

    std::string addr = "xx"; // Replace with std::format("{:x}", (uintptr_t)node) if available
    std::string nodeStr = "[" + addr + ", " + std::to_string(node->val) + "]";

    std::string leftStr = printtree(node->left);
    std::string rightStr = printtree(node->right);

    return nodeStr + ", " + leftStr + ", " + rightStr;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Full tree of varrying depths
//
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[1] = { &alloc3 };
    gtl_info.initializeGC<1>(allocs);

    //If depth > 10, fails
    //GCAlloc ends up being garbage data
    //Looks like it is assigned a page pointer which clobbers the base pointer
    //Without the manual collection calls this works fine
    TreeNodeValue* t1 = makeTree(14, 4);

    auto t1_start = printtree(t1);
    collect();

    auto t1_end = printtree(t1);

    assert(t1_start == t1_end);

    collect();
    auto t1_end_end = printtree(t1);

    assert(t1_end == t1_end_end);
    assert(t1_start == t1_end_end);

    return 0;
}