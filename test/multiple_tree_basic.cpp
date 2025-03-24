#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>

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

    n->left = makeTree(depth - 1, val + 1);
    n->right = makeTree(depth - 1, val + 1);

    return n;
}

std::string printtree(TreeNodeValue* node) {
    if (node == nullptr) {
        return "null"; 
    }

    std::string addr = "xx"; 
    std::string nodeStr = "[" + addr + ", " + std::to_string(node->val) + "]";

    std::string leftStr = printtree(node->left);
    std::string rightStr = printtree(node->right);

    return nodeStr + ", " + leftStr + ", " + rightStr;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Could be nice to place each tree root in garray
//
int main(int argc, char **argv)
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;

    GCAllocator* allocs[1] = { &alloc3 };
    gtl_info.initializeGC<1>(allocs);

    TreeNodeValue* tree1 = makeTree(1, 4);
    TreeNodeValue* tree2 = makeTree(1, 4);
    TreeNodeValue* tree3 = makeTree(1, 4);
    TreeNodeValue* tree4 = makeTree(1, 4);
    TreeNodeValue* tree5 = makeTree(1, 4);
    TreeNodeValue* tree6 = makeTree(1, 4);
    TreeNodeValue* tree7 = makeTree(1, 4);
    TreeNodeValue* tree8 = makeTree(1, 4);
    TreeNodeValue* tree9 = makeTree(1, 4);
    TreeNodeValue* tree10 = makeTree(1, 4);
    TreeNodeValue* tree11 = makeTree(1, 4);
    TreeNodeValue* tree12 = makeTree(1, 4);
    TreeNodeValue* tree13 = makeTree(1, 4);
    TreeNodeValue* tree14 = makeTree(1, 4);
    TreeNodeValue* tree15 = makeTree(1, 4);
    TreeNodeValue* tree16 = makeTree(1, 4);
    TreeNodeValue* tree17 = makeTree(1, 4);
    TreeNodeValue* tree18 = makeTree(1, 4);
    TreeNodeValue* tree19 = makeTree(1, 4);

    uint64_t init_total_bytes = gtl_info.total_live_bytes;

    assert(tree1 != nullptr);
    assert(tree2 != nullptr);
    assert(tree3 != nullptr);
    assert(tree4 != nullptr);
    assert(tree5 != nullptr);
    assert(tree6 != nullptr);
    assert(tree7 != nullptr);
    assert(tree8 != nullptr);
    assert(tree9 != nullptr);
    assert(tree10 != nullptr);
    assert(tree11 != nullptr);
    assert(tree12 != nullptr);
    assert(tree13 != nullptr);
    assert(tree14 != nullptr);
    assert(tree15 != nullptr);
    assert(tree16 != nullptr);
    assert(tree17 != nullptr);
    assert(tree18 != nullptr);
    assert(tree19 != nullptr);

    auto tree1_initial_state = printtree(tree1);
    auto tree2_initial_state = printtree(tree2);
    auto tree3_initial_state = printtree(tree3);
    auto tree4_initial_state = printtree(tree4);
    auto tree5_initial_state = printtree(tree5);
    auto tree6_initial_state = printtree(tree6);
    auto tree7_initial_state = printtree(tree7);
    auto tree8_initial_state = printtree(tree8);
    auto tree9_initial_state = printtree(tree9);
    auto tree10_initial_state = printtree(tree10);
    auto tree11_initial_state = printtree(tree11);
    auto tree12_initial_state = printtree(tree12);
    auto tree13_initial_state = printtree(tree13);
    auto tree14_initial_state = printtree(tree14);
    auto tree15_initial_state = printtree(tree15);
    auto tree16_initial_state = printtree(tree16);
    auto tree17_initial_state = printtree(tree17);
    auto tree18_initial_state = printtree(tree18);
    auto tree19_initial_state = printtree(tree19);

    collect();

    auto tree1_final_state = printtree(tree1);
    auto tree2_final_state = printtree(tree2);
    auto tree3_final_state = printtree(tree3);
    auto tree4_final_state = printtree(tree4);
    auto tree5_final_state = printtree(tree5);
    auto tree6_final_state = printtree(tree6);
    auto tree7_final_state = printtree(tree7);
    auto tree8_final_state = printtree(tree8);
    auto tree9_final_state = printtree(tree9);
    auto tree10_final_state = printtree(tree10);
    auto tree11_final_state = printtree(tree11);
    auto tree12_final_state = printtree(tree12);
    auto tree13_final_state = printtree(tree13);
    auto tree14_final_state = printtree(tree14);
    auto tree15_final_state = printtree(tree15);
    auto tree16_final_state = printtree(tree16);
    auto tree17_final_state = printtree(tree17);
    auto tree18_final_state = printtree(tree18);
    auto tree19_final_state = printtree(tree19);

    assert(tree1_initial_state == tree1_final_state);
    assert(tree2_initial_state == tree2_final_state);
    assert(tree3_initial_state == tree3_final_state);
    assert(tree4_initial_state == tree4_final_state);
    assert(tree5_initial_state == tree5_final_state);
    assert(tree6_initial_state == tree6_final_state);
    assert(tree7_initial_state == tree7_final_state);
    assert(tree8_initial_state == tree8_final_state);
    assert(tree9_initial_state == tree9_final_state);
    assert(tree10_initial_state == tree10_final_state);
    assert(tree11_initial_state == tree11_final_state);
    assert(tree12_initial_state == tree12_final_state);
    assert(tree13_initial_state == tree13_final_state);
    assert(tree14_initial_state == tree14_final_state);
    assert(tree15_initial_state == tree15_final_state);
    assert(tree16_initial_state == tree16_final_state);
    assert(tree17_initial_state == tree17_final_state);
    assert(tree18_initial_state == tree18_final_state);
    assert(tree19_initial_state == tree19_final_state);

    assert(init_total_bytes == gtl_info.total_live_bytes);

    return 0;
}