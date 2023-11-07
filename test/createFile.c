#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <constant.h>
#include <debug.h>

int main() {
    int createNum = 10;
    char buf[BUFFER_SIZE];
    char msg[BUFFER_SIZE];
    scanf("%d", &createNum);
    for(int i = 0; i < createNum; ++i) {
        sprintf(buf, "%s/%s/%s%05d%s", PATH2PDIR, OPDIR, FILEPREFIX, i, FILEPSUFFIX);
        sprintf(msg, "%s from %d\n", "hello", i);
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