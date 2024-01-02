#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#include <random>

#include "constant.h"
#include "helper.h"

int test_for_one_file() {
    char buf[BUFFER_SIZE + 7];
    uint64_t file_id = 0;
    printf("Look up for file id:");
    std::cin >> file_id;
    sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, MOUNTDIR, FILEPREFIX, FILE_ID_LEN, file_id, FILESUFFIX);

    struct timeval start, end;
    gettimeofday(&start, NULL);
    FILE *fp = fopen(buf, "rb");
    if(fp == NULL) {
        print_error("Error on open file %s\n", buf);
        return -1;
    }
    int size = fread(buf, 1, BUFFER_SIZE, fp);
    if(size < BUFFER_SIZE) buf[size] = '\0';
    printf("size: %d buf: %s\n", size, buf);
    
    fclose(fp);
    gettimeofday(&end, NULL);
    long timeuse = 1000000*(end.tv_sec - start.tv_sec) + end.tv_usec-start.tv_usec;
    printf("Cost time: %ldus\n", timeuse);
    return 0;
}

long test_for_many_files(unsigned int seed = 0, bool once = true, long num = 10, long MOD = 10) {
    srand(seed);
    char buf[BUFFER_SIZE + 7];

    long test_num = 0, test_mod = 0;
    if(once) {
        printf("Test case num and max file num is:");
        scanf("%ld %ld", &test_num, &test_mod);
        srand(test_num);
    }
    else {
        test_num = num;
        test_mod = MOD;
    }
    long array[test_num];
    for(long i = 0; i < test_num; ++i) {
        array[i] = rand() % test_mod;
    }

    struct timeval start, end;
    gettimeofday(&start, NULL);
    for(long i = 0; i < test_num; ++i) {
        sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, MOUNTDIR, FILEPREFIX, FILE_ID_LEN, array[i], FILESUFFIX);
        FILE *fp = fopen(buf, "rb");
        if(fp == NULL) {
            print_error("Error on open file %s\n", buf);
            return -1;
        }
        int size = fread(buf, 1, BUFFER_SIZE, fp);
        if(size == 0) {
            print_error("File %s not exist\n", buf);
        }
        
        fclose(fp);
    }
    gettimeofday(&end, NULL);
    long timeuse = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec-start.tv_usec;
    if (once) printf("Cost time: %ldus\n", timeuse);
    else return timeuse;
    return 0;
}

int tese_for_time() {
    int test_times = 0, test_num = 0;
    long MOD = 0;
    unsigned long avg_time = 0;
    printf("Test times, test num and MOD is:");
    scanf("%d %d %ld", &test_times, &test_num, &MOD);
    srand(test_times);
    for(int i = 0; i < test_times; ++i) {
        unsigned int seed = rand();
        long timeuse = test_for_many_files(seed, false, test_num, MOD);
        if(timeuse < 0) {
            print_error("Failed\n");
            return -1;
        }
        COUT_THIS("Test " << i << " time: " << timeuse);
        avg_time += timeuse;
    }
    COUT_THIS("Avg time: " << avg_time / test_times);
    return 0;
}

void test_for_range() {
    char buf[BUFFER_SIZE + 7];
    long start = 0, test_file_num = 0;
    printf("Start file and num is:");
    scanf("%ld %ld", &start, &test_file_num);
    for(long i = 0; i < test_file_num; ++i) {
        sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, MOUNTDIR, FILEPREFIX, FILE_ID_LEN, i + start, FILESUFFIX);
        FILE *fp = fopen(buf, "rb");
        if(fp == NULL) {
            print_error("Error on open file %s\n", buf);
        }
        int size = fread(buf, 1, BUFFER_SIZE, fp);
        if(size == 0) {
            print_error("File %s not exist\n", buf);
        }
        
        fclose(fp);
    }
}

int main() {
    int test_type = 0;
    printf("Test for:\none file(0) | multiple test(1) | time test(2) | range test(3):");
    scanf("%d", &test_type);
    if(test_type == 0) {
        return test_for_one_file();
    }
    else if(test_type == 1) {
        return test_for_many_files();
    }
    else if(test_type == 2){
        return tese_for_time();
    }
    else test_for_range();
    return 0;
}