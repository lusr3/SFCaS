#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <constant.h>
#include <debug.h>

int main() {
    long createNum = 10;
    char buf[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    scanf("%ld", &createNum);
    for(long i = 0; i < createNum; ++i) {
        sprintf(buf, "%s/%s/%s%0*ld%s", PATH2PDIR, OPDIR, FILEPREFIX, FILE_ID_LEN, i, FILESUFFIX);
        sprintf(msg, "%s from %ld\n", "hello", i);
        FILE *fp = fopen(buf, "w");
        if(fp == NULL) {
            print_error("Error in fopen file %s\n", buf);
            return 1;
        }
        else {
            for(int j = 0; j < 1; ++j) fputs(msg, fp);
            fclose(fp);
        }
    }
    return 0;
}