#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <constant.h>
#include <debug.h>

int main() {
    char buf[BUFFER_SIZE];
    char filename[MAX_FILE_LEN + 1];
    scanf("%s", filename);
    sprintf(buf, "%s/%s/%s", PATH2PDIR, MOUNTDIR, filename);
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
    return 0;
}