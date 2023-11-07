BIN_DIR:=./bin
TEST_DIR:=./test
UTILS_DIR:=./utils
SRC_DIR:=./src
INCLUDE_DIR:=./include

OP_DIR:=/home/lusr/SFCaS/testDir
MOUNT_DIR:=/home/lusr/SFCaS/mountDir

UTILS_SRC = $(wildcard $(UTILS_DIR)/*.c)

# build:$(BIN_DIR)/myread $(BIN_DIR)/readFile

$(BIN_DIR)/myread:$(SRC_DIR)/myread.c $(UTILS_SRC)
	gcc -Wall $^ -o $@ `pkg-config fuse3 --cflags --libs` -I $(INCLUDE_DIR)

$(BIN_DIR)/readFile:$(TEST_DIR)/readFile.c $(UTILS_DIR)/debug.c
	gcc -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/combineFile:$(TEST_DIR)/combineFile.c $(UTILS_SRC)
	gcc -Wall -o $@ $^ -I $(INCLUDE_DIR)

$(BIN_DIR)/createFile:$(TEST_DIR)/createFile.c $(UTILS_DIR)/debug.c
	gcc -Wall -o $@ $^ -I $(INCLUDE_DIR)

test:$(BIN_DIR)/readFile
	$^

combine:$(BIN_DIR)/combineFile
	$^

create:$(BIN_DIR)/createFile
	$^

run:$(BIN_DIR)/myread
	$^ -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

debug:$(BIN_DIR)/myread
	$^ -d -f -o modules=subdir,subdir=$(OP_DIR) $(MOUNT_DIR)

clean:
	rm $(BIN_DIR)/* 

clear:
	rm $(OP_DIR)/*