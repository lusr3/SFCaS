#include "helper.h"
#include "constant.h"
#include "needle.h"
#include "util.h"

static string IP = "localhost";
static string PORT = "50002";
static int GID = -1;
static string NAME_IP = "localhost";
static string NAME_PORT = "50001";

class HealthCheckServerImpl final : public HealthCheck::Service {
    // 能连接就能服务
    Status Check(ServerContext *context, const HealthCheckRequest *request,
                HealthCheckResponse *reply) override {
        reply->set_status(HealthCheckResponse::SERVING);
        return Status::OK;
    }
};

class FileAccessDataServerImpl final : public FileAccess::Service {
public:
    Status get_data(ServerContext *context, const DataRequest *request,
                    ServerWriter<DataReply> *writer) override {
        LOG_THIS(IP + ":" + PORT << " is serving");
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
    
    StartUpReply::ConnectState connect_to_master() {
        StartUpMsg msg;
        msg.set_address(IP + ":" + PORT);
        msg.set_gid(GID);
        ClientContext context;
        StartUpReply reply;

        Status status = stub_->connect_to_master(&context, msg, &reply);
        if(!status.ok()) {
            LOG_THIS(status.error_code() << " " << status.error_message());
            return StartUpReply::ERROR;
        }

        return reply.connect_state();
    }

    void upload_metadata() {
        // client 准备
        StartUpMsg msg;
        msg.set_gid(GID);
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
    FileAccessDataServerImpl file_access_service;
    HealthCheckServerImpl health_check_service;

    string address(IP + ":" + PORT);
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&file_access_service);
    builder.RegisterService(&health_check_service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Dataserver listening on port: " << address << endl;

    server->Wait();
}

void startup() {
    string address(NAME_IP + ":" + NAME_PORT);
    FileAccessDataServerClient data_server_client(
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials())
    );
    // 新连接且无这个组别才需要上传
    StartUpReply::StartUpReply::ConnectState connect_state = data_server_client.connect_to_master();
    if(connect_state == StartUpReply::NEW) {
        LOG_THIS("New Connect!");
        data_server_client.upload_metadata();
    }
    else if(connect_state == StartUpReply::BACKUP)
        LOG_THIS("New Group Member!");
    else if(connect_state == StartUpReply::RECONNECT)
        LOG_THIS("Reconnect!");
    else {
        LOG_THIS("Connection Refused!");
        return;
    }

    run_dataserver();
}

void read_ini() {
    // nameserver ini
    boost::property_tree::ptree name_pt;
    boost::property_tree::ini_parser::read_ini("./config/nameserver.ini", name_pt);

    NAME_IP = name_pt.get<string>("Address.IP");
    NAME_PORT = name_pt.get<string>("Address.PORT");

    // dataserver ini
    boost::property_tree::ptree data_pt;
    boost::property_tree::ini_parser::read_ini("./config/dataserver.ini", data_pt);

    IP = data_pt.get<string>("Address.IP");
    PORT = data_pt.get<string>("Address.PORT");
    GID = data_pt.get<int>("Group.GID");
}

int main() {
    read_ini();
    startup();
    return 0;
}