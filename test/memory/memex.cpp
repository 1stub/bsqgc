#include "../../src/runtime/memory/gc.h"

#include <string>
#include <format>

TypeInfoBase ListNodeType = {
    .type_id = 1,
    .type_size = 16,
    .slot_size = 2,
    .ptr_mask = "10",  
    .typekey = "ListNode"
};

GCAllocator alloc2(16, REAL_ENTRY_SIZE(16), collect);

struct ListNodeValue {
    ListNodeValue* next;
    int64_t val;
};

ListNodeValue* makeList(size_t length, int64_t val) {
    ListNodeValue* ll = nullptr;
    
    for(size_t i = 0; i < length; i++) {
        ListNodeValue* n = AllocType(ListNodeValue, alloc2, &ListNodeType);
        n->next = ll;
        n->val = val + (length - (i + 1));

        ll = n;
    }

    return ll;
}

std::string printlist(ListNodeValue* ll) {
    ListNodeValue* cur = ll;

    std::string rr = "";
    while(cur != nullptr) {
        std::string addr = std::format("{:x}", (uintptr_t)cur);
        rr = rr + "[" + addr + ", " + std::to_string(cur->val) + "] -> ";
        cur = cur->next;
    }

    return rr + "null";
}

int main(int argc, char** argv) {
    InitBSQMemoryTheadLocalInfo();

    GCAllocator* allocs[1] = { &alloc2 };
    gtl_info.initializeGC<1>(allocs);

    ListNodeValue* l1 = makeList(2, 5); //stays live
    makeList(1, 0); //dies

//    auto p1 = printlist(l1);

    collect();

//    auto p2 = printlist(l1);
    assert(*((int64_t*)l1 + 1) != 0);

    return 0;
}
