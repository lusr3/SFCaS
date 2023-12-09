#include "index.h"

int init(struct needle_index_list *index_list) {
    printf("Init start!\n");
    char path[1024];
	sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, INDEXFILE);

	FILE *index_file = fopen(path, "rb");
	if(index_file == NULL) {
		print_error("Error on open index file %s\n", path);
		return -1;
	}

    fread(&(index_list->size), sizeof(long), 1, index_file);
    index_list->indexs.resize(index_list->size);

	// if(index_list->indexs == NULL) {
	// 	print_error("Error on malloc needle index\n");
	// 	return -1;
	// }

	// 读入 index 文件
    for(long index_size = 0; index_size < index_list->size; ++index_size) {
		read_needle_index(&(index_list->indexs[index_size]), index_file);
	}
	fclose(index_file);

	// 排序
    std::sort(index_list->indexs.begin(), index_list->indexs.end());
    // 打开大文件
    sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, DATAFILE);
	index_list->data_file = fopen(path, "rb");
	if(index_list->data_file == NULL) {
        release_needle(index_list);
		print_error("Error on open data file.\n");
		return -1;
	}

    // 初始化 cache
    index_list->is_cached = 0;
    index_list->cached_item = NULL;

	return index_list->size;
}

sindex_t *get_sindex_model(std::vector<struct needle_index> &indexs){
    // 转化
    std::vector<index_key_t> keys(indexs.size());
    for(size_t i = 0; i < indexs.size(); ++i) {
        keys[i].set_key(indexs[i].filename);
    }
    std::vector<uint64_t> vals(indexs.size());
    std::iota(vals.begin(), vals.end(), 0);
    return new sindex_t(keys, vals, 1);
}

struct needle_index *find_index(struct needle_index_list *index_list, const char *filename, sindex_t *index_model){
    uint64_t pos = 0;
    if(index_model->get(index_key_t(filename), pos, 1)) {
        return &(index_list->indexs[pos]);
    }
    return nullptr;
}