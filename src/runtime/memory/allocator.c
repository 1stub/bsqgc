#include "allocator.h"

static void initializePage(void* page, uint16_t entrysize) {
    PageInfo* pinfo = (PageInfo*)page;
    pinfo->freelist = (FreeListEntry*)((char*)page + sizeof(PageInfo));
    pinfo->entrysize = entrysize;
    pinfo->entrycount = (BSQ_BLOCK_ALLOCATION_SIZE - sizeof(PageInfo)) / (entrysize + sizeof(MetaData));
    pinfo->freecount = pinfo->entrycount;
    pinfo->pagestate = PageStateInfo_GroundState;

    FreeListEntry* current = pinfo->freelist;
    for(int i = 0; i < pinfo->entrycount - 1; i++) {
        current->next = (FreeListEntry*)((char*)current + entrysize + sizeof(MetaData));
        current = current->next;
    }
    current->next = NULL;
}

#if 0
#include <stdio.h>
#include <stdlib.h> //free
#include <stdint.h>
#include <assert.h>
#include <sys/mman.h> //mmap
#include <math.h>

#define DEBUG_LOG

//each page is 4k 
#define PAGE_SIZE 4096

//the whole block including header and metadata
#define BLOCK_SIZE 32

#define NUM_BLOCKS_PER_PAGE     PAGE_SIZE / BLOCK_SIZE

//actual data in our page, will be 24 bytes
typedef struct block_header{
    struct block_header* next;
    
    //pointer to data segment of this block
    void* data;
}block_header;

//our big chunk of contiguous memory
typedef struct Page{
    uint8_t free_list[NUM_BLOCKS_PER_PAGE];
    struct Page* next;
    struct block_header* block_list_head;
    struct block_header* block_list_tail;
} Page;

//Allocates a 4k page with NUM_BLOCKS_PER_PAGE supported number of blocks.
//All blocks are alligned to 32 bytes. Data about where a block can be allocated
//is stored in the free_list, to be used in fool_alloc(). We store a pointer
//to the beginning and end of our current list of blocks for usage with
//freeing blocks no longer in use and returing memory to the OS
Page* allocate_page() {
    //we create our page with 4096 + page header size size blocks allowing read and writes,
    //private and anon mapping since we dont need to worry about inter
    //process comms for now, and fd offset 0 for simplicity
    Page* p_ptr = (Page*)mmap(NULL, PAGE_SIZE + sizeof(Page), PROT_READ | PROT_WRITE ,
            MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    assert(p_ptr != MAP_FAILED);

    //pointer to first element of block list
    p_ptr->block_list_head = (block_header*)((char*)p_ptr + sizeof(Page));

    //tail is what we use to keep track of where we are in the page
    p_ptr->block_list_tail = p_ptr->block_list_head;

#ifdef DEBUG_LOG
    printf("Size of block_header %i\n", sizeof(block_header));
    printf("Number of bytes of data per block %i\n", (int)(BLOCK_SIZE - sizeof(block_header)));
#endif

    //initialize our page, all blocks to free
    for(int i = 0; i < NUM_BLOCKS_PER_PAGE; i++){
        p_ptr->free_list[i] = 1;
    }

#ifdef DEBUG_LOG
    printf("Created page at: %p\n", p_ptr);
    printf("Block list starts at: %p\n\n", p_ptr->block_list_head);
#endif

    return p_ptr;
}


void* fool_alloc(Page* p, size_t size) {
    if(((char*)p->block_list_head + (int)size) - (char*)(p->block_list_head) > PAGE_SIZE){

#ifdef DEBUG_LOG
    printf("Not enough free space to allocate given size\n\n");
#endif 
        return NULL;
    }
    if(!size){

#ifdef DEBUG_LOG
    printf("Invalid size attempted to be allocated\n\n");
#endif 
        return NULL;
    }

    block_header* head = p->block_list_tail; //ptr to the last block allocd
    int n_blocks = (size + (BLOCK_SIZE - sizeof(block_header)) - 1) 
        / (BLOCK_SIZE - sizeof(block_header)); //round up to nearest block size

#ifdef DEBUG_LOG
    printf("For size %i data needed %i blocks\n", (int)size, n_blocks);
#endif
    
    //First we need to make sure there is enough contiguous space to allocate our blocks
    int contig_free = 0; 
    int start_index = -1;
    for(int i = 0; i < NUM_BLOCKS_PER_PAGE; i++){
        if(p->free_list[i]){
            //set our starting index to the first element we find that is free
            if(contig_free == 0){
                start_index = i; 
            }

            contig_free++;
            if(contig_free == n_blocks){
                break;
            }
        }else{
            contig_free = 0;
        }
    } 

    if(contig_free < n_blocks){

#ifdef DEBUG_LOG
    printf("Unable to find enough contiguous blocks to be allocated\n\n");
#endif
        return NULL;
    }

    //now that we have found a chunk of space fitting our size we can actually
    //link the blocks together
    void* data_bp = 
        (char*)p->block_list_head + (NUM_BLOCKS_PER_PAGE * sizeof(block_header));
    block_header* last_block;
    block_header* first_block;
    for(int i = 0; i < n_blocks; i++){
        p->free_list[start_index + i] = 0;
        block_header* b = (block_header*)((char*)head + ((start_index + i) * BLOCK_SIZE));
        
        if(i == 0){
            first_block = b;
        
        }
        b->data = (char*)data_bp + (i * (BLOCK_SIZE - sizeof(block_header)));
        b->next = (block_header*)((char*)b + sizeof(block_header));

#ifdef DEBUG_LOG
    printf("Allocated block header starting at %p\n", b);
    printf("Allocated block data segment starting at %p\n", b->data);
#endif

        p->block_list_tail = b;

        if(i == (n_blocks - 1)){
            last_block = b;
        }
    }

    p->block_list_tail = last_block->next;

#ifdef DEBUG_LOG
    printf("FINSHED ALLOCATING OBJECT\n\n");
#endif

    return (void*)(first_block->data);
}


void fool_free(Page* p, void* obj) {
    //we will be passed a pointer to the data segment of our block
    //this will need to be converted back into block in the header 
    //portion of our page

    /*
     
    int offset = (char*)obj - (char*)(p->block_list_head);
    int start_index = offset / BLOCK_SIZE;

    //how many blocks are associated with given object
    int allocated_blocks = ((block_header*)obj)->size / BLOCK_SIZE;
    
    while(allocated_blocks){
        p->free_list[start_index + allocated_blocks] = 1;
        allocated_blocks--;
    }

    */
#ifdef DEBUG_LOG
   // printf("Freed block at: %p\n", block);
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
   
    //we are trying to allocate one 64 byte chunk and another
    //128 byte chunk
    //then 8192 byte block which will fail (for now) since we 
    //exceede a page size
    void* object_one = fool_alloc(page, 64);
    void* object_two = fool_alloc(page, 128);

    void* failed_alloc = fool_alloc(page, 8192);

    int* data = (int*)object_one;
    *data = 0x42;

    printf("Data stored at object one: %i\n", *data);

    //if either are null we should make a new page - fine for now
    if(object_one != NULL && object_two != NULL){
        fool_free(page, object_one);
        fool_free(page, object_two);
    }

    destroy_page(page);
    return 0;
}
#endif