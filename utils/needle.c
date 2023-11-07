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

int init(struct needle_index_list *index_list) {
    char path[1024];
	sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, INDEXFILE);

	FILE *index_file = fopen(path, "rb");
	if(index_file == NULL) {
		print_error("Error on open index file %s\n", path);
		return -1;
	}

    fread(&(index_list->size), sizeof(long), 1, index_file);
    index_list->indexs = malloc(sizeof(struct needle_index) * index_list->size);
	if(index_list->indexs == NULL) {
		print_error("Error on malloc needle index\n");
		return -1;
	}

	// 读入 index 文件
    for(long index_size = 0; index_size < index_list->size; ++index_size) {
		read_needle_index(index_list->indexs + index_size, index_file);
	}
	fclose(index_file);

	// 打开大文件
    sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, DATAFILE);
	index_list->data_file = fopen(path, "rb");
	if(index_list->data_file == NULL) {
        release_needle(index_list);
		print_error("Error on open data file.\n");
		return -1;
	}

	return index_list->size;
}

void release_needle(struct needle_index_list *index_list) {
    if(index_list->indexs) free(index_list->indexs);
    if(index_list->data_file) fclose(index_list->data_file);
}

struct needle_index *find_index(struct needle_index_list *index_list, const char *filename){
    for(long i = 0; i < index_list->size; ++i) {
        struct needle_index *cur = &(index_list->indexs)[i];
        if(strcmp(cur->filename, filename) == 0 && (cur->flags & FILE_EXIT)) {
            return cur;
        }
    }
    return NULL;
}