#include "index.h"

int64_t init(struct needle_index_list *index_list) {
    COUT_THIS("Init start!");
    char path[1024];
	sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, INDEXFILE);

	FILE *index_file = fopen(path, "rb");
	if(index_file == NULL) {
		print_error("Error on open index file %s\n", path);
		return -1;
	}

    fread(&(index_list->index_num), sizeof(uint64_t), 1, index_file);
    index_list->indexs.resize(index_list->index_num);
    DEBUG_THIS("index num: " << index_list->index_num);

	// 读入 index 文件
    for(size_t index_i = 0; index_i < index_list->index_num; ++index_i) {
		read_needle_index(&(index_list->indexs[index_i]), index_file);
	}
	fclose(index_file);

	// 排序
    std::sort(index_list->indexs.begin(), index_list->indexs.end());
    // 打开大文件
    sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, BIGFILE);
	index_list->data_file = fopen(path, "rb");
	if(index_list->data_file == NULL) {
        fclose(index_file);
        release_needle(index_list);
		print_error("Error on open data file.\n");
		return -1;
	}

    // 初始化 cache
    index_list->is_cached = 0;
    index_list->cached_item = nullptr;

	return index_list->index_num;
}

sindex_t *get_sindex_model(std::vector<struct needle_index> &indexs){
    // 构造 keys 和 vals
    // std::vector<index_key_t> keys(indexs.size());
    // for(size_t i = 0; i < indexs.size(); ++i) {
    //     keys[i] = indexs[i].filename;
    // }
    // std::vector<uint64_t> vals(indexs.size());
    // std::iota(vals.begin(), vals.end(), 0);
    // DEBUG_THIS("Index size: " << indexs.size());
    // return new sindex_t(keys, vals, indexs);
    return nullptr;
}

struct needle_index *find_index(struct needle_index_list *index_list, const char *filename, sindex_t *index_model){
    // uint64_t pos = 0;
    // if(index_model->get(index_key_t(filename), pos)) {
    //     return &(index_list->indexs[pos]);
    // }
    // return nullptr;
    
    needle_index key;
    key.filename = index_key_t(filename);
    auto iter = std::lower_bound(index_list->indexs.begin(), index_list->indexs.end(), key);
    if(iter != index_list->indexs.end() && iter->filename == key.filename) return &(index_list->indexs[iter - index_list->indexs.begin()]);
    return nullptr;
}

void release_needle(struct needle_index_list *index_list) {
    if(index_list->data_file) fclose(index_list->data_file);
    index_list->data_file = nullptr;
    index_list->cached_item = nullptr;
}
