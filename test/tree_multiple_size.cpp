#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <random>
#include <string>
#include <format>

struct TypeInfoBase TreeNode1Type = {
    .type_id = 1,
    .type_size = 24,
    .slot_size = 3,
    .ptr_mask = "100",  
    .typekey = "TreeNode1Type"
};

struct TypeInfoBase TreeNode2Type = {
    .type_id = 2,
    .type_size = 32,
    .slot_size = 4,
    .ptr_mask = "1100",  
    .typekey = "TreeNode2Type"
};

struct TypeInfoBase TreeNode3Type = {
    .type_id = 3,
    .type_size = 40,
    .slot_size = 5,
    .ptr_mask = "11100",  
    .typekey = "TreeNode3Type"
};

struct TypeInfoBase TreeNode4Type = {
    .type_id = 4,
    .type_size = 48,
    .slot_size = 6,
    .ptr_mask = "111100",  
    .typekey = "TreeNode4Type"
};

struct TypeInfoBase TreeNode5Type = {
    .type_id = 5,
    .type_size = 56,
    .slot_size = 7,
    .ptr_mask = "1111100",  
    .typekey = "TreeNode5Type"
};

enum class TreeNodeType {
    TreeNode1,
    TreeNode2,
    TreeNode3,
    TreeNode4,
    TreeNode5
};

struct TreeNode1Value{
    void* n1 = nullptr;
    int64_t val = 0;
    TreeNodeType type = TreeNodeType::TreeNode1;
};

struct TreeNode2Value {
    void* n1 = nullptr;
    void* n2 = nullptr;
    int64_t val = 0;
    TreeNodeType type = TreeNodeType::TreeNode2;
};

struct TreeNode3Value {
    void* n1 = nullptr;
    void* n2 = nullptr;
    void* n3 = nullptr;
    int64_t val = 0;
    TreeNodeType type = TreeNodeType::TreeNode3;
};

struct TreeNode4Value {
    void* n1 = nullptr;
    void* n2 = nullptr;
    void* n3 = nullptr;
    void* n4 = nullptr;
    int64_t val = 0;
    TreeNodeType type = TreeNodeType::TreeNode4;
};

struct TreeNode5Value {
    void* n1 = nullptr;
    void* n2 = nullptr;
    void* n3 = nullptr;
    void* n4 = nullptr;
    void* n5 = nullptr;
    int64_t val = 0;
    TreeNodeType type = TreeNodeType::TreeNode5;
};

GCAllocator alloc3(24, REAL_ENTRY_SIZE(24), collect);
GCAllocator alloc4(32, REAL_ENTRY_SIZE(32), collect);
GCAllocator alloc5(40, REAL_ENTRY_SIZE(40), collect);
GCAllocator alloc6(48, REAL_ENTRY_SIZE(48), collect);
GCAllocator alloc7(56, REAL_ENTRY_SIZE(56), collect);

std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> type_dist(1, 5); // Randomly choose between TreeNode1Type to TreeNode5Type

void* makeTree(int64_t type, int64_t depth, int64_t val) {
    if (depth < 0 || type > 5) {
        return nullptr;
    }

    switch (type) {
        case 1: {
            TreeNode1Value* node = AllocType(TreeNode1Value, alloc3, &TreeNode1Type);
            node->val = val;
            node->n1 = makeTree(type + 1, depth - 1, val + 1);
            return node;
        }
        case 2: {
            TreeNode2Value* node = AllocType(TreeNode2Value, alloc4, &TreeNode2Type);
            node->val = val;
            node->n1 = makeTree(type + 1, depth - 1, val + 1);
            node->n2 = makeTree(type + 1, depth - 1, val + 1);
            return node;
        }
        case 3: {
            TreeNode3Value* node = AllocType(TreeNode3Value, alloc5, &TreeNode3Type);
            node->val = val;
            node->n1 = makeTree(type + 1, depth - 1, val + 1);
            node->n2 = makeTree(type + 1, depth - 1, val + 1);
            node->n3 = makeTree(type + 1, depth - 1, val + 1);
            return node;
        }
        case 4: {
            TreeNode4Value* node = AllocType(TreeNode4Value, alloc6, &TreeNode4Type);
            node->val = val;
            node->n1 = makeTree(type + 1, depth - 1, val + 1);
            node->n2 = makeTree(type + 1, depth - 1, val + 1);
            node->n3 = makeTree(type + 1, depth - 1, val + 1);
            node->n4 = makeTree(type + 1, depth - 1, val + 1);
            return node;
        }
        case 5: {
            TreeNode5Value* node = AllocType(TreeNode5Value, alloc7, &TreeNode5Type);
            node->val = val;
            node->n1 = makeTree(type + 1, depth - 1, val + 1);
            node->n2 = makeTree(type + 1, depth - 1, val + 1);
            node->n3 = makeTree(type + 1, depth - 1, val + 1);
            node->n4 = makeTree(type + 1, depth - 1, val + 1);
            node->n5 = makeTree(type + 1, depth - 1, val + 1);
            return node;
        }
        default:
            return nullptr;
    }
}

