#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <format>
#include <stack>
#include <iostream>

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

uint64_t find_size_bytes(TreeNodeValue* n) 
{
    if(n == nullptr) {
        return 0;
    }

    return TreeNodeType.type_size + 
           find_size_bytes(n->left) + 
           find_size_bytes(n->right);
}

TreeNodeValue* garray[3] = {nullptr, nullptr, nullptr};

//
//Full tree of varrying depths
//A possible improvement could be making tree for each depth up to a certain threshold (say n=14)
//
int main(int argc, char** argv) {
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;
    gtl_info.disable_stack_refs_for_tests = true;

    GCAllocator* allocs[1] = { &alloc3 };
    gtl_info.initializeGC<1>(allocs);

    const int depth = 15;
    const int iterations = 100;
    int failed_iterations = 0;

    std::cout << "Starting " << iterations << " iterations of GC stress testing for multiple_tree_deep...\n";

    for (int i = 0; i < iterations; i++) {
        TreeNodeValue* tree_root = makeTree(depth, 4);
        garray[0] = tree_root;

        uint64_t init_total_bytes = find_size_bytes(tree_root);

        auto t1_start = printtree(tree_root);
        collect();
        
        auto t1_end = printtree(tree_root);

        assert(t1_start == t1_end);

        uint64_t final = gtl_info.total_live_bytes;
        assert(init_total_bytes == final);

        garray[0] = nullptr;

        //Clear out pending decs
        while(gtl_info.total_live_bytes != 0) {
            collect();
        }

        if(gtl_info.total_live_bytes != 0) {
            std::cerr << "Iteration " << i << " failed: incorrect live bytes\n";
            failed_iterations++;
            continue;
        }
    }
    std::cout << "collection time " << gtl_info.compute_average_time(gtl_info.collection_times) << " ms\n";
    std::cout << "marking time " << gtl_info.compute_average_time(gtl_info.marking_times) << " ms\n";
    std::cout << "evacuation time " << gtl_info.compute_average_time(gtl_info.evacuation_times) << " ms\n";
    std::cout << "decrement time " << gtl_info.compute_average_time(gtl_info.decrement_times) << " ms\n";

    std::cout << "Failed iterations: " << failed_iterations << "/" << iterations << "\n";
    return failed_iterations > 0 ? 1 : 0;

    return 0;
}