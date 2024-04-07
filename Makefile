CUR_DIR := $(shell pwd)
OP_DIR := $(CUR_DIR)/testDir
MOUNT_DIR := $(CUR_DIR)/mountDir
CLIENT_DIR := $(CUR_DIR)/clientDir
CLIENT_MOUNT_DIR := $(CUR_DIR)/clientMountDir
BIN_DIR := ./bin
BUILDL_DIR := ./build
PROTO_DIR := ./src/proto
GRPC_DIR := ./src/grpc

AUX_CPP = $(wildcard ./src/aux/*.cpp)
SINDEX_SRC_CPP = $(wildcard ./src/sindex/*.cpp)
MKL_LIB_DIR := /sharenvme/usershome/lusr/spack/opt/spack/linux-ubuntu22.04-icelake/gcc-11.4.0/intel-oneapi-mkl-2024.0.0-ywegtlj4lv65ftghgngsyepdu24uetxq/mkl/2024.0/lib/intel64
MKL_INCLUDE_DIR = /sharenvme/usershome/lusr/spack/opt/spack/linux-ubuntu22.04-icelake/gcc-11.4.0/intel-oneapi-mkl-2024.0.0-ywegtlj4lv65ftghgngsyepdu24uetxq/mkl/2024.0/include

FUSE_ENV := `pkg-config fuse3 --cflags --libs`
CXX_FLAGS := -fmax-errors=5 -faligned-new -march=native -mtune=native
# NDEBUG_FLAG := -DNDEBUGGING
LIBS := -lpthread -lmkl_rt

MAIN_INCLUDE := -I ./include -I ./include/sindex -I $(MKL_INCLUDE_DIR)
MAIN_FLAGS := $(CXX_FLAGS)

.PHONY: build run stop test combine create dcreate clean clear
build:
	@if [ ! -d $(CUR_DIR)/build ]; then \
		mkdir -p $(CUR_DIR)/build; \
	fi
	@cd $(BUILDL_DIR) && cmake .. && make && cd ..

proto:$(PROTO_DIR)/file_access.proto $(PROTO_DIR)/health_check.proto
	@protoc --grpc_out $(GRPC_DIR) --cpp_out $(GRPC_DIR) \
	-I $(PROTO_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $(PROTO_DIR)/file_access.proto
	@protoc --grpc_out $(GRPC_DIR) --cpp_out $(GRPC_DIR) \
	-I $(PROTO_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $(PROTO_DIR)/health_check.proto

# main program
sfcas:$(SINDEX_SRC_CPP) $(AUX_CPP) ./src/sfcas.cpp
	@g++ -Wall $^ -o ./bin/sfcas $(FUSE_ENV) $(MAIN_INCLUDE) -L $(MKL_LIB_DIR) $(MAIN_FLAGS) $(LIBS)

run:$(BIN_DIR)/sfcas
	$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

stop:
	umount $(MOUNT_DIR)

# dfs
nameserver:$(BIN_DIR)/nameserver
	$^

dataserver:$(BIN_DIR)/dataserver
	$^

client:$(BIN_DIR)/client
	$^ -f -o modules=subdir,subdir=$(CLIENT_DIR) $(CLIENT_MOUNT_DIR)

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
ifndef BIN_DIR
	@echo "Directory for BIN_DIR is not defined."
else
ifeq ($(strip $(BIN_DIR)), "")
	@echo "Directory for BIN_DIR is defined but empty."
else
	rm -r $(BIN_DIR)/*
endif
endif
ifndef BUILDL_DIR
	@echo "Directory for BUILDL_DIR is not defined."
else
ifeq ($(strip $(BUILDL_DIR)), "")
	@echo "Directory for BUILDL_DIR is defined but empty."
else
	rm -r $(BUILDL_DIR)/*
endif
endif

clear:
ifndef OP_DIR
	@echo "Directory for OP_DIR is not defined."
else
ifeq ($(strip $(OP_DIR)), "")
	@echo "Directory for OP_DIR is defined but empty."
else
	rm -r $(OP_DIR)/*
endif
endif