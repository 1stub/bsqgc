#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>
#include <stack>

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

//
//Made this non-recursive to avoid tons of stack frames when we call
//a collection
//
TreeNodeValue* makeTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr; 
    }

    std::stack<std::pair<TreeNodeValue*, int64_t>> stack;

    TreeNodeValue* root = AllocType(TreeNodeValue, alloc3, &TreeNodeType);

    root->left = nullptr;
    root->right = nullptr;
    root->val = val;

    stack.push({root, depth});
    while (!stack.empty()) {
        auto [node, depth] = stack.top();
        stack.pop();

        if (depth > 0) {
            //left child
            node->left = AllocType(TreeNodeValue, alloc3, &TreeNodeType);
            node->left->left = nullptr;
            node->left->right = nullptr;
            node->left->val = val;
            stack.push({node->left, depth - 1});

            //right child
            node->right = AllocType(TreeNodeValue, alloc3, &TreeNodeType);
            node->right->left = nullptr;
            node->right->right = nullptr;
            node->right->val = val;
            stack.push({node->right, depth - 1});
        }
    }

    return root;
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

TreeNodeValue* garray[3] = {nullptr, nullptr, nullptr};

//
//Full tree of varrying depths
//A possible improvement could be making tree for each depth up to a certain threshold (say n=14)
//

//TODO: figure out why we are off by exactly one treenode type size in our final calculation
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;

    GCAllocator* allocs[1] = { &alloc3 };
    gtl_info.initializeGC<1>(allocs);

    int depth = 15;
    TreeNodeValue* tree_root = makeTree(depth, 4);
    garray[0] = tree_root;

    uint64_t init_total_bytes = ((1 << (depth + 1)) - 1) * TreeNodeType.type_size;

    auto t1_start = printtree(tree_root);
    collect();
    
    auto t1_end = printtree(tree_root);

    assert(t1_start == t1_end);

    uint64_t final = gtl_info.total_live_bytes;
    assert(init_total_bytes == final);

    gtl_info.disable_stack_refs_for_tests = true;
    garray[0] = nullptr;

    //big tree, so collect a few times to clear up pending decs
    collect();
    collect();
    collect();
    collect();
    collect();
    collect();

    assert(gtl_info.total_live_bytes == 0);

    return 0;
}