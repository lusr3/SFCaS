#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <cstdint>

#include "constant.h"
#include "strkey.h"

#if !defined(NEEDLE_H)
#define NEEDLE_H

// index 文件中记录每个小文件的相关信息
struct needle_index {
    index_key_t filename;
    uint64_t offset;
    uint32_t size;
    // 在 index 文件中该小文件元数据占据的大小
    uint32_t neddle_size;
    uint8_t flags;

    bool operator<(const struct needle_index &other) {
        return this->filename < other.filename;
    }
};

struct needle_index_list {
    std::vector<needle_index> indexs;
    uint64_t index_num;
    FILE *data_file = nullptr;
    struct needle_index *cached_item = nullptr;
    bool is_cached = false;
};

// 利用小文件信息和大文件中的偏移填充 needle_index
void set_needle_index(struct needle_index *needle, struct stat *file_info, struct dirent *entry, uint64_t offset);

// 从指定的索引文件中读取一个 needle_index
void read_needle_index(struct needle_index *needle, FILE *index_file);

// 向指定的索引文件中插入一个 needle_index
void insert_needle_index(struct needle_index *needle, FILE *index_file);

#endif