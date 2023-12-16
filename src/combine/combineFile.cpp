#include <iostream>
#include <dirent.h>
#include <sys/stat.h>

#include "constant.h"
#include "needle.h"
#include "helper.h"

int main() {
    char path2file[BUFFER_SIZE], path2indexFile[BUFFER_SIZE], path2dataFile[BUFFER_SIZE];
    char buf[BUFFER_SIZE];
    sprintf(path2file, "%s/%s", PATH2PDIR, OPDIR);
    sprintf(path2indexFile, "%s/%s/%s", PATH2PDIR, OPDIR, INDEXFILE);
    sprintf(path2dataFile, "%s/%s/%s", PATH2PDIR, OPDIR, DATAFILE);

    uint64_t small_file_num = 0;
    FILE *index_file = fopen(path2indexFile, "wb+");
    if(index_file == NULL) {
        print_error("Error for index file %s\n", path2indexFile);
        return -1;
    }
    // 假设文件都是新创建的，追加暂不考虑
    // size_t read_bytes = fread(&small_file_num, sizeof(uint64_t), 1, index_file);
    // // 文件新创建则先写入 0 占位
    // if(read_bytes < sizeof(uint64_t)) {
    //     fwrite(&small_file_num, sizeof(uint64_t), 1, index_file);
    // }
    fwrite(&small_file_num, sizeof(uint64_t), 1, index_file);
    FILE *data_file = fopen(path2dataFile, "wb+");
    if(data_file == NULL) {
        fclose(index_file);
        print_error("Error for data file %s\n", path2dataFile);
        return -1;
    }
    
    DIR *dir = opendir(path2file);
    struct dirent *entry;
    struct stat file_info;
    struct needle_index needle;

    // 1.查找小文件
    // 2.更新索引文件
    // 3.合并到大文件中
    while((entry = readdir(dir)) != 0) {
        
        sprintf(path2file, "%s/%s/%s", PATH2PDIR, OPDIR, entry->d_name);
        int flag = lstat(path2file, &file_info);
        
        if(flag < 0) {
            fclose(index_file);
            fclose(data_file);
            print_error("Error for stat %s\n", entry->d_name);
            return -1;
        }
        
        // 跳过目录、索引文件和大文件
        if(S_ISDIR(file_info.st_mode) 
        || strcmp(entry->d_name, INDEXFILE) == 0 
        || strcmp(entry->d_name, DATAFILE) == 0) continue;

        // 插入索引文件
        set_needle_index(&needle, &file_info, entry, ftell(data_file));
        insert_needle_index(&needle, index_file);

        // 插入数据文件
        int cnt = 0;
        FILE *small_file = fopen(path2file, "r");
        if(small_file == NULL) {
            fclose(index_file);
            fclose(data_file);
            print_error("Error on open %s\n", path2file);
            return -1;
        }
        do {
            int read_bytes = std::min(file_info.st_size - cnt, (long)BUFFER_SIZE);
            fread(buf, 1, read_bytes, small_file);
            fwrite(buf, 1, read_bytes, data_file);
            cnt += read_bytes;
        }
        while(cnt < file_info.st_size);
        fclose(small_file);
        ++small_file_num;
        
        flag = remove(path2file);
        if(flag < 0) {
            fclose(index_file);
            fclose(data_file);
            print_error("Error on remove %s\n", path2file);
            return 1;
        }
    }

    // 将小文件数量记录在索引文件中
    fseek(index_file, 0, SEEK_SET);
    fwrite(&small_file_num, sizeof(uint64_t), 1, index_file);
    COUT_THIS("Small file num: " << small_file_num);

    closedir(dir);
    
    fclose(index_file);
    fclose(data_file);
    return 0;
}