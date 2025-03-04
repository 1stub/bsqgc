#pragma once

#include "threadinfo.h"

//This methods drives the collection routine -- uses the thread local information from invoking thread to get pages 
extern void collect() noexcept;
