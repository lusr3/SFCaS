/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.

  gcc -Wall ./src/myread.c ./utils/debug.c ./utils/needle.c -o ./bin/myread `pkg-config fuse3 --cflags --libs` -I ./include
*/

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
#include <needle.h>
#include <debug.h>
#include <memory.h>
#include <stdlib.h>

// 初始化时加载的索引信息
static struct needle_index_list index_list;

static int my_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	printf("%s %s\n", __FUNCTION__, path);
	char *filename = strrchr(path, '/');
	
	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(filename, "/") == 0) {
		stbuf->st_mode = __S_IFDIR | 0755;
		index_list.is_cached = 0;
	}
	else if(strcmp(filename + 1, DATAFILE) == 0
	|| strcmp(filename + 1, INDEXFILE) == 0) {
		stbuf->st_mode = __S_IFREG | 0444;
		index_list.is_cached = 0;
	}
	else {
		struct needle_index *cur_index = find_index(&index_list, filename + 1);
		if(!cur_index) {
			index_list.is_cached = 0;
			print_error("Error on finding target file %s.\n", filename + 1);
			return -errno;
		}
		index_list.is_cached = 1;
		index_list.cached_item = cur_index;
		stbuf->st_mode = __S_IFREG | 0444;
		stbuf->st_size = cur_index->size;
	}
	
	return 0;
}

static int my_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
			   enum fuse_readdir_flags flags) {
	printf("%s %s\n", __FUNCTION__, path);
	DIR *dp;
	struct dirent *de;

	(void) offset;
	(void) fi;

	dp = opendir(path);
	if (dp == NULL)
		return -errno;

	while ((de = readdir(dp)) != NULL) {
		struct stat st;
		memset(&st, 0, sizeof(st));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;
		if (filler(buf, de->d_name, &st, 0, 0))
			break;
	}

	closedir(dp);
	return 0;
}

static int my_open(const char *path, struct fuse_file_info *fi) {	
	printf("%s %s\n", __FUNCTION__, path);
	return 0;
}

static int my_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi) {
	printf("%s %s size %ld offset %ld\n", __FUNCTION__, path, size, offset);
	char *filename = strrchr(path, '/');

	struct needle_index *cur_index = NULL;
	if(index_list.is_cached && strcmp(filename + 1, index_list.cached_item->filename) == 0) 
		cur_index = index_list.cached_item;
	else 
		cur_index = find_index(&index_list, filename + 1);
	if(cur_index) {
		int flag = fseek(index_list.data_file, cur_index->offset + offset, SEEK_SET);
		if(flag) {
			print_error("Error on fseek position\n");
			return -errno;
		}
		int read_size = fread(buf, 1, size, index_list.data_file);
		printf("Read %d bytes\n", read_size);
		return read_size;
	}
	print_error("Error on finding target file %s.\n", filename + 1);
	return -errno;
}

static const struct fuse_operations myOper =  {
	.getattr 	= my_getattr,
	.readdir 	= my_readdir,
	.open 		= my_open,
	.read 		= my_read
};

int main(int argc, char *argv[])
{
	memset(&index_list, 0, sizeof(index_list));
	if(init(&index_list) < 0) {
		print_error("Error on load index\n");
		return 1;
	}

	int res = fuse_main(argc, argv, &myOper, NULL);

	release_needle(&index_list);
	return res;
}
