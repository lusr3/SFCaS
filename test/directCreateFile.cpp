#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <constant.h>
#include <needle.h>
#include <debug.h>

#define min(a, b) a < b ? a : b

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
    // for(int i = 0; i < createNum; ++i) {
    //     printf("%ld ", shuffle_files_id[i]);
    // }
    // printf("\n");
    char path2indexFile[1024], path2dataFile[1024];
    sprintf(path2indexFile, "%s/%s/%s%ld", PATH2PDIR, BACKDIR, INDEXFILE, createNum);
    sprintf(path2dataFile, "%s/%s/%s%ld", PATH2PDIR, BACKDIR, DATAFILE, createNum);

    long small_file_num = 0;
    FILE *index_file = fopen(path2indexFile, "wb+");
    if(index_file == NULL) {
        print_error("Error for index file %s\n", path2indexFile);
        return 1;
    }
    size_t read_bytes = fread(&small_file_num, sizeof(long), 1, index_file);
    // 文件新创建则先写入 0 占位
    if(read_bytes < sizeof(long)) {
        fwrite(&small_file_num, sizeof(long), 1, index_file);
    }
    FILE *data_file = fopen(path2dataFile, "wb+");
    if(data_file == NULL) {
        print_error("Error for data file %s\n", path2dataFile);
        return 1;
    }

    struct needle_index needle;
    struct stat file_info;
    struct dirent entry;
    for(long i = 0; i < createNum; ++i) {
        sprintf(entry.d_name, "%s%0*ld%s", FILEPREFIX, FILE_ID_LEN, shuffle_files_id[i], FILESUFFIX);
        sprintf(msg, "%s from %ld\n", "hello", shuffle_files_id[i]);
        file_info.st_size = strlen(msg);
        set_needle_index(&needle, &file_info, &entry, ftell(data_file));
        insert_needle_index(index_file, &needle);

        // 插入数据文件
        int cnt = 0;
        do {
            int read_bytes = min(file_info.st_size - cnt, BUFFER_SIZE);
            fwrite(msg, 1, read_bytes, data_file);
            cnt += read_bytes;
        }
        while(cnt < file_info.st_size);
        ++small_file_num;
    }
    // 将小文件数量记录在索引文件中
    fseek(index_file, 0, SEEK_SET);
    fwrite(&small_file_num, sizeof(long), 1, index_file);

    fclose(index_file);
    fclose(data_file);
    return 0;
}