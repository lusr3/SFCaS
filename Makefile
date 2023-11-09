BIN_DIR := ./bin
TEST_DIR := ./test
UTILS_DIR := ./utils
SRC_DIR := ./src
INCLUDE_DIR := ./include

OP_DIR := ./testDir
MOUNT_DIR := ./mountDir

UTILS_SRC = $(wildcard $(UTILS_DIR)/*.c)

CC := gcc

build:$(BIN_DIR)/myread $(BIN_DIR)/combineFile $(BIN_DIR)/readFile $(BIN_DIR)/createFile $(BIN_DIR)/directCreateFile
	@if [ ! -d $(OP_DIR) ]; then \
		mkdir -p $(OP_DIR); \
	fi
	@if [ ! -d $(MOUNT_DIR) ]; then \
		mkdir -p $(MOUNT_DIR); \
	fi

$(BIN_DIR)/myread:$(SRC_DIR)/myread.c $(UTILS_SRC)
	$(CC) -Wall $^ -o $@ `pkg-config fuse3 --cflags --libs` -I $(INCLUDE_DIR)

$(BIN_DIR)/readFile:$(TEST_DIR)/readFile.c $(UTILS_DIR)/debug.c
	$(CC) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/combineFile:$(SRC_DIR)/combineFile.c $(UTILS_SRC)
	$(CC) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/createFile:$(TEST_DIR)/createFile.c $(UTILS_DIR)/debug.c
	$(CC) -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/directCreateFile:$(TEST_DIR)/directCreateFile.c $(UTILS_SRC)
	$(CC) -Wall -o $@ $^ -I $(INCLUDE_DIR)

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