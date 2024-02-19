#include <iostream>
#include <string>

#include <grpcpp/grpcpp.h>

#include "helper.h"
#include "constant.h"
#include "needle.h"
#include "sfcas.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ServerWriter;
using grpc::ClientWriter;
using google::protobuf::Empty;

using grpc::Channel;
using grpc::ClientContext;

using std::string;
using std::shared_ptr;
using std::unique_ptr;
using std::cout;
using std::endl;

using sfcas_dfs::FileAccess;
using sfcas_dfs::DataRequest;
using sfcas_dfs::DataReply;
using sfcas_dfs::StartUpMsg;

static const string IP = "localhost";
static const string PORT = "50002";
static const string MASTER_IP = "localhost";
static const string MASTER_PORT = "50001";

class FileAccessDataServerImpl final : public FileAccess::Service {
public:
    Status get_data(ServerContext *context, const DataRequest *request,
                    ServerWriter<DataReply> *writer) override {
        const string &filename = request->filename();
        uint64_t offset = request->offset(), size = request->size();
        char path2file[BUFFER_SIZE];
        sprintf(path2file, "%s/%s/%s", PATH2PDIR, MOUNTDIR, filename.data());
        FILE *fp = fopen(path2file, "rb");
        if(fp == nullptr) {
            print_error("Error on open file %s\n", filename.data());
            return Status(StatusCode::NOT_FOUND, "Can't find this file!");
        }

        if(this->get_part_of_file(fp, writer, offset, size)) 
            return Status::OK;
        fclose(fp);
        return Status(StatusCode::UNKNOWN, "Failed to read this file!");
    }
private:
    bool get_part_of_file(FILE *fp, ServerWriter<DataReply> *writer,
        uint64_t offset, uint64_t size) {
        char buf[BUFFER_SIZE] = {'\0'};
        DataReply chunkdata;
        int flag = fseek(fp, offset, SEEK_SET);
        if(flag) {
            print_error("offset error\n");
            return false;
        }
        while(size) {
            size_t read_bytes = fread(buf, 1, BUFFER_SIZE, fp);
            // 到达末尾 / 出错
            if(read_bytes < BUFFER_SIZE) {
                // feof 可以判断吗？
                chunkdata.set_chunk(buf, read_bytes);
                writer->Write(chunkdata);
                break;
            }
            // 必须限定大小，否则遇到结束符就会截止
            chunkdata.set_chunk(buf, BUFFER_SIZE);
            writer->Write(chunkdata);
            size -= BUFFER_SIZE;
        }
        return true;
    }
};

class FileAccessDataServerClient {
public:
    FileAccessDataServerClient(shared_ptr<Channel> channel)
        : stub_(FileAccess::NewStub(channel)) {}
    
    void upload_metadata() {
        // client 准备
        StartUpMsg msg;
        msg.set_address(IP + ":" + PORT);
        ClientContext context;
        Empty empty_reply;
        unique_ptr<ClientWriter<StartUpMsg>> writer(
            stub_->upload_metadata(&context, &empty_reply));
        
        // 读取 index 文件获得当前所有的文件并发送
        char path[1024];
        sprintf(path, "%s/%s/%s", PATH2PDIR, OPDIR, INDEXFILE);
        FILE *index_file = fopen(path, "rb");
        if(index_file == nullptr) {
            print_error("Error on open index file %s\n", path);
            exit(1);
        }
        uint64_t index_num = 0;
        needle_index needle;
        fread(&index_num, sizeof(uint64_t), 1, index_file);
        for(size_t needle_i = 0; needle_i < index_num; ++needle_i) {
            read_needle_index(&needle, index_file);
            msg.set_filename(needle.filename.get_name());
            msg.set_file_size(needle.size);
            if(!writer->Write(msg)) {
                break;
            }
        }
        fclose(index_file);
        writer->WritesDone();
        Status status = writer->Finish();
        if(status.ok()) {
            printf("Transfer %ld file metadata to master.\n", index_num);
        }
        else {
            print_error("Transfer file metadata failed!\n");
        }
    }

private:
    unique_ptr<FileAccess::Stub> stub_;
};

void run_dataserver() {
    FileAccessDataServerImpl service;

    string address(IP + ":" + PORT);
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Master listening on port: " << address << endl;

    server->Wait();
}

void startup() {
    string address(MASTER_IP + ":" + MASTER_PORT);
    FileAccessDataServerClient data_server_client(
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials())
    );
    data_server_client.upload_metadata();

    run_dataserver();
}

int main() {
    startup();
    return 0;
}