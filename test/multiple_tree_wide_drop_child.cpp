#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <chrono>
#include <iostream>

struct TypeInfoBase TreeNode30Type = {
    .type_id = 1,
    .type_size = 248,
    .slot_size = 31,
    .ptr_mask = "1111111111111111111111111111110",  
    .typekey = "TreeNode30Type"
};

struct TreeNodeValue {
    TreeNodeValue* n1;
    TreeNodeValue* n2;
    TreeNodeValue* n3;
    TreeNodeValue* n4;
    TreeNodeValue* n5;
    TreeNodeValue* n6;
    TreeNodeValue* n7;
    TreeNodeValue* n8;
    TreeNodeValue* n9;
    TreeNodeValue* n10;
    TreeNodeValue* n11;
    TreeNodeValue* n12;
    TreeNodeValue* n13;
    TreeNodeValue* n14;
    TreeNodeValue* n15;
    TreeNodeValue* n16;
    TreeNodeValue* n17;
    TreeNodeValue* n18;
    TreeNodeValue* n19;
    TreeNodeValue* n20;
    TreeNodeValue* n21;
    TreeNodeValue* n22;
    TreeNodeValue* n23;
    TreeNodeValue* n24;
    TreeNodeValue* n25;
    TreeNodeValue* n26;
    TreeNodeValue* n27;
    TreeNodeValue* n28;
    TreeNodeValue* n29;
    TreeNodeValue* n30;

    int64_t val;
};

GCAllocator alloc248(248, REAL_ENTRY_SIZE(248), collect);

//
//Make tree recursively (for now)
//
TreeNodeValue* makeTree(int64_t depth, int64_t val) {
    if (depth < 0) {
        return nullptr; 
    }

    TreeNodeValue* n = AllocType(TreeNodeValue, alloc248, &TreeNode30Type);
    n->val = val;

    n->n1 = makeTree(depth - 1, val + 1);
    n->n2 = makeTree(depth - 1, val + 1);
    n->n3 = makeTree(depth - 1, val + 1);
    n->n4 = makeTree(depth - 1, val + 1);
    n->n5 = makeTree(depth - 1, val + 1);
    n->n6 = makeTree(depth - 1, val + 1);
    n->n7 = makeTree(depth - 1, val + 1);
    n->n8 = makeTree(depth - 1, val + 1);
    n->n9 = makeTree(depth - 1, val + 1);
    n->n10 = makeTree(depth - 1, val + 1);
    n->n11 = makeTree(depth - 1, val + 1);
    n->n12 = makeTree(depth - 1, val + 1);
    n->n13 = makeTree(depth - 1, val + 1);
    n->n14 = makeTree(depth - 1, val + 1);
    n->n15 = makeTree(depth - 1, val + 1);
    n->n16 = makeTree(depth - 1, val + 1);
    n->n17 = makeTree(depth - 1, val + 1);
    n->n18 = makeTree(depth - 1, val + 1);
    n->n19 = makeTree(depth - 1, val + 1);
    n->n20 = makeTree(depth - 1, val + 1);
    n->n21 = makeTree(depth - 1, val + 1);
    n->n22 = makeTree(depth - 1, val + 1);
    n->n23 = makeTree(depth - 1, val + 1);
    n->n24 = makeTree(depth - 1, val + 1);
    n->n25 = makeTree(depth - 1, val + 1);
    n->n26 = makeTree(depth - 1, val + 1);
    n->n27 = makeTree(depth - 1, val + 1);
    n->n28 = makeTree(depth - 1, val + 1);
    n->n29 = makeTree(depth - 1, val + 1);
    n->n30 = makeTree(depth - 1, val + 1);

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
    childStrs += printtree(node->n3) + ", ";
    childStrs += printtree(node->n4) + ", ";
    childStrs += printtree(node->n5) + ", ";
    childStrs += printtree(node->n6) + ", ";
    childStrs += printtree(node->n7) + ", ";
    childStrs += printtree(node->n8) + ", ";
    childStrs += printtree(node->n9) + ", ";
    childStrs += printtree(node->n10) + ", ";
    childStrs += printtree(node->n11) + ", ";
    childStrs += printtree(node->n12) + ", ";
    childStrs += printtree(node->n13) + ", ";
    childStrs += printtree(node->n14) + ", ";
    childStrs += printtree(node->n15) + ", ";
    childStrs += printtree(node->n16) + ", ";
    childStrs += printtree(node->n17) + ", ";
    childStrs += printtree(node->n18) + ", ";
    childStrs += printtree(node->n19) + ", ";
    childStrs += printtree(node->n20) + ", ";
    childStrs += printtree(node->n21) + ", ";
    childStrs += printtree(node->n22) + ", ";
    childStrs += printtree(node->n23) + ", ";
    childStrs += printtree(node->n24) + ", ";
    childStrs += printtree(node->n25) + ", ";
    childStrs += printtree(node->n26) + ", ";
    childStrs += printtree(node->n27) + ", ";
    childStrs += printtree(node->n28) + ", ";
    childStrs += printtree(node->n29) + ", ";
    childStrs += printtree(node->n30);

    return nodeStr + ", " + childStrs;
}

