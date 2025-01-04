#include <stdio.h>
#include <stdlib.h> //free
#include <assert.h>
#include <stdint.h> //uint8_t
#include <sys/mman.h> //mmap

#define DEBUG_LOG

//each page is 4k 
#define PAGE_SIZE 4096

//actual data in our page
typedef struct Block{
    struct Block* next;
    void* data;
    int is_free;
}Block;

//our big chunk of contiguous memory
typedef struct Page{
    struct Page* next;
    struct Block* block_list_head;
} Page;

Page* allocate_page() {
    //we create our page with 4096 size blocks allowing read and writes,
    //private and anon mapping since we dont need to worry about inter
    //process comms for now, and fd offset 0 for simplicity
    Page* p_ptr = (Page*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE ,
            MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    assert(p_ptr != MAP_FAILED);

    //pointer to first element of block list
    p_ptr->block_list_head = (Block*)((char*)p_ptr + sizeof(Page));

    printf("SIZE OF BLOCK %i\n", sizeof(Block));
    int num_blocks = PAGE_SIZE / sizeof(Block);

    //initialize our page
    Block* cur_block = p_ptr->block_list_head;
    for(int i = 0; i < num_blocks - 1; i++){
        cur_block->next = (Block*)((char*)cur_block + sizeof(Block)); 
        cur_block->is_free = 1;
        cur_block = cur_block->next;
    }
    //last block in the page
    cur_block->next = NULL; 
    cur_block->is_free = 1;

#ifdef DEBUG_LOG
    printf("Created page at: %p\n", p_ptr);
    printf("Block List starts at: %p\n", p_ptr->block_list_head);
#endif
    return p_ptr;
}

void* allocate_block(Page* p) {
    Block* cur = p->block_list_head; //ptr to head of block list
    while(cur){
        if(cur->is_free){
            cur->is_free = 0;
#ifdef DEBUG_LOG
    printf("Allocated block at address: %p\n", cur);
#endif
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

void free_block(Page* p, void* obj) {
    Block* block = (Block*)obj;
    block->is_free = 1;
#ifdef DEBUG_LOG
    printf("Freed block at: %p\n", block);
#endif
}

void destroy_page(Page* p) {
#ifdef DEBUG_LOG
    printf("Destroyed page: %p\n", p);
#endif
    munmap(p, PAGE_SIZE);
}

int main(){
    Page* page = allocate_page();
   
    //for now our blocks contain no data, allocate_block just allows us
    //to know that something was placed in mem here (is_free = 0)
    void* object_one = allocate_block(page);
    void* object_two = allocate_block(page);

    //if either are null we should make a new page - fine for now
    if(object_one != NULL && object_two != NULL){
        free_block(page, object_one);
        free_block(page, object_two);
    }

    destroy_page(page);
    return 0;
}
