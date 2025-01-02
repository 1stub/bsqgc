#include <stdio.h>
#include <assert.h>
#include <stdint.h> //uint8_t
#include <sys/mman.h> //mmap

#define DEBUG_LOG

//each page is 4k 
#define PAGE_SIZE 4096

//each object is 2 double words (32 bytes) - assuming a word to be 16 bytes
#define OBJ_SIZE 32

#define NUM_OBJS (PAGE_SIZE / OBJ_SIZE)

typedef struct Page{
    uint8_t* page_start;

    //effectively a boolean array that we use to check if a slot in our page
    //is used or not
    int free_slots[NUM_OBJS];
} Page;

Page* create_page() {
    Page* p_ptr = NULL;
    
    //we create our page with 4096 size blocks allowing read and writes,
    //private and anon mapping since we dont need to worry about inter
    //process comms for now, and fd offset 0 for simplicity
    p_ptr = (Page*)mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE ,
            MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    assert(p_ptr != MAP_FAILED);

    //we set the start of our page to AFTER the pages meta data
    p_ptr->page_start = (uint8_t*)p_ptr + sizeof(Page);
#ifdef DEBUG_LOG
    printf("Created page at address: 0x%x\n", p_ptr->page_start);
#endif
    return p_ptr;
}

void allocate_obj(Page* p) {
    static uint32_t obj = 0x0000; //our 32 byte object of irrelevent data
    uint8_t* data = p->page_start;

    //we find closest to base ptr available memory 
    for(int i = 0; i < NUM_OBJS; i++){
        if(p->free_slots[i] == 0){
            //instead of indexing p as an array, we create a block manually
            //to keep consistent offsets
            //this might not be necessary?
            uint32_t* block = (uint32_t*)(data + (i * OBJ_SIZE));
            *block = obj;
#ifdef DEBUG_LOG
    printf("Created block at address: 0x%x\n", block);
    printf("Data at said block: 0x%x\n", *block);
#endif
            p->free_slots[i] = 1;
            obj++;
            break;
        }
    }
}

void free_obj() {

}

void destroy_page() {

}

int main(){
    Page* test_page = create_page();
    
    //allocate two objects (incremental data) to our page
    allocate_obj(test_page);
    allocate_obj(test_page);

    return 0;
}
