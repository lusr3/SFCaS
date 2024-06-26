#define FUSE_USE_VERSION 31

#include <fuse.h>

#include "helper.h"
#include "constant.h"
#include "util.h"

static string NAME_IP = "localhost";
static string NAME_PORT = "50001";

struct MetaData {
    string address;
    uint64_t file_size;

    MetaData() : file_size(0) {}
    MetaData(const string &_address, uint64_t _file_size) 
        : address(_address), file_size(_file_size) {}
};

class FileAccessMetaDataClient {
public:
    FileAccessMetaDataClient(shared_ptr<Channel> channel)
        : stub_(FileAccess::NewStub(channel)), is_cached_(false) {}

    bool get_file_metadata(const char *filename, MetaData &metadata) {
        MetaDataRequest request;
        request.set_filename(filename);
        MetaDataReply reply;
        ClientContext context;

        Status status = stub_->get_file_metadata(&context, request, &reply);
        if(!status.ok()) {
            COUT_THIS(status.error_message());
            return false;
        }

		metadata.address = reply.address();
		metadata.file_size = reply.file_size();
        return true;
    }

    inline void remove_cache() { is_cached_ = false; }
	inline bool is_cached() { return is_cached_; }
	inline const char* get_cached_filename() { return cached_filename_.data(); }
	inline string& get_cached_address() { return cached_metadata_.address; }

    void set_cache_item(const char *filename, MetaData &metadata) {
        is_cached_ = true;
        cached_filename_ = filename;
		cached_metadata_ = metadata;
    }

private:
    unique_ptr<FileAccess::Stub> stub_;
    bool is_cached_;
    string cached_filename_;
	MetaData cached_metadata_;
};

class FileAccessDataClient {
public:
    FileAccessDataClient(shared_ptr<Channel> channel)
        : stub_(FileAccess::NewStub(channel)) {}

    int get_data(const char *filename, uint64_t offset, uint64_t size, char *buf) {
        DataRequest request;
        request.set_filename(filename);
        request.set_offset(offset);
        request.set_size(size);
        DataReply reply;
        ClientContext context;
        unique_ptr<ClientReader<DataReply>> reader(
            stub_->get_data(&context, request)
        );

        int received_bytes = 0;
        // 数据量小一次传输就行
        while(reader->Read(&reply)) {
            const string &data = reply.chunk();
            memcpy(buf + received_bytes, data.data(), data.length());
            received_bytes += data.length();
        }

        Status status = reader->Finish();
        if(!status.ok()) {
            COUT_THIS(status.error_message());
			return -1;
        }
		return received_bytes;
    }
private:
    unique_ptr<FileAccess::Stub> stub_;
};

static FileAccessMetaDataClient *metadata_client;

static int client_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	const char *filename = strrchr(path, '/');
	
	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(filename, "/") == 0) {
		stbuf->st_mode = __S_IFDIR | 0755;
		metadata_client->remove_cache();
	}
	else if(strcmp(filename + 1, BIGFILE) == 0
	|| strcmp(filename + 1, INDEXFILE) == 0) {
		stbuf->st_mode = __S_IFREG | 0444;
		metadata_client->remove_cache();
	}
	else {
		// 从 master 处读取文件的元数据信息
		// 包括 存储机地址 和 文件大小
		MetaData metadata;
		if(!metadata_client->get_file_metadata(filename + 1, metadata)) {
			metadata_client->remove_cache();
			print_error("Error on finding target file %s.\n", filename + 1);
			return -ENOENT;
		}
        metadata_client->set_cache_item(filename + 1, metadata);
		stbuf->st_mode = __S_IFREG | 0444;
		stbuf->st_size = metadata.file_size;
	}
	
	return 0;
}

static int client_open(const char *path, struct fuse_file_info *fi) {	
	const char *filename = strrchr(path, '/');

	MetaData metadata;
	if(!metadata_client->is_cached() || strcmp(filename + 1, metadata_client->get_cached_filename()) != 0) {
		if(metadata_client->get_file_metadata(filename + 1, metadata)) {
			metadata_client->set_cache_item(filename + 1, metadata);
		}
		else return -ENOENT;
	}
	
	return 0;
}

static int client_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi) {
	const char *filename = strrchr(path, '/');

	MetaData metadata;
	if(!metadata_client->is_cached() || strcmp(filename + 1, metadata_client->get_cached_filename()) != 0) {
		if(metadata_client->get_file_metadata(filename + 1, metadata)) {
			metadata_client->set_cache_item(filename + 1, metadata);
		}
		else {
			print_error("Error on finding target file %s.\n", filename + 1);
			return -ENOENT;
		}
	}
	// 从存储机处读取数据
	FileAccessDataClient data_client(
		grpc::CreateChannel(metadata_client->get_cached_address(),
		grpc::InsecureChannelCredentials())
	);
	
	int read_size = data_client.get_data(filename + 1, offset, size, buf);
	// nameserver 还未检测到 dataserver 的异常
	if(read_size < 0) {
		LOG_THIS("Something wrong with dataserver");
		return -ENOENT;
	}
	return read_size;
}

static void *client_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	(void) conn;
	// cfg->kernel_cache = 1;
	return NULL;
}

// 按照出现顺序无序初始化
static const struct fuse_operations myOper = {
	.getattr 	= client_getattr,
	.open 		= client_open,
	.read 		= client_read,
	.init		= client_init
};

void read_ini() {
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("./config/nameserver.ini", pt);

    NAME_IP = pt.get<string>("Address.IP");
    NAME_PORT = pt.get<string>("Address.PORT");
}

int main(int argc, char *argv[])
{	
	read_ini();
	metadata_client = new FileAccessMetaDataClient(
		grpc::CreateChannel(NAME_IP + ":" + NAME_PORT,
		grpc::InsecureChannelCredentials())
	);
	int res = fuse_main(argc, argv, &myOper, NULL);
	return res;
}
