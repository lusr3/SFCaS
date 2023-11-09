#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <constant.h>
#include <debug.h>

// index 文件中记录每个小文件的相关信息
struct needle_index {
    char filename[MAX_FILE_LEN + 1];
    unsigned long offset;
    // 在 index 文件中该小文件元数据占据的大小
    unsigned int neddle_size;
    unsigned int size; 
    unsigned char flags;
};

struct needle_index_list {
    struct needle_index *indexs;
    int is_cached;
    struct needle_index *cached_item;
    long size;
    FILE *data_file;
};

// 利用小文件信息和大文件中的偏移填充 needle_index
void set_needle_index(struct needle_index *needle, struct stat *file_info, struct dirent *entry, long offset);

// 从指定的索引文件中读取一个 needle_index
void read_needle_index(struct needle_index *needle, FILE *index_file);

// 向指定的索引文件中插入一个 needle_index
void insert_needle_index(FILE *index_file, struct needle_index *needle);

// 装载 index 文件内容并得到大文件的文件指针
// 成功返回 index 数目，失败返回 -1
int init(struct needle_index_list *index_list);
void release_needle(struct needle_index_list *index_list);

// 从 index_list 中找到对应于 filename 的 needle_index
struct needle_index *find_index(struct needle_index_list *index_list, const char *filename);
