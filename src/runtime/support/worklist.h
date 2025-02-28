#pragma once

#include "xalloc.h"

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

    WorkListSegment<T>* head_segment;
    T* head_max;

    WorkListSegment<T>* tail_segment;
    T* tail_max;

    void worklist_push_slow(T* obj) noexcept
    {
        WorkListSegment<T>* xseg = XAllocPageManager::g_page_manager.allocatePage<WorkListSegment<T>>();
        xseg->data = (T*)((char*)xseg + sizeof(WorkListSegment<T>));

        //Case when no pages have been linked
        xseg->next = nullptr;
        if(this->head_segment == nullptr && this->tail_segment == nullptr) {
            this->head_segment = xseg;
            this->head_max = XAllocPageManager::get_max_for_segment(xseg);

            this->tail_segment = xseg;
            this->tail_max = XAllocPageManager::get_max_for_segment(xseg);

            this->tail = xseg->data;
            this->head = xseg->data;
        }
        else {
            if(this->tail_segment != nullptr) {
                this->tail_segment->next = xseg;
            }
            this->tail_segment = xseg;
            this->tail_max = XAllocPageManager::get_max_for_segment(xseg);

            this->tail = xseg->data;
        }

        *(this->tail) = obj;
    }

    void* worklist_pop_slow() noexcept
    {
#ifdef BSQ_GC_CHECK_ENABLED
        assert(this->head != nullptr);
#endif
        void* res = *(this->head);

        //Only segment, reset everything
        if(this->head_segment->next == nullptr) {
            this->head_segment = nullptr;
            this->head_max = nullptr;

            this->tail_segment = nullptr;
            this->tail_max = nullptr;
            
            this->head = nullptr;
            this->tail = nullptr;
        }
        else {
            WorkListSegment* xseg = this->head_segment;
            this->head_segment = this->head_segment->next;
            this->head_max = XAllocPageManager::get_max_for_segment(this->head_segment);
            this->head = XAllocPageManager::get_min_for_segment(this->head_segment);
            XAllocPageManager::g_page_manager.freePage<WorkListSegment<T>>(xseg);
        }

        return res;
    }

public:
    WorkList() noexcept : head(nullptr), tail(nullptr), head_segment(nullptr), tail_segment(nullptr) {}

    const inline isEmpty() noexcept
    {
        return this->head == nullptr || this->tail == nullptr;
    }

    inline void push(T* obj) noexcept
    {
        if(this->tail != nullptr && this->tail < this->tail_max) { 
            *(++this->tail) = obj; 
        } 
        else { 
            this->worklist_push_slow(obj);
        }
    }

    inline T* pop() noexcept
    {
        if(this->head != this->tail) {
            return *(this->head++);
        }
        else {
            this->worklist_pop_slow();
        }
    }
};
