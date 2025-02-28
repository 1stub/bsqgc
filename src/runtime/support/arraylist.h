#pragma once

#include "xalloc.h"

#ifdef DSA_INVARIANTS
#define DSA_INVARIANT_CHECK(x) assert(x)
#else
#define DSA_INVARIANT_CHECK(x)
#endif

template <typename T>
struct ArrayListSegment {
    T* data;
    ArrayListSegment* next;
    ArrayListSegment* prev;
};

template <typename T>
class ArrayList {
private:
    T* head; //either head == tail (and empty) OR head is first valid element in list
    T* tail; //tail is always FIRST invalid element off end of list

    T* head_min; //first valid data element in head_segment
    T* head_max; //one past the last valid data element in head_segment

    T* tail_min; //first valid data element in tail_segment
    T* tail_max; //one past the last valid data element in tail_segment

    ArrayListSegment<T>* head_segment;
    ArrayListSegment<T>* tail_segment;   
    
    void push_front_slow(T*)
    {

    }

    void push_back_slow(T*)
    {
        
    }

    T* pop_front_slow()
    {
        
    }

    T* pop_back_slow()
    {
        
    }

public:
    ArrayList() noexcept : head(nullptr), tail(nullptr), head_min(nullptr), head_max(nullptr), tail_min(nullptr), tail_max(nullptr), head_segment(nullptr), tail_segment(nullptr) {}

    bool invariant() noexcept
    {
        if(this->head_segment == nullptr || this->head_min == nullptr || this->head_max == nullptr || this->head == nullptr) {
            return false;
        }
        if(this->tail_segment == nullptr || this->tail_min == nullptr || this->tail_max == nullptr || this->tail == nullptr) {
            return false;
        }

        //head should be in the head segment
        if(this->head < this->head_min || this->head >= this->head_max) {
            return false;
        }

        //tail should be in the tail segment
        if(this->tail < this->tail_min || this->tail >= this->tail_max) {
            return false;
        }

        //head should be before tail (only check if segments are same)
        if(this->head_segment == this->tail_segment && this->head > this->tail) {
            return false;
        }

        //no stray segments
        if(this->head_segment->prev != nullptr || this->tail_segment->next != nullptr) {
            return false;
        }

        //empty list should degenerate to a single segment
        if(this->head == this->tail) {
            return this->head_segment == this->tail_segment;
        }
        
        return true;
    }

    void initialize() noexcept
    {
        DSA_INVARIANT_CHECK(this->head == nullptr && this->tail == nullptr && this->head_segment == nullptr && this->tail_segment == nullptr);

        ArrayListSegment<T>* xseg = (ArrayListSegment<T>*)XAllocPageManager::g_page_manager.allocatePage();
        xseg->data = (T*)((uint8_t*)xseg + sizeof(ArrayListSegment<T>));
        xseg->next = nullptr;
        xseg->prev = nullptr;

        //Empty case and we need to set head too
        this->head_segment = xseg;
        this->head_min = xseg->data;
        this->head_max = (T*)((uint8_t*)xseg + BSQ_BLOCK_ALLOCATION_SIZE);
        this->head = xseg->data;

        this->tail_segment = xseg;
        this->tail_min = xseg->data;
        this->tail_max = (T*)((uint8_t*)xseg + BSQ_BLOCK_ALLOCATION_SIZE);
        this->tail = xseg->data;

        DSA_INVARIANT_CHECK(this->invariant());
    }

    void clear() noexcept
    {
        DSA_INVARIANT_CHECK(this->invariant());

        while(this->head_segment != nullptr) {
            ArrayListSegment<T>* xseg = this->head_segment;
            this->head_segment = this->head_segment->next;

            XAllocPageManager::g_page_manager.freePage(xseg);
        }

        this->head_segment = nullptr;
        this->head_min = nullptr;
        this->head_max = nullptr;
        this->head = nullptr;

        this->tail_segment = nullptr;
        this->tail_min = nullptr;
        this->tail_max = nullptr;
        this->tail = nullptr;
    }

    inline bool isEmpty() const noexcept
    {
        return this->head == this->tail;
    }

    void push_front(T*)
    {
        DSA_INVARIANT_CHECK(this->invariant());

        if(this->head_min < this->head) [[likely]] {
            *(--this->head) = obj;
        }
        else [[unlikely]] {
            this->push_front_slow(obj);
        }
    }

    void push_back_slow(T*)
    {
        DSA_INVARIANT_CHECK(this->invariant());

        if(this->tail < this->tail_max) [[likely]] {
            *(this->tail++) = obj;
        }
        else [[unlikely]] {
            this->push_back_slow(obj);
        }
    }

    T* pop_front_slow()
    {
        DSA_INVARIANT_CHECK(this->head != this->tail);
        DSA_INVARIANT_CHECK(this->invariant());

        if(this->head < this->head_max) [[likely]] {
            return *(this->head++);
        }
        else [[unlikely]] {
            this->pop_front_slow();
        }
    }

    T* pop_back_slow()
    {
        DSA_INVARIANT_CHECK(this->head != this->tail);
        DSA_INVARIANT_CHECK(this->invariant());
    }
};

#define arraylist_push_head(L, O) if((L).head && (L).head > GET_MIN_FOR_SEGMENT((L).head, AL_SEG_SIZE)) {*(--(L).head) = O; } else {arraylist_push_head_slow(&(L), O);}
#define arraylist_push_tail(L, O) if((L).tail && (L).tail < GET_MAX_FOR_SEGMENT((L).tail, AL_SEG_SIZE)) { *(++(L).tail) = O; } else {arraylist_push_tail_slow(&(L), O);}

#define arraylist_pop_head(T, L) ((T*)((L).head != GET_MAX_FOR_SEGMENT((L).head, AL_SEG_SIZE) ? *((L).head++) : arraylist_pop_head_slow(&(L))))
#define arraylist_pop_tail(T, L) ((T*)((L).tail != GET_MIN_FOR_SEGMENT((L).tail, AL_SEG_SIZE) ? *((L).tail--) : arraylist_pop_tail_slow(&(L))))