std::string printtree(void* node) {
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
        childStrs += printtree(n1->n1);
    }
    else if (type == &TreeNode2Type) {
        TreeNode2Value* n2 = static_cast<TreeNode2Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n2->val) + "]";
        childStrs += printtree(n2->n1) + ", " + printtree(n2->n2);
    }
    else if (type == &TreeNode3Type) {
        TreeNode3Value* n3 = static_cast<TreeNode3Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n3->val) + "]";
        childStrs += printtree(n3->n1) + ", " + printtree(n3->n2) + ", " + printtree(n3->n3);
    }
    else if (type == &TreeNode4Type) {
        TreeNode4Value* n4 = static_cast<TreeNode4Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n4->val) + "]";
        childStrs += printtree(n4->n1) + ", " + printtree(n4->n2) + ", " + printtree(n4->n3) + ", " + printtree(n4->n4);
    }
    else if (type == &TreeNode5Type) {
        TreeNode5Value* n5 = static_cast<TreeNode5Value*>(node);
        nodeStr = "[" + addr + ", " + std::to_string(n5->val) + "]";
        childStrs += printtree(n5->n1) + ", " + printtree(n5->n2) + ", " + printtree(n5->n3) + ", " + printtree(n5->n4) + ", " + printtree(n5->n5);
    }
    else {
        return "null";
    }

    return nodeStr + ", " + childStrs;
}

void* garray[3] = {nullptr, nullptr, nullptr};

//
//The main purpose of this test is to be able to ensure 
//the collector can handle having multiple allocators.
//
//This creates a tree in a sort of funnel shape where each layer's
//children nodes differ from the previous
//
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;

    GCAllocator* allocs[5] = { &alloc3, &alloc4, &alloc5, &alloc6, &alloc7 };
    gtl_info.initializeGC<5>(allocs);

    int depth = 5;
    void* t = makeTree(1, depth, 0);
    garray[0] = t;

    uint64_t expected_bytes = 0;
    uint64_t nodes_at_level = 1;
    for (int level = 0; level <= depth; level++) {
        TypeInfoBase* current_type;
        int children_per_node;
        
        switch(level + 1) { //+1 because our types start with one
            case 1: current_type = &TreeNode1Type; children_per_node = 1; break;
            case 2: current_type = &TreeNode2Type; children_per_node = 2; break;
            case 3: current_type = &TreeNode3Type; children_per_node = 3; break;
            case 4: current_type = &TreeNode4Type; children_per_node = 4; break;
            case 5: current_type = &TreeNode5Type; children_per_node = 5; break;
            default: continue;
        }
        
        expected_bytes += nodes_at_level * current_type->type_size;
        
        if (level < depth) {
            nodes_at_level *= children_per_node; 
        }
    }

    uint64_t init_total_bytes = expected_bytes;

    auto t1_start = printtree(t);
    collect();

    auto t1_end = printtree(t);

    assert(t1_start == t1_end);

    uint64_t final = gtl_info.total_live_bytes;
    assert(init_total_bytes == final);

    gtl_info.disable_stack_refs_for_tests = true;
    garray[0] = nullptr;
    collect();

    assert(gtl_info.total_live_bytes == 0);

    return 0;
}