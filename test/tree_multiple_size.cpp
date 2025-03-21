#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <random>
#include <string>
#include <format>

struct TypeInfoBase TreeNode1Type = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "10",  
    .typekey = "TreeNode1Type"
};

struct TypeInfoBase TreeNode2Type = {
    .type_id = 2,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "110",  
    .typekey = "TreeNode2Type"
};

struct TypeInfoBase TreeNode3Type = {
    .type_id = 3,
    .type_size = 32,
    .slot_size = 4,
    .ptr_mask = "1110",  
    .typekey = "TreeNode3Type"
};

struct TypeInfoBase TreeNode4Type = {
    .type_id = 4,
    .type_size = 40,
    .slot_size = 5,
    .ptr_mask = "11110",  
    .typekey = "TreeNode4Type"
};

struct TypeInfoBase TreeNode5Type = {
    .type_id = 5,
    .type_size = 48,
    .slot_size = 6,
    .ptr_mask = "111110",  
    .typekey = "TreeNode5Type"
};

struct TreeNodeBase {
    virtual ~TreeNodeBase() = default; // Enable polymorphic behavior
    int64_t val = 0;
};

struct TreeNode1Value : TreeNodeBase{
    TreeNodeBase* n1 = nullptr;
};

struct TreeNode2Value : TreeNodeBase {
    TreeNodeBase* n1 = nullptr;
    TreeNodeBase* n2 = nullptr;
};

struct TreeNode3Value : TreeNodeBase {
    TreeNodeBase* n1 = nullptr;
    TreeNodeBase* n2 = nullptr;
    TreeNodeBase* n3 = nullptr;
};

struct TreeNode4Value : TreeNodeBase{
    TreeNodeBase* n1 = nullptr;
    TreeNodeBase* n2 = nullptr;
    TreeNodeBase* n3 = nullptr;
    TreeNodeBase* n4 = nullptr;
};

struct TreeNode5Value : TreeNodeBase{
    TreeNodeBase* n1 = nullptr;
    TreeNodeBase* n2 = nullptr;
    TreeNodeBase* n3 = nullptr;
    TreeNodeBase* n4 = nullptr;
    TreeNodeBase* n5 = nullptr;
};

GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);
GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);
GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);
GCAllocator alloc5(40, REAL_ENTRY_SIZE(40), collect);
GCAllocator alloc6(48, REAL_ENTRY_SIZE(48), collect);

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> type_dist(1, 5); // Randomly choose between TreeNode1Type to TreeNode5Type

TreeNodeBase* makeTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr;
    }

    // Randomly choose the type of the current node
    int node_type = type_dist(gen);

    switch (node_type) {
        case 1: {
            auto* node = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
            node->val = val;
            node->n1 = makeTree(depth - 1, val + 1);
            return node;
        }
        case 2: {
            auto* node = AllocType(TreeNode2Value, alloc3, &TreeNode2Type);
            node->val = val;
            node->n1 = makeTree(depth - 1, val + 1);
            node->n2 = makeTree(depth - 1, val + 1);
            return node;
        }
        case 3: {
            auto* node = AllocType(TreeNode3Value, alloc4, &TreeNode3Type);
            node->val = val;
            node->n1 = makeTree(depth - 1, val + 1);
            node->n2 = makeTree(depth - 1, val + 1);
            node->n3 = makeTree(depth - 1, val + 1);
            return node;
        }
        case 4: {
            auto* node = AllocType(TreeNode4Value, alloc5, &TreeNode4Type);
            node->val = val;
            node->n1 = makeTree(depth - 1, val + 1);
            node->n2 = makeTree(depth - 1, val + 1);
            node->n3 = makeTree(depth - 1, val + 1);
            node->n4 = makeTree(depth - 1, val + 1);
            return node;
        }
        case 5: {
            auto* node = AllocType(TreeNode5Value, alloc6, &TreeNode5Type);
            node->val = val;
            node->n1 = makeTree(depth - 1, val + 1);
            node->n2 = makeTree(depth - 1, val + 1);
            node->n3 = makeTree(depth - 1, val + 1);
            node->n4 = makeTree(depth - 1, val + 1);
            node->n5 = makeTree(depth - 1, val + 1);
            return node;
        }
        default:
            return nullptr;
    }
}

//
//TODO: 
//To keep -fno-rtti flag need to introduce maybe an enum storing
//type of each of these structs helping us not use dynamic casts here
//
std::string printtree(TreeNodeBase* node) {
    if (node == nullptr) {
        return "null";
    }

    std::string addr = "xx"; 
    std::string nodeStr = "[" + addr + ", " + std::to_string(node->val) + "]";

    std::string childStrs;
    if (auto* n1 = dynamic_cast<TreeNode1Value*>(node)) {
        childStrs += printtree(n1->n1);
    } 
    else if (auto* n2 = dynamic_cast<TreeNode2Value*>(node)) {
        childStrs += printtree(n2->n1) + ", " + printtree(n2->n2);
    } 
    else if (auto* n3 = dynamic_cast<TreeNode3Value*>(node)) {
        childStrs += printtree(n3->n1) + ", " + printtree(n3->n2) + ", " + printtree(n3->n3);
    } 
    else if (auto* n4 = dynamic_cast<TreeNode4Value*>(node)) {
        childStrs += printtree(n4->n1) + ", " + printtree(n4->n2) + ", " + printtree(n4->n3) + ", " + printtree(n4->n4);
    } 
    else if (auto* n5 = dynamic_cast<TreeNode5Value*>(node)) {
        childStrs += printtree(n5->n1) + ", " + printtree(n5->n2) + ", " + printtree(n5->n3) + ", " + printtree(n5->n4) + ", " + printtree(n5->n5);
    }

    return nodeStr + ", " + childStrs;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//Full tree of varrying depths
//
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), garray);

    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[5] = { &alloc2, &alloc3, &alloc4, &alloc5, &alloc6};
    gtl_info.initializeGC<5>(allocs);

    TreeNodeBase* t1 = makeTree(5, 4);

    auto t1_start = printtree(t1);
    collect();

    auto t1_end = printtree(t1);

    assert(t1_start == t1_end);
    return 0;
}