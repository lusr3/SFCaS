OP_DIR := ./testDir
MOUNT_DIR := ./mountDir
BIN_DIR := ./bin
BUDILL_DIR := ./build

.PHONY: build run stop test combine create dcreate clean clear
build:
	@cd $(BUDILL_DIR) && cmake .. && make && cd ..

# main program
run:$(BIN_DIR)/sfcas
	$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

stop:
	umount $(MOUNT_DIR)

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
ifndef BUDILL_DIR
	@echo "Directory for BUDILL_DIR is not defined."
else
ifeq ($(strip $(BUDILL_DIR)), "")
	@echo "Directory for BUDILL_DIR is defined but empty."
else
	rm -r $(BUDILL_DIR)/*
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