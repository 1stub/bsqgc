#include "../src/runtime/memory/gc.h"
#include "../src/runtime/memory/threadinfo.h"

#include <string>
#include <iostream>
#include <chrono>

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
//This test creates similar shared tree as in tree_shared
//but we create multiple as a sort of stress test
//for the collector
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

    const int depth = 10;
    const int iterations = 100;
    int failed_iterations = 0;

    //
    //If we want to have this run higher workloads (completly full collections)
    //we need to do depth=11 then call multiple collections instead of just one to 
    //properly clear our old tree. If we do not we collect further and further behind
    //schedule (1024 filled pages) and the collector falls apart.
    //

    std::cout << "Starting " << iterations << " iterations of GC stress testing for multiple_tree_shared...\n";

    for (int i = 0; i < iterations; i++) {
        // Create big tree and keep a subtree alive
        TreeNode3Value* root1 = makeSharedTree(depth, 2);
        garray[0] = root1;
        TreeNode1Value* root2 = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
        root2->val = 2;
        garray[1] = root2;
        root2->n1 = root1->n1->n1;

        auto root1_init = printtree(root1);
        auto root2_init = printtree(root2);

        // Drop root1 and collect
        garray[0] = nullptr;
        //std::cout << "filled_pages_count " << gtl_info.newly_filled_pages_count << std::endl;
        collect();

        auto root2_final = printtree(root2);

        // Verify kept subtree is intact
        uint64_t subtree_size = find_size_bytes(root2->n1);
        uint64_t expected_size = subtree_size + TreeNode1Type.type_size;
        if (gtl_info.total_live_bytes != expected_size) {
            std::cerr << "Iteration " << i << " failed: incorrect live bytes\n";
            failed_iterations++;
            continue;
        }

        if(root2_final != root2_init) {
            std::cerr << "Iteration " << i << " failed: subtree not intact after collection\n";
            failed_iterations++;
            continue;
        }

        // Drop everything and collect
        garray[1] = nullptr;
        collect();

        #if 0
        if (gtl_info.total_live_bytes != 0) {
            std::cerr << "Iteration " << i << " failed: memory not fully collected\n";
            failed_iterations++;
        }
        #endif
    }

    std::cout << "collection time " << gtl_info.compute_average_collection_time() << " ms\n";

    std::cout << "Failed iterations: " << failed_iterations << "/" << iterations << "\n";
    return failed_iterations > 0 ? 1 : 0;
}