#include <iostream>
#include <string>
#include <unordered_map>

#include <grpcpp/grpcpp.h>

#include "sfcas_dfs.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;

using sfcas_dfs::FileAccess;
using sfcas_dfs::LocationRequest;
using sfcas_dfs::LocationReply;

struct Location {
    std::string ip;
    std::string port;

    Location() {}
    Location(const std::string &_ip, const std::string &_port) 
        : ip(_ip), port(_port) {}
};

class FileAccessMasterImpl final : public FileAccess::Service {
public:
    void add_location(const std::string &filename, const std::string &ip, const std::string &port) {
        ump_[filename] = Location(ip, port);
    }

private:
    Status get_file_location(ServerContext *context, 
        const LocationRequest *request, LocationReply *reply) override {
        const std::string &req_filename = request->filename();
        if(ump_.find(req_filename) != ump_.end()) {
            reply->set_ip(ump_[req_filename].ip);
            reply->set_port(ump_[req_filename].port);
            return Status::OK;
        }

        return Status(StatusCode::NOT_FOUND, "Can't find this file!");
    }

    std::unordered_map<std::string, Location> ump_;
};

void run_master() {
    FileAccessMasterImpl service;
    service.add_location("test1.txt", "127.0.0.1", "50002");
    service.add_location("test2.txt", "127.0.0.1", "50002");
    service.add_location("test3.txt", "127.0.0.1", "50002");
    service.add_location("test4.txt", "127.0.0.1", "50002");

    std::string address("localhost:50001");
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Master listening on port: " << address << std::endl;

    server->Wait();
}

int main() {
    run_master();

    return 0;
}