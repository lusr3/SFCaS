#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>

#include "needle.h"
#include "helper.h"
#include "index.h"

// 初始化时加载的索引信息
static struct needle_index_list index_list;
static sindex_t *sindex_model = nullptr;
static std::unordered_map<index_key_t, size_t> ump;

static int sfcas_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	const char *filename = strrchr(path, '/');
	
	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(filename, "/") == 0) {
		stbuf->st_mode = __S_IFDIR | 0755;
		index_list.is_cached = false;
	}
	else if(strcmp(filename + 1, BIGFILE) == 0
	|| strcmp(filename + 1, INDEXFILE) == 0) {
		stbuf->st_mode = __S_IFREG | 0444;
		index_list.is_cached = false;
	}
	else {
		struct needle_index *cur_index = find_index(&index_list, filename + 1, ump);
		if(!cur_index) {
			index_list.is_cached = false;
			print_error("Error on finding target file %s.\n", filename + 1);
			return -ENOENT;
		}
		index_list.is_cached = true;
		index_list.cached_item = cur_index;
		stbuf->st_mode = __S_IFREG | 0444;
		stbuf->st_size = cur_index->size;
	}
	
	return 0;
}

static int sfcas_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
			   enum fuse_readdir_flags flags) {
	for(size_t i = 0; i < index_list.index_num; ++i) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_size = index_list.indexs[i].size;
		st.st_mode = __S_IFREG | 0444;
		if (filler(buf, index_list.indexs[i].filename.get_name(), &st, 0, fuse_fill_dir_flags(0)))
			break;
	}
	filler(buf, ".", nullptr, 0, fuse_fill_dir_flags(0));
	filler(buf, "..", nullptr, 0, fuse_fill_dir_flags(0));

	return 0;
}

static int sfcas_open(const char *path, struct fuse_file_info *fi) {	
	const char *filename = strrchr(path, '/');

	struct needle_index *cur_index = NULL;
	if(index_list.is_cached && strcmp(filename + 1, index_list.cached_item->filename.get_name()) == 0) 
		cur_index = index_list.cached_item;
	else 
		cur_index = find_index(&index_list, filename + 1, ump);
	
	if(!cur_index) {
		return -ENOENT;
	}
	return 0;
}

static int sfcas_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi) {
	const char *filename = strrchr(path, '/');

	// 查找文件元数据
	struct needle_index *cur_index = NULL;
	if(index_list.is_cached && strcmp(filename + 1, index_list.cached_item->filename.get_name()) == 0) 
		cur_index = index_list.cached_item;
	else 
		cur_index = find_index(&index_list, filename + 1, ump);
	// 读取数据
	if(cur_index) {
		int flag = fseek(index_list.data_file, cur_index->offset + offset, SEEK_SET);
		if(flag) {
			print_error("Error on fseek position\n");
			return -errno;
		}
		int read_size = fread(buf, 1, size, index_list.data_file);
		return read_size;
	}
	print_error("Error on finding target file %s.\n", filename + 1);
	return -ENOENT;
}

static void *sfcas_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	// cfg->kernel_cache = 1;
	return NULL;
}

// 按照出现顺序无序初始化
static const struct fuse_operations myOper = {
	.getattr 	= sfcas_getattr,
	.open 		= sfcas_open,
	.read 		= sfcas_read,
	.readdir 	= sfcas_readdir,
	.init		= sfcas_init
};

int main(int argc, char *argv[])
{
	if(init(&index_list, ump) < 0) {
		print_error("Error on load index\n");
		return 1;
	}
	printf("Init Success!\n");
	sindex_model = get_sindex_model(index_list.indexs);
	printf("Get Model success!\n");

	int res = fuse_main(argc, argv, &myOper, NULL);

	release_needle(&index_list);
	release_model(sindex_model);
	return res;
}
