#include <needle.h>

void set_needle_index(struct needle_index *needle, struct stat *file_info, struct dirent *entry, uint64_t offset) {
    needle->flags = FILE_EXIT;
    needle->offset = offset;
    needle->size = file_info->st_size;
    needle->filename.set_key(entry->d_name);
    needle->neddle_size = NEEDLE_BASIC_SIZE + strlen(needle->filename.get_name());
}

void read_needle_index(struct needle_index *needle, FILE *index_file) {
    fread(&needle->neddle_size, sizeof(needle->neddle_size), 1, index_file);
    fread(&needle->flags, sizeof(needle->flags), 1, index_file);
    fread(&needle->offset, sizeof(needle->offset), 1, index_file);
    fread(&needle->size, sizeof(needle->size), 1, index_file);
    // 必须有结尾的 '\0'
    char *filename_ptr = needle->filename.get_name();
    fread(filename_ptr, 1, needle->neddle_size - NEEDLE_BASIC_SIZE, index_file);
    filename_ptr[needle->neddle_size - NEEDLE_BASIC_SIZE] = '\0';
}

void insert_needle_index(struct needle_index *needle, FILE *index_file) {
    fwrite(&(needle->neddle_size), sizeof(needle->neddle_size), 1, index_file);
    fwrite(&(needle->flags), sizeof(needle->flags), 1, index_file);
    fwrite(&(needle->offset), sizeof(needle->offset), 1, index_file);
    fwrite(&(needle->size), sizeof(needle->size), 1, index_file);
    fwrite(needle->filename.get_name(), 1, strlen(needle->filename.get_name()), index_file);
}