TreeNodeValue* garray[3] = {nullptr, nullptr, nullptr};

//
//Purpose of this test is to create a very wide tree
//then drop some subtrees
//
int main(int argc, char **argv)
{
    INIT_LOCKS();
    GlobalDataStorage::g_global_data.initialize(sizeof(garray), (void**)garray);

    InitBSQMemoryTheadLocalInfo();
    gtl_info.disable_automatic_collections = true;
    gtl_info.disable_stack_refs_for_tests = true;

    GCAllocator* allocs[1] = { &alloc248 };
    gtl_info.initializeGC<1>(allocs);

    const int depth = 2;
    const int iterations = 1000;
    int failed_iterations = 0;

    std::cout << "Starting " << iterations << " iterations of GC stress testing for multiple_tree_wide_drop_child...\n";

    for (int i = 0; i < iterations; i++) {        
        TreeNodeValue* root = makeTree(depth, 2);
        garray[0] = root;

        root->n1 = nullptr;
        root->n3 = nullptr;
        root->n5 = nullptr;
        root->n7 = nullptr;
        root->n9 = nullptr;
        root->n11 = nullptr;
        root->n13 = nullptr;
        root->n15 = nullptr;
        root->n17 = nullptr;
        root->n19 = nullptr;
        root->n21 = nullptr;
        root->n23 = nullptr;
        root->n25 = nullptr;
        root->n27 = nullptr;
        root->n29 = nullptr;

        uint64_t init_total_bytes = ((1 + 15 + 15*30) * TreeNode30Type.type_size);

        auto root_init = printtree(garray[0]);
        auto root0_init = printtree(garray[1]);

        collect();

        auto root_final = printtree(garray[0]);
        auto root0_final = printtree(garray[1]);

        if(!((root_init == root_final) == (root0_init == root0_final))) {
            std::cerr << "Iteration " << i << " failed: subtree not intact after collection\n";
            failed_iterations++;
            continue;
        }

        if(init_total_bytes != gtl_info.total_live_bytes) {
            std::cerr << "Iteration " << i << " failed: incorrect live bytes\n";
            failed_iterations++;
            continue;
        }

        garray[0] = nullptr;
        collect();
        
        if (gtl_info.total_live_bytes != 0) {
            std::cerr << "Iteration " << i << " failed: memory not fully collected\n";
            failed_iterations++;
        }
    }

    std::cout << "collection time " << gtl_info.compute_average_time(gtl_info.collection_times) << " ms\n";
    std::cout << "marking time " << gtl_info.compute_average_time(gtl_info.marking_times) << " ms\n";
    std::cout << "evacuation time " << gtl_info.compute_average_time(gtl_info.evacuation_times) << " ms\n";
    std::cout << "decrement time " << gtl_info.compute_average_time(gtl_info.decrement_times) << " ms\n";
    std::cout << "Failed iterations: " << failed_iterations << "/" << iterations << "\n";
    
    return failed_iterations > 0 ? 1 : 0;
}