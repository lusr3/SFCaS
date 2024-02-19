#include <iostream>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "sfcas.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ServerReader;
using google::protobuf::Empty;

using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::cout;
using std::endl;

using sfcas_dfs::FileAccess;
using sfcas_dfs::MetaDataRequest;
using sfcas_dfs::MetaDataReply;
using sfcas_dfs::StartUpMsg;

static const string IP = "localhost";
static const string PORT = "50001";

struct MetaData {
    string address;
    uint64_t file_size;

    MetaData() {}
    MetaData(const string &_address, uint64_t _file_size) 
        : address(_address), file_size(_file_size) {}
};

class FileAccessMasterImpl final : public FileAccess::Service {
public:
    Status get_file_metadata(ServerContext *context, 
        const MetaDataRequest *request, MetaDataReply *reply) override {
        const string &req_filename = request->filename();
        if(ump_.find(req_filename) != ump_.end()) {
            reply->set_address(ump_[req_filename].address);
            reply->set_file_size(ump_[req_filename].file_size);
            return Status::OK;
        }

        return Status(StatusCode::NOT_FOUND, "Can't find this file!");
    }

    Status upload_metadata(ServerContext *context,
        ServerReader<StartUpMsg> *reader, Empty *response) override {
        StartUpMsg start_up_msg;
        while(reader->Read(&start_up_msg)) {
            add_metadata(start_up_msg.filename(), start_up_msg.address(), start_up_msg.file_size());
        }
        return Status::OK;
    }

private:
    inline void add_metadata(const string &filename, const string &address,
                        uint64_t file_size) {
        ump_[filename] = MetaData(address, file_size);
    }

    unordered_map<string, MetaData> ump_;
};

void run_master() {
    FileAccessMasterImpl service;

    string address(IP + ":" + PORT);
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Master listening on port: " << address << endl;

    server->Wait();
}

int main() {
    run_master();
    return 0;
}