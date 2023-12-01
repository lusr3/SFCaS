#include <needle.h>

void set_needle_index(struct needle_index *needle, struct stat *file_info, struct dirent *entry, long offset) {
    needle->flags = FILE_EXIT;
    needle->offset = offset;
    needle->size = file_info->st_size;
    sprintf(needle->filename, "%s", entry->d_name);
    needle->neddle_size = NEEDLE_BASIC_SIZE + strlen(needle->filename);
}

void read_needle_index(struct needle_index *needle, FILE *index_file){
    fread(&needle->neddle_size, sizeof(needle->neddle_size), 1, index_file);
    fread(&needle->flags, sizeof(needle->flags), 1, index_file);
    fread(&needle->offset, sizeof(needle->offset), 1, index_file);
    fread(&needle->size, sizeof(needle->size), 1, index_file);
    fread(needle->filename, 1, needle->neddle_size - NEEDLE_BASIC_SIZE, index_file);
    needle->filename[needle->neddle_size - NEEDLE_BASIC_SIZE] = '\0';
}

void insert_needle_index(FILE *index_file, struct needle_index *needle) {
    fwrite(&(needle->neddle_size), sizeof(needle->neddle_size), 1, index_file);
    fwrite(&(needle->flags), sizeof(needle->flags), 1, index_file);
    fwrite(&(needle->offset), sizeof(needle->offset), 1, index_file);
    fwrite(&(needle->size), sizeof(needle->size), 1, index_file);
    fwrite(needle->filename, 1, strlen(needle->filename), index_file);
}

void release_needle(struct needle_index_list *index_list) {
    // if(index_list->indexs) free(index_list->indexs);
    if(index_list->data_file) fclose(index_list->data_file);
}

