#include "common.h"

mtx_t g_lock;

size_t GlobalThreadAllocInfo::s_thread_counter = 0;
void* GlobalThreadAllocInfo::s_current_page_address = ALLOC_BASE_ADDRESS;
