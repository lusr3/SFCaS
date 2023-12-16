#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <vector>

#include "helper.h"
#include "sindex.h"
#include "needle.h"

#if !defined(INDEX_H)
#define INDEX_H

typedef sindex::SIndex<index_key_t, uint64_t> sindex_t;

// 装载 index 文件内容并得到大文件的文件指针
// 成功返回 index 数目，失败返回 -1
int64_t init(struct needle_index_list *index_list);
void release_needle(struct needle_index_list *index_list);

// 从 index_list 中找到对应于 filename 的 needle_index
struct needle_index *find_index(struct needle_index_list *index_list, const char *filename, sindex_t *sindex_model);

/*  SIndex  */
sindex_t *get_sindex_model(std::vector<struct needle_index> &indexs);
inline void release_model(sindex_t *sindex_model) {
    delete sindex_model;
}

#endif