BIN_DIR := ./bin
TEST_DIR := ./test
SRC_DIR := ./src
AUX_DIR := $(SRC_DIR)/aux
SINDEX_DIR := $(SRC_DIR)/sindex
INCLUDE_DIR := ./include
SINDEX_INCLUDE_DIR := ./include/sindex

# MKL 库
MKL_INCLUDE_DIR := /opt/intel/oneapi/mkl/2023.2.0/include
MKL_LIB_DIR := /opt/intel/oneapi/mkl/2023.2.0/lib/intel64

FUSE_ENV := `pkg-config fuse3 --cflags --libs`
CXX_FLAGS := -fmax-errors=5 -faligned-new -march=native -mtune=native
NDEBUG_FLAG := -DNDEBUGGING
LIBS := -lpthread -lmkl_rt

# work directory
CUR_DIR := $(shell pwd)
OP_DIR := $(CUR_DIR)/testDir
MOUNT_DIR := $(CUR_DIR)/mountDir

AUX_CPP = $(wildcard $(AUX_DIR)/*.cpp)
SINDEX_SRC_CPP = $(wildcard $(SINDEX_DIR)/*.cpp)

MAIN_INCLUDE := -I $(INCLUDE_DIR) -I $(SINDEX_INCLUDE_DIR) -I $(MKL_INCLUDE_DIR)
MAIN_FLAGS := $(CXX_FLAGS) $(NDEBUG_FLAG)

CXX := g++

$(shell if [ ! -d $(CUR_DIR)/bin ]; then mkdir -p $(CUR_DIR)/bin; fi)
$(shell if [ ! -d $(OP_DIR) ]; then mkdir -p $(OP_DIR); fi)
$(shell if [ ! -d $(MOUNT_DIR) ]; then mkdir -p $(MOUNT_DIR); fi)

build:$(BIN_DIR)/sfcas $(BIN_DIR)/combineFile $(BIN_DIR)/readFile $(BIN_DIR)/createFile $(BIN_DIR)/directCreateFile

# main program
run:$(BIN_DIR)/sfcas
	$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

stop:
	umount $(MOUNT_DIR)

$(BIN_DIR)/sfcas:$(SINDEX_SRC_CPP) $(AUX_CPP) $(SRC_DIR)/sfcas.cpp
	@$(CXX) -Wall $^ -o $@ $(FUSE_ENV) $(MAIN_INCLUDE) -L $(MKL_LIB_DIR) $(MAIN_FLAGS) $(LIBS)

$(BIN_DIR)/combineFile:$(SRC_DIR)/combine/combineFile.cpp $(AUX_DIR)/needle.cpp
	@$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

# debug for DEBUG
$(BIN_DIR)/sfcas_debug:$(SINDEX_SRC_CPP) $(AUX_CPP) $(SRC_DIR)/sfcas.cpp
	@$(CXX) -Wall $^ -o $@ $(FUSE_ENV) $(MAIN_INCLUDE) -L $(MKL_LIB_DIR) $(CXX_FLAGS) $(LIBS)

debug:$(BIN_DIR)/sfcas_debug
	@$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

# test program
$(BIN_DIR)/readFile:$(TEST_DIR)/readFile.cpp
	@$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/createFile:$(TEST_DIR)/createFile.cpp
	@$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/directCreateFile:$(TEST_DIR)/directCreateFile.cpp $(AUX_DIR)/needle.cpp
	@$(CXX) -Wall -o $@ $^ -I $(INCLUDE_DIR) 

test:$(BIN_DIR)/readFile
	$^

combine:$(BIN_DIR)/combineFile
	$^

create:$(BIN_DIR)/createFile
	$^

dcreate:$(BIN_DIR)/directCreateFile
	@if [ ! -d $(CUR_DIR)/back ]; then \
		mkdir -p $(CUR_DIR)/back; \
	fi
	$^

clean:
	rm $(BIN_DIR)/* 

clear:
	rm $(OP_DIR)/*