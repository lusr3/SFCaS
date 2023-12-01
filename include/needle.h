#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <constant.h>
#include <debug.h>
#include <vector>

#if !defined(NEEDLE_H)
#define NEEDLE_H

// index 文件中记录每个小文件的相关信息
struct needle_index {
    char filename[MAX_FILE_LEN + 1];
    unsigned long offset;
    // 在 index 文件中该小文件元数据占据的大小
    unsigned int neddle_size;
    unsigned int size; 
    unsigned char flags;
    bool operator<(const struct needle_index &other) {
        // printf("cmp %d %d\n", strlen(filename), strlen(other.filename));
        return memcmp(filename, other.filename, MAX_FILE_LEN + 1) < 0;
    }
    needle_index() {
        memset(filename, 0, MAX_FILE_LEN + 1);
    }
};

struct needle_index_list {
    std::vector<struct needle_index> indexs;
    int is_cached = 0;
    struct needle_index *cached_item = nullptr;
    long size = 0;
    FILE *data_file = nullptr;
};

// 利用小文件信息和大文件中的偏移填充 needle_index
void set_needle_index(struct needle_index *needle, struct stat *file_info, struct dirent *entry, long offset);

// 从指定的索引文件中读取一个 needle_index
void read_needle_index(struct needle_index *needle, FILE *index_file);

// 向指定的索引文件中插入一个 needle_index
void insert_needle_index(FILE *index_file, struct needle_index *needle);



#endif