#include "helper.h"
#include "nameserver.h"

// 到每个 dataserver 的心跳连接管理
class HealthCheckClient {
public:
    enum LivingState {
        UNKNOWN = 0,
        NOT_READY = 1, // 新连接还在上传数据中
        LIVING = 2,
        DEAD = 3
    };

    HealthCheckClient() : timestamp_(Clock::now()), gid(-1), state_(LivingState::NOT_READY) {}

    HealthCheckClient(shared_ptr<Channel> channel, const Timestamp &current_timestamp, int _gid = -1)
        : stub_(HealthCheck::NewStub(channel)), timestamp_(current_timestamp), 
        state_(LivingState::NOT_READY), gid(_gid) {}

    // 检测 dataserver 是否失效并设置状态和时间戳
    void check(const string &server_address) {
        // 还未开启 dataserver 端服务（上传需要时间，需要更新时间戳）
        if(this->state_ == LivingState::NOT_READY) {
            timestamp_ = Clock::now();
            return;
        }
        // 为了防止立刻就心跳检测而服务还未启动的余量
        if(!is_timeout(CHECK_BASE)) return;

        ClientContext ctx;
        HealthCheckRequest req;
        HealthCheckResponse reply;
        
        Status status = stub_->Check(&ctx, req, &reply);
        // gRPC 错误
        if(!status.ok()) {
            LOG_THIS(status.error_code() << " " << status.error_message());
            this->state_ = LivingState::DEAD;
            return;
        }

        // 服务端服务未 ready
        if(reply.status() != HealthCheckResponse::SERVING) {
            LOG_THIS("Dataserver not serving!");
            this->state_ = UNKNOWN;
        }

        // 可能从断线恢复正常
        // 正常则需要更新时间戳
        this->state_ = LivingState::LIVING;
        this->timestamp_ = Clock::now();
    }

    bool is_timeout(const std::chrono::seconds &timeout=TIMEOUT) {
        Timestamp current_timestamp = Clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(current_timestamp - this->timestamp_);
        return elapsedTime > timeout;
    }

    // 重连立刻恢复状态
    void reconnect() {
        this->state_ = LivingState::LIVING;
        this->timestamp_ = Clock::now();
    }

    // NOT_READY 是待活状态，也算 living
    inline bool is_living() { return this->state_ == LivingState::LIVING || this->state_ == LivingState::NOT_READY; }
    inline int get_gid() { return this->gid; }
    inline void set_ready() {
        this->state_ = LivingState::LIVING;
        this->timestamp_ = Clock::now();
    };
    
private:
    unique_ptr<HealthCheck::Stub> stub_;
    LivingState state_;
    Timestamp timestamp_;
    int gid;
};

class FileAccessNameserverImpl final : public FileAccess::Service {
public:
    Status get_file_metadata(ServerContext *context, 
        const MetaDataRequest *request, MetaDataReply *reply) override {
        const string &req_filename = request->filename();
        
        metadata_rwlocker_.lock_shared();
        // 找到记录
        if(metadata_ump_.find(req_filename) != metadata_ump_.end()) {
            const int gid = metadata_ump_[req_filename].gid;
            const uint64_t file_size = metadata_ump_[req_filename].file_size;
            metadata_rwlocker_.unlock_shared();
            
            // 从对应组中找一个 dataserver 来服务
            // 保证组内全部 dataserver 删除时不会进来
            group_locker_.lock();
            string server_address = group_ump_[gid].get_least_access();
            group_locker_.unlock();
            if(server_address == "") {
                return Status(StatusCode::UNAVAILABLE, "Dataserver not working!");
            }
            
            reply->set_address(server_address);
            reply->set_file_size(file_size);
            return Status::OK;
        }

        metadata_rwlocker_.unlock_shared();
        return Status(StatusCode::NOT_FOUND, "Can't find this file!");
    }

