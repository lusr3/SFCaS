#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

#include "constant.h"
#include "needle.h"
#include "helper.h"

int main() {
    long createNum = 10;
    char msg[BUFFER_SIZE];
    scanf("%ld", &createNum);

    long *shuffle_files_id = (long*) malloc(sizeof(long) * createNum);
    for(int i = 0; i < createNum; ++i) {
        shuffle_files_id[i] = i;
    }
    // shuffle
    for(int i = 0; i < createNum; ++i) {
        int a = rand() % createNum, b = rand() % createNum;
        int temp = shuffle_files_id[a];
        shuffle_files_id[a] = shuffle_files_id[b];
        shuffle_files_id[b] = temp;
    }
    
    char path2indexFile[BUFFER_SIZE], path2dataFile[BUFFER_SIZE];
    sprintf(path2indexFile, "%s/%s/%s%ld", PATH2PDIR, BACKDIR, INDEXFILE, createNum);
    sprintf(path2dataFile, "%s/%s/%s%ld", PATH2PDIR, BACKDIR, BIGFILE, createNum);

    FILE *index_file = fopen(path2indexFile, "wb");
    if(index_file == NULL) {
        print_error("Error for index file %s\n", path2indexFile);
        return -1;
    }
    fwrite(&createNum, sizeof(long), 1, index_file);

    FILE *data_file = fopen(path2dataFile, "wb");
    if(data_file == NULL) {
        fclose(index_file);
        print_error("Error for data file %s\n", path2dataFile);
        return -1;
    }

    struct needle_index needle;
    struct stat file_info;
    struct dirent entry;
    for(long i = 0; i < createNum; ++i) {
        // 创建文件名和内容
        sprintf(entry.d_name, "%s%0*ld%s", FILEPREFIX, FILE_ID_LEN, shuffle_files_id[i], FILESUFFIX);
        sprintf(msg, "%s from %ld\n", "hello", shuffle_files_id[i]);
        file_info.st_size = strlen(msg);
        set_needle_index(&needle, &file_info, &entry, ftell(data_file));
        
        // 写入索引文件
        insert_needle_index(&needle, index_file);

        // 插入数据文件
        int cnt = 0;
        do {
            int read_bytes = std::min(file_info.st_size - cnt, (long)BUFFER_SIZE);
            fwrite(msg, 1, read_bytes, data_file);
            cnt += read_bytes;
        }
        while(cnt < file_info.st_size);
    }

    fclose(index_file);
    fclose(data_file);
    return 0;
}