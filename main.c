#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h> //mmap

//each page is 4k 
#define PAGE_SIZE 4096

//each object is 2 double words (16 bytes)
#define OBJ_SIZE 16

#define NUM_OBJS (PAGE_SIZE / OBJ_SIZE)

typedef struct Page{
    uint8_t* page_start;
    int free_slots[NUM_OBJS];
} Page;

Page* create_page() {

}

void* allocate_obj() {

}

void free_obj() {

}

void destroy_page() {

}

int main(){
    
}