    // 增加 dataserver 连接（加入心跳检测和对应的服务组）
    Status connect_to_master(ServerContext *context,
        const StartUpMsg *request, StartUpReply *reply) override {
        const string &server_address = request->address();
        const int server_gid = request->gid();
        health_locker_.lock();
        // 新连接
        if(health_ump_.find(server_address) == health_ump_.end()) {
            // 加入心跳检测的 client
            // 设置 gid
            health_ump_[server_address] = HealthCheckClient(grpc::CreateChannel(
                server_address, grpc::InsecureChannelCredentials()), Clock::now(), server_gid);
            
            // 新加入时需要在对应组中加入该机器
            group_locker_.lock();
            // 组内已有其他 dataserver 不需要重复上传元数据
            if(group_ump_[server_gid].empty())
                reply->set_connect_state(StartUpReply::NEW);
            else {
                reply->set_connect_state(StartUpReply::BACKUP);
                health_ump_[server_address].set_ready();
            }
            group_ump_[server_gid].add_server(AccessTime(0, server_address));
            group_locker_.unlock();
        }
        // 重新连接
        else {
            health_ump_[server_address].reconnect();
            group_locker_.lock();
            group_ump_[server_gid].reconnect(server_address);
            group_locker_.unlock();
            reply->set_connect_state(StartUpReply::RECONNECT);
        }
        health_locker_.unlock();
        LOG_THIS("Dataserver in " << server_address << " connected");
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
            metadata_ump_[start_up_msg.filename()] = MetaData(start_up_msg.gid(), start_up_msg.file_size());
        }
        metadata_rwlocker_.unlock();
        
        // 组内第一次上传元数据
        group_locker_.lock();
        health_locker_.lock();
        health_ump_[server_address].set_ready();
        health_locker_.unlock();
        group_locker_.unlock();
        return Status::OK;
    }

    void start_bg() {
        this->t_ = thread([this]() { health_check(); });
        this->group_t_ = thread([this]() { alter_time(); });
    }

private:
    // 每 10s 进行一次检测
    // 60s 没有回复则超时删除
    void health_check() {
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));

            std::unordered_set<string> dataservers_to_remove;
            std::unordered_set<int> gid_to_remove;

            health_locker_.lock();
            for(auto &health_item : health_ump_) {
                const string &server_address = health_item.first;
                HealthCheckClient &health_check_client = health_item.second;
                health_check_client.check(server_address);

                // 濒死状态 且 超时
                if(!health_check_client.is_living() 
                && health_check_client.is_timeout()) dataservers_to_remove.insert(server_address);

                // 仅 濒死 放到 unavailable list
                else if(!health_check_client.is_living()) {
                    LOG_THIS("Dataserver in " << server_address << " unavailable");
                    group_locker_.lock();
                    group_ump_[health_check_client.get_gid()].unavail_server(server_address);
                    group_locker_.unlock();
                }
            }

            // 超时删除
            for(auto &dataserver_address : dataservers_to_remove) {
                int gid = health_ump_[dataserver_address].get_gid();
                LOG_THIS("Delete dataserver in " << dataserver_address << " at " << formatTime(Clock::now()));
                health_ump_.erase(dataserver_address);
                group_locker_.lock();
                group_ump_[gid].remove_server(dataserver_address);
                // 组内所有 dataserver 死亡才删除
                if(group_ump_[gid].empty()) {
                    gid_to_remove.insert(gid);
                    group_ump_.erase(gid);
                    LOG_THIS("Delete Group " << gid << " at " << formatTime(Clock::now()));
                }
                group_locker_.unlock();
            }
            health_locker_.unlock();
            
            metadata_rwlocker_.lock();
            for(auto it = metadata_ump_.begin(); it != metadata_ump_.end(); ) {
                if(gid_to_remove.find(it->second.gid) != gid_to_remove.end()) {
                    metadata_ump_.erase(it++);
                }
                else ++it;
            }
            metadata_rwlocker_.unlock();
        }
    }

    // 每隔 10min 清除所有的访问记录
    // 防止新加入的过多访问
    void alter_time() {
        while(true) {
            std::this_thread::sleep_for(TIME_CLEAN);

            group_locker_.lock();
            for(auto &group : group_ump_) group.second.reset();
            group_locker_.unlock();
        }
    }

    unordered_map<string, MetaData> metadata_ump_;
    // 读写锁防止并发问题
    shared_mutex metadata_rwlocker_;

    unordered_map<string, HealthCheckClient> health_ump_;
    shared_mutex health_locker_;
    thread t_;

    shared_mutex group_locker_;
    unordered_map<int, DataCluster> group_ump_;
    thread group_t_;
};

void run_nameserver() {
    FileAccessNameserverImpl service;

    string address(IP + ":" + PORT);
    ServerBuilder builder;

    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Nameserver listening on port: " << address << endl;

    service.start_bg();
    server->Wait();
}

void read_ini() {
    boost::property_tree::ptree pt;
    boost::property_tree::ini_parser::read_ini("./config/nameserver.ini", pt);

    IP = pt.get<string>("Address.IP");
    PORT = pt.get<string>("Address.PORT");
}

int main() {
    read_ini();
    run_nameserver();
    return 0;
}