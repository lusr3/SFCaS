CUR_DIR := $(shell pwd)
OP_DIR := $(CUR_DIR)/testDir
MOUNT_DIR := $(CUR_DIR)/mountDir
CLIENT_DIR := $(CUR_DIR)/clientDir
CLIENT_MOUNT_DIR := $(CUR_DIR)/clientMountDir
BIN_DIR := ./bin
BUILDL_DIR := ./build
PROTO_DIR := ./src/proto
GRPC_DIR := ./src/grpc

.PHONY: build run stop test combine create dcreate clean clear
build:
	@cd $(BUILDL_DIR) && cmake .. && make && cd ..

proto:$(PROTO_DIR)/file_access.proto $(PROTO_DIR)/health_check.proto
	@protoc --grpc_out $(GRPC_DIR) --cpp_out $(GRPC_DIR) \
	-I $(PROTO_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $(PROTO_DIR)/file_access.proto
	@protoc --grpc_out $(GRPC_DIR) --cpp_out $(GRPC_DIR) \
	-I $(PROTO_DIR) --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` $(PROTO_DIR)/health_check.proto

# main program
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