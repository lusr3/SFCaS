#include <iostream>
#include <string>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "helper.h"
#include "file_access.grpc.pb.h"
#include "health_check.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using grpc::StatusCode;
using grpc::ServerReader;
using grpc::Channel;
using grpc::ClientContext;
using google::protobuf::Empty;

using std::string;
using std::unordered_map;
using std::unique_ptr;
using std::shared_ptr;
using std::mutex;
using std::shared_mutex;
using std::thread;
using std::cout;
using std::endl;

using sfcas::fileaccess::FileAccess;
using sfcas::fileaccess::MetaDataRequest;
using sfcas::fileaccess::MetaDataReply;
using sfcas::fileaccess::StartUpMsg;
using sfcas::fileaccess::StartUpReply;
using sfcas::healthcheck::HealthCheck;
using sfcas::healthcheck::HealthCheckRequest;
using sfcas::healthcheck::HealthCheckResponse;

typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::time_point<Clock> Timestamp;

static const string IP = "localhost";
static const string PORT = "50001";
static const std::chrono::seconds TIMEOUT(25);
static const std::chrono::seconds CHECK_BASE(8);
static const std::chrono::seconds CHECK_INTERVAL(10);

struct MetaData {
    string address;
    uint64_t file_size;

    MetaData() {}
    MetaData(const string &_address, uint64_t _file_size) 
        : address(_address), file_size(_file_size) {}
};

class HealthCheckClient {
public:
    enum LivingState {
        UNKNOWN = 0,
        LIVING = 1,
        DEAD = 2
    };

    HealthCheckClient() {}

    HealthCheckClient(shared_ptr<Channel> channel, const Timestamp &current_timestamp)
        : stub_(HealthCheck::NewStub(channel)), timestamp_(current_timestamp), state_(LivingState::LIVING) { }

    void check(const string &server_address) {
        // 防止刚加入时的心跳检测误判
        if(!is_timeout(CHECK_BASE)) return;
        // LOG_THIS("Check health");

        ClientContext ctx;
        HealthCheckRequest req;
        HealthCheckResponse reply;
        
        Status status = stub_->Check(&ctx, req, &reply);
        // gRPC 错误
        if(!status.ok()) {
            std::cerr << status.error_code() << " " << status.error_message() << endl;
            this->state_ = LivingState::DEAD;
            return;
        }

        // 服务端服务未 ready
        if(reply.status() != HealthCheckResponse::SERVING) {
            this->state_ = UNKNOWN;
        }

        // 可能从断线恢复正常
        // 正常则需要更新时间戳
        this->state_ = LivingState::LIVING;
        this->timestamp_ = Clock::now();
    }

    void set_time_stamp(Timestamp &current_timestamp) {
        timestamp_ = current_timestamp;
    }

    bool is_timeout(const std::chrono::seconds &timeout=TIMEOUT) {
        Timestamp current_timestamp = Clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(current_timestamp - this->timestamp_);
        return elapsedTime > timeout;
    }

    void reconnect() {
        this->state_ = LivingState::LIVING;
        this->timestamp_ = Clock::now();
    }

    inline bool is_living() { return this->state_ == LivingState::LIVING; }
    
    LivingState state_;

private:
    unique_ptr<HealthCheck::Stub> stub_;
    Timestamp timestamp_;
};

class FileAccessNameserverImpl final : public FileAccess::Service {
public:
    Status get_file_metadata(ServerContext *context, 
        const MetaDataRequest *request, MetaDataReply *reply) override {
        const string &req_filename = request->filename();
        
        metadata_rwlocker_.lock_shared();
        // 找到记录
        if(metadata_ump_.find(req_filename) != metadata_ump_.end()) {
            const string &server_address = metadata_ump_[req_filename].address;
            const uint64_t file_size = metadata_ump_[req_filename].file_size;
            metadata_rwlocker_.unlock_shared();
            
            health_locker_.lock_shared();
            // dataserver 已失效未恢复 / 濒死状态
            if(health_ump_.find(server_address) == health_ump_.end()
            || !health_ump_[server_address].is_living()) {
                health_locker_.unlock_shared();
                return Status(StatusCode::UNAVAILABLE, "Dataserver not working!");
            }
            health_locker_.unlock_shared();
            
            reply->set_address(server_address);
            reply->set_file_size(file_size);
            return Status::OK;
        }

        metadata_rwlocker_.unlock_shared();
        return Status(StatusCode::NOT_FOUND, "Can't find this file!");
    }

