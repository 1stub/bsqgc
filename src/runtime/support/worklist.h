#pragma once

#include "xalloc.h"

#ifdef DSA_INVARIANTS
#define DSA_INVARIANT_CHECK(x) assert(x)
#else
#define DSA_INVARIANT_CHECK(x)
#endif

template <typename T>
struct WorkListSegment
{
    T* data;
    WorkListSegment* next;
};

template <typename T>
class WorkList
{
private:
    T* head;
    T* tail;

    T* head_max;
    T* tail_max;

    WorkListSegment<T>* head_segment;
    WorkListSegment<T>* tail_segment;

    void worklist_enqueue_slow(T* obj) noexcept
    {
        WorkListSegment<T>* xseg = (WorkListSegment<T>*)XAllocPageManager::g_page_manager.allocatePage<WorkListSegment<T>>();
        xseg->data = (T*)((uint8_t*)xseg + sizeof(WorkListSegment<T>));
        xseg->next = nullptr;

        this->tail_segment->next = xseg;

        this->tail_segment = xseg;
        this->tail = xseg->data;
        this->tail_max = (T*)((uint8_t*)xseg + BSQ_BLOCK_ALLOCATION_SIZE);

        *(this->tail++) = obj;
    }

    void* worklist_dequeue_slow() noexcept
    {
        void* res = *(this->head++);

        //Only segment, reset everything
        if(this->head_segment->next == nullptr) {
            this->head_segment = nullptr;
            this->head = nullptr;
            this->head_max = nullptr;

            this->tail_segment = nullptr;
            this->tail = nullptr;
            this->tail_max = nullptr;
        }
        else {
            WorkListSegment* xseg = this->head_segment;

            this->head_segment = this->head_segment->next;
            this->head = this->head_segment->data;
            this->head_max = (T*)((uint8_t*)this->head_segment + BSQ_BLOCK_ALLOCATION_SIZE);

            XAllocPageManager::g_page_manager.freePage<WorkListSegment<T>>(xseg);
        }

        return res;
    }

public:
    WorkList() noexcept : head(nullptr), tail(nullptr), head_max(nullptr), tail_max(nullptr), head_segment(nullptr), tail_segment(nullptr) {}

    void initialize() noexcept
    {
        DSA_INVARIANT_CHECK(this->invariant());
        DSA_INVARIANT_CHECK(this->head == nullptr);

        WorkListSegment<T>* xseg = (WorkListSegment<T>*)XAllocPageManager::g_page_manager.allocatePage<WorkListSegment<T>>();
        xseg->data = (T*)((uint8_t*)xseg + sizeof(WorkListSegment<T>));
        xseg->next = nullptr;

        //Empty case and we need to set head too
        this->head_segment = xseg;
        this->head = xseg->data;
        this->head_max = (T*)((uint8_t*)xseg + BSQ_BLOCK_ALLOCATION_SIZE);

        this->tail_segment = xseg;
        this->tail = xseg->data;
        this->tail_max = (T*)((uint8_t*)xseg + BSQ_BLOCK_ALLOCATION_SIZE);
    }

    void clear() noexcept
    {
        DSA_INVARIANT_CHECK(this->invariant());

        while(this->head_segment != nullptr) {
            WorkListSegment<T>* xseg = this->head_segment;
            this->head_segment = this->head_segment->next;

            XAllocPageManager::g_page_manager.freePage<WorkListSegment<T>>(xseg);
        }

        this->head_segment = nullptr;
        this->head = nullptr;
        this->head_max = nullptr;

        this->tail_segment = nullptr;
        this->tail = nullptr;
        this->tail_max = nullptr;
    }

    const inline isEmpty() noexcept
    {
        return this->head == this->tail;
    }

    inline void enqueue(T* obj) noexcept
    {
        DSA_INVARIANT_CHECK(this->invariant());

        if(this->tail < this->tail_max) [[likely]] { 
            *(this->tail++) = obj; 
        } 
        else [[unlikely]] { 
            this->worklist_enqueue_slow(obj);
        }
    }

    inline T* dequeue() noexcept
    {
        DSA_INVARIANT_CHECK(this->invariant());
        DSA_INVARIANT_CHECK(this->head != this->tail);

        if(this->head < this->head_max) [[likely]] {
            return *(this->head++);
        }
        else [[unlikely]] {
            this->worklist_dequeue_slow();
        }
    }
};
