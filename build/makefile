MAKE_PATH=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR=$(MAKE_PATH)/
BIN_DIR=$(MAKE_PATH)/../bin/
SRC_DIR=$(MAKE_PATH)/../src/

RUNTIME_DIR=$(SRC_DIR)runtime/
SUPPORT_DIR=$(SRC_DIR)runtime/support/
MEMORY_DIR=$(SRC_DIR)runtime/memory/

OUT_EXE=$(BUILD_DIR)output/
OUT_OBJ=$(BUILD_DIR)output/obj/

TEST_DIR=$(MAKE_PATH)/../test/
MEMORY_TEST_DIR=$(MAKE_PATH)/../test/memory/

LANGUAGE_DIR=$(MAKE_PATH)/../src/language/

#dev is default, for another flavor : make BUILD=release or debug
BUILD := dev

CC=g++
CSTDFLAGS=-Wall -Wextra -Wno-unused-parameter -Wuninitialized -Werror -std=gnu++20 -fno-exceptions -fno-rtti -fno-strict-aliasing -fno-omit-frame-pointer -fno-stack-protector

CFLAGS_OPT.dev=-O0 -g -ggdb 
CFLAGS_OPT.release=-O3 -march=x86-64-v3
CFLAGS=${CFLAGS_OPT.${BUILD}} ${CSTDFLAGS}
CFLAGS_TEST=${CFLAGS_OPT.dev} ${CSTDFLAGS}

SUPPORT_HEADERS=$(SUPPORT_DIR)threadinfo.h $(RUNTIME_DIR)common.h $(SUPPORT_DIR)xalloc.h  $(SUPPORT_DIR)stack.h $(SUPPORT_DIR)arraylist.h $(SUPPORT_DIR)worklist.h $(SUPPORT_DIR)pagetable.h
SUPPORT_SOURCES=$(SUPPORT_DIR)threadinfo.cpp $(RUNTIME_DIR)common.cpp $(SUPPORT_DIR)xalloc.cpp  $(SUPPORT_DIR)stack.cpp $(SUPPORT_DIR)arraylist.cpp $(SUPPORT_DIR)worklist.cpp $(SUPPORT_DIR)pagetable.cpp
SUPPORT_OBJS=$(OUT_OBJ)threadinfo.o $(OUT_OBJ)common.o $(OUT_OBJ)xalloc.o $(OUT_OBJ)stack.o $(OUT_OBJ)arraylist.o $(OUT_OBJ)worklist.o $(OUT_OBJ)pagetable.o

MEMORY_HEADERS=$(RUNTIME_DIR)common.h $(MEMORY_DIR)allocator.h $(MEMORY_DIR)gc.h
MEMORY_SOURCES=$(MEMORY_DIR)allocator.cpp $(MEMORY_DIR)gc.cpp
MEMORY_OBJS=$(OUT_OBJ)allocator.o $(OUT_OBJ)gc.o 

all: $(OUT_EXE)memex

test: $(SUPPORT_HEADERS) $(MEMORY_HEADERS) $(SUPPORT_OBJS) $(MEMORY_OBJS)
	@if [ -z "$(TEST)" ]; then \
		echo "Error: Please specify a test file with TEST=<filename>."; \
		exit 1; \
	fi
	@mkdir -p $(OUT_EXE)
	$(CC) $(CFLAGS_TEST) -o $(OUT_EXE)$(TEST) $(SUPPORT_OBJS) $(MEMORY_OBJS) $(TEST_DIR)$(TEST).cpp
	@echo "Running test: $(TEST)"
	@$(OUT_EXE)$(TEST)

$(OUT_EXE)memex: $(SUPPORT_HEADERS) $(MEMORY_HEADERS) $(SUPPORT_OBJS) $(MEMORY_OBJS) $(MEMORY_TEST_DIR)/memex.cpp
	@mkdir -p $(OUT_EXE)
	$(CC) $(CFLAGS) -o $(OUT_EXE)memex $(SUPPORT_OBJS) $(MEMORY_OBJS) $(MEMORY_TEST_DIR)/memex.cpp

$(OUT_OBJ)allocator.o: $(MEMORY_HEADERS) $(MEMORY_DIR)allocator.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)allocator.o -c $(MEMORY_DIR)allocator.cpp

$(OUT_OBJ)gc.o: $(MEMORY_HEADERS) $(MEMORY_DIR)gc.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)gc.o -c $(MEMORY_DIR)gc.cpp

$(OUT_OBJ)stack.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)stack.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)stack.o -c $(SUPPORT_DIR)stack.cpp

$(OUT_OBJ)arraylist.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)arraylist.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)arraylist.o -c $(SUPPORT_DIR)arraylist.cpp

$(OUT_OBJ)threadinfo.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)threadinfo.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)threadinfo.o -c $(SUPPORT_DIR)threadinfo.cpp

$(OUT_OBJ)worklist.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)worklist.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)worklist.o -c $(SUPPORT_DIR)worklist.cpp

$(OUT_OBJ)xalloc.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)xalloc.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)xalloc.o -c $(SUPPORT_DIR)xalloc.cpp

$(OUT_OBJ)pagetable.o: $(SUPPORT_HEADERS) $(SUPPORT_DIR)pagetable.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)pagetable.o -c $(SUPPORT_DIR)pagetable.cpp

$(OUT_OBJ)common.o: $(SUPPORT_HEADERS) $(RUNTIME_DIR)common.cpp
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)common.o -c $(RUNTIME_DIR)common.cpp

clean:
	rm -rf $(OUT_EXE)* $(OUT_OBJ)*.o $(BIN_DIR)*