    Status connect_to_master(ServerContext *context,
        const StartUpMsg *request, StartUpReply *reply) override {
        const string &server_address = request->address();
        health_locker_.lock();
        // 新连接
        if(health_ump_.find(server_address) == health_ump_.end()) {
            // 加入心跳检测的 client
            health_ump_[server_address] = HealthCheckClient(grpc::CreateChannel(
                server_address, grpc::InsecureChannelCredentials()), Clock::now());
            reply->set_connect_state(StartUpReply::NEW);
        }
        // 重新连接
        else {
            health_ump_[server_address].reconnect();
            reply->set_connect_state(StartUpReply::RECONNECT);
        }
        health_locker_.unlock();
        return Status::OK;
    }

    Status upload_metadata(ServerContext *context,
        ServerReader<StartUpMsg> *reader, Empty *response) override {
        StartUpMsg start_up_msg;
        string server_address;

        // 防止多个 dataserver 同时启动上传导致的并发问题
        metadata_rwlocker_.lock();
        while(reader->Read(&start_up_msg)) {
            if(server_address.empty()) server_address = start_up_msg.address();
            add_metadata(start_up_msg.filename(), start_up_msg.address(), start_up_msg.file_size());
        }
        metadata_rwlocker_.unlock();
        
        return Status::OK;
    }

    void start_health_check() {
        this->checking_state_ = true;
        this->t_ = thread([this]() { check(); });
    }

private:
    inline void add_metadata(const string &filename, const string &address,
                        uint64_t file_size) {
        metadata_ump_[filename] = MetaData(address, file_size);
    }

    // 每 10s 进行一次检测
    // 60s 没有回复则超时删除
    void check() {
        while(checking_state_) {
            std::unordered_set<string> dataservers_to_remove;

            health_locker_.lock();
            for(auto &health_item : health_ump_) {
                const string &server_address = health_item.first;
                HealthCheckClient &health_check_client = health_item.second;
                health_check_client.check(server_address);

                // 濒死状态且超时
                if(!health_check_client.is_living() 
                && health_check_client.is_timeout()) dataservers_to_remove.insert(server_address);
            }

            // 超时删除
            for(auto &dataserver_address : dataservers_to_remove) {
                LOG_THIS("Delete dataserver in " << dataserver_address << " at " << formatTime(Clock::now()));
                health_ump_.erase(dataserver_address);
            }
            health_locker_.unlock();

            metadata_rwlocker_.lock();
            for(auto it = metadata_ump_.begin(); it != metadata_ump_.end(); ) {
                if(dataservers_to_remove.find(it->second.address) != dataservers_to_remove.end()) {
                    metadata_ump_.erase(it++);
                }
                else ++it;
            }
            metadata_rwlocker_.unlock();
            
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));
        }
    }

    unordered_map<string, MetaData> metadata_ump_;
    // 读写锁防止并发问题
    shared_mutex metadata_rwlocker_;

    unordered_map<string, HealthCheckClient> health_ump_;
    shared_mutex health_locker_;
    thread t_;
    bool checking_state_;
};

void run_nameserver() {
    FileAccessNameserverImpl service;

    string address(IP + ":" + PORT);
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Nameserver listening on port: " << address << endl;

    service.start_health_check();
    server->Wait();
}

int main() {
    run_nameserver();
    return 0;
}