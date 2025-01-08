MAKE_PATH=$(realpath $(dir $(lastword $(MAKEFILE_LIST))))
BUILD_DIR=$(MAKE_PATH)/
BIN_DIR=$(MAKE_PATH)/../bin/
SRC_DIR=$(MAKE_PATH)/../src/

MEMORY_DIR=$(SRC_DIR)runtime/memory/

OUT_EXE=$(BUILD_DIR)output/
OUT_OBJ=$(BUILD_DIR)output/obj/

TEST_DIR=$(MAKE_PATH)/../test/
MEMORY_TEST_DIR=$(MAKE_PATH)/../test/memory/

#dev is default, for another flavor : make BUILD=release or debug
BUILD := dev

CC=gcc
CSTDFLAGS=-Wall -Wextra -Wno-unused-parameter -Wuninitialized -Werror

CFLAGS_OPT.debug=-O0 -g -ggdb -fno-omit-frame-pointer
CFLAGS_OPT.dev=-O0 -g -ggdb -fno-omit-frame-pointer
CFLAGS_OPT.release=-O3 -fno-omit-frame-pointer -march=x86-64-v3
CFLAGS=${CFLAGS_OPT.${BUILD}} ${CSTDFLAGS}
CFLAGS_TEST=${CFLAGS_OPT.dev} ${CSTDFLAGS}

MEMORY_HEADERS=$(MEMORY_DIR)common.h
MEMORY_SOURCES=$(MEMORY_DIR)allocator.c
MEMORY_OBJS=$(OUT_OBJ)allocator.o

all: $(OUT_EXE)memex

$(OUT_EXE)memex: $(MEMORY_HEADERS) $(MEMORY_OBJS) $(MEMORY_TEST_DIR)/memex.c
	@mkdir -p $(OUT_EXE)
	$(CC) $(CFLAGS) -o $(OUT_EXE)memex $(MEMORY_OBJS) $(MEMORY_TEST_DIR)/memex.c

$(OUT_OBJ)allocator.o: $(MEMORY_HEADERS) $(MEMORY_DIR)allocator.c
	@mkdir -p $(OUT_OBJ)
	$(CC) $(CFLAGS) -o $(OUT_OBJ)allocator.o -c $(MEMORY_DIR)allocator.c

clean:
	rm -rf $(OUT_EXE)* $(OUT_OBJ)*.o $(BIN_DIR)*
