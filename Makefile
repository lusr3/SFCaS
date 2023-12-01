BIN_DIR := ./bin
TEST_DIR := ./test
UTILS_DIR := ./utils
SRC_DIR := ./src
AUX_DIR := $(SRC_DIR)/aux
INCLUDE_DIR := ./include
SINDEX_INCLUDE_DIR := ./include/sindex

FUSE_ENV := `pkg-config fuse3 --cflags --libs`

MKL_INCLUDE_DIR := /opt/intel/oneapi/mkl/2023.2.0/include
MKL_LIB_DIR := /opt/intel/oneapi/mkl/2023.2.0/lib/intel64
CXX_FLAGS := -fmax-errors=5 -faligned-new -march=native -mtune=native
DEBUG_FLAG := -DNDEBUGGING
LIBS := -lpthread -lmkl_rt

CUR_DIR := $(shell pwd)
OP_DIR := $(CUR_DIR)/testDir
MOUNT_DIR := $(CUR_DIR)/mountDir
SINDEX_DIR := $(SRC_DIR)/sindex

UTILS_SRC = $(wildcard $(UTILS_DIR)/*.cpp)
AUX_CPP = $(wildcard $(AUX_DIR)/*.cpp)
SINDEX_SRC_CPP = $(wildcard $(SINDEX_DIR)/*.cpp)

MAIN_INCLUDE := -I $(INCLUDE_DIR) -I $(SINDEX_INCLUDE_DIR) -I $(MKL_INCLUDE_DIR)
MAIN_FLAGS := $(CXX_FLAGS) $(DEBUG_FLAG)

CXX := g++

build:$(BIN_DIR)/myread $(BIN_DIR)/combineFile $(BIN_DIR)/readFile $(BIN_DIR)/createFile $(BIN_DIR)/directCreateFile
	@if [ ! -d $(OP_DIR) ]; then \
		mkdir -p $(OP_DIR); \
	fi
	@if [ ! -d $(MOUNT_DIR) ]; then \
		mkdir -p $(MOUNT_DIR); \
	fi

# main program
$(BIN_DIR)/myread:$(SINDEX_SRC_CPP) $(UTILS_SRC) $(AUX_CPP) $(SRC_DIR)/myread.cpp
	$(CXX) -Wall $^ -o $@ $(FUSE_ENV) $(MAIN_INCLUDE) -L $(MKL_LIB_DIR) $(MAIN_FLAGS) $(LIBS)

$(BIN_DIR)/combineFile:$(SRC_DIR)/combine/combineFile.cpp $(UTILS_SRC) $(AUX_DIR)/needle.cpp
	$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

# test program
$(BIN_DIR)/readFile:$(TEST_DIR)/readFile.cpp $(UTILS_SRC)
	$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/createFile:$(TEST_DIR)/createFile.cpp $(UTILS_SRC) $(AUX_DIR)/needle.cpp
	$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/directCreateFile:$(TEST_DIR)/directCreateFile.cpp $(UTILS_SRC) $(AUX_DIR)/needle.cpp
	$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR) 

test:$(BIN_DIR)/readFile
	$^

combine:$(BIN_DIR)/combineFile
	$^

create:$(BIN_DIR)/createFile
	$^

dcreate:$(BIN_DIR)/directCreateFile
	$^

run:$(BIN_DIR)/myread
	$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

debug:$(BIN_DIR)/myread
	$^ -d -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

clean:
	rm $(BIN_DIR)/* 

clear:
	rm $(OP_DIR)/*