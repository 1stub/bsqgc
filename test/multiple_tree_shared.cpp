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

    const int depth = 11;
    const int iterations = 1000;
    int failed_iterations = 0;

    std::cout << "Starting " << iterations << " iterations of GC stress testing...\n";
    auto test_start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; i++) {
        auto iter_start = std::chrono::high_resolution_clock::now();

        // Create big tree and keep a subtree alive
        TreeNode3Value* root1 = makeSharedTree(depth, 2);
        garray[0] = root1;
        TreeNode1Value* root2 = AllocType(TreeNode1Value, alloc2, &TreeNode1Type);
        root2->val = 2;
        garray[1] = root2;
        root2->n1 = root1->n1->n1;

        // Drop root1 and collect
        garray[0] = nullptr;
        for (int j = 0; j < 6; j++) {
            collect();
        }

        // Verify kept subtree is intact
        uint64_t subtree_size = find_size_bytes(root2->n1);
        uint64_t expected_size = subtree_size + TreeNode1Type.type_size;
        if (gtl_info.total_live_bytes != expected_size) {
            std::cerr << "Iteration " << i << " failed: incorrect live bytes\n";
            failed_iterations++;
            continue;
        }

        // Drop everything and collect
        garray[1] = nullptr;
        for (int j = 0; j < 6; j++) {
            collect();
        }

        if (gtl_info.total_live_bytes != 0) {
            std::cerr << "Iteration " << i << " failed: memory not fully collected\n";
            failed_iterations++;
        }

        auto iter_end = std::chrono::high_resolution_clock::now();
        auto iter_time = std::chrono::duration_cast<std::chrono::milliseconds>(iter_end - iter_start).count();
        if (iter_time > 100) {
            std::cout << "Iteration " << i << " took " << iter_time << "ms\n";
        }
    }

    //Didn't do these calculations in other tests but fun to see
    auto test_end = std::chrono::high_resolution_clock::now();
    auto total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(test_end - test_start).count();
    double total_time_seconds = total_time_ms / 1000.0;

    std::cout << "\nTest completed in " << std::fixed << std::setprecision(3) << total_time_seconds << " seconds\n";

    std::cout << "Failed iterations: " << failed_iterations << "/" << iterations << "\n";
    return failed_iterations > 0 ? 1 : 0;
}