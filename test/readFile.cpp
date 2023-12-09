#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <constant.h>
#include <debug.h>
#include <sys/time.h>

int test_for_one_file() {
    char buf[BUFFER_SIZE];
    long file_id = 0;
    printf("Look up for file id:");
    scanf("%ld", &file_id);
    sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, MOUNTDIR, FILEPREFIX, FILE_ID_LEN, file_id, FILESUFFIX);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    FILE *fp = fopen(buf, "rb");
    if(fp == NULL) {
        print_error("Error on open file %s\n", buf);
        return 1;
    }
    // fseek(fp, 4098, SEEK_SET);
    int size = fread(buf, 1, 20, fp);
    if(size < 1024) buf[size] = '\0';
    printf("size: %d buf: %s\n", size, buf);
    
    fclose(fp);
    gettimeofday(&end, NULL);
    long timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec-start.tv_usec;
    printf("Cost time: %ldus\n", timeuse);
    return 0;
}

int test_for_many_files() {
    char buf[BUFFER_SIZE];
    long start_id = 0, test_num = 0, MOD = 0;
    printf("Start look up files from:");
    scanf("%ld", &start_id);
    printf("Test case num and max file num is:");
    scanf("%ld %ld", &test_num, &MOD);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    while(test_num--) {
        // scanf("%ld", &file_id);
        sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, MOUNTDIR, FILEPREFIX, FILE_ID_LEN, start_id, FILESUFFIX);
        FILE *fp = fopen(buf, "rb");
        if(fp == NULL) {
            print_error("Error on open file %s\n", buf);
            return 1;
        }
        // fseek(fp, 4098, SEEK_SET);
        int size = fread(buf, 1, 20, fp);
        if(size == 0) {
            print_error("File %s not exist\n", buf);
        }
        // if(size < 1024) buf[size] = '\0';
        // printf("size: %d buf: %s\n", size, buf);
        
        fclose(fp);
        start_id = (start_id + 1) % MOD;
    }
    gettimeofday(&end, NULL);
    long timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec-start.tv_usec;
    printf("Cost time: %ldus\n", timeuse);
    return 0;
}

// hahah

int main() {
    int test_case = 0;
    printf("Test for one file(0) or range test(1):");
    scanf("%d", &test_case);
    if(!test_case) {
        return test_for_one_file();
    }
    test_for_many_files();
}