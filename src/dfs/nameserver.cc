#include "helper.h"
#include "nameserver.h"

class FileAccessNameserverImpl final : public FileAccess::Service {
public:
    FileAccessNameserverImpl() {
        data_clusters_.resize(MAX_GROUP_NUM);
    }

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
            string server_address = data_clusters_[gid].get_least_access();
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
        // 新连接
        if(data_clusters_[server_gid].is_new_connect(server_address)) {
            data_clusters_[server_gid].add_server(server_address, server_gid);
            // 组内只有一个 dataserver 需要上传元数据
            if(data_clusters_[server_gid].single())
                reply->set_connect_state(StartUpReply::NEW);
            else {
                reply->set_connect_state(StartUpReply::BACKUP);
            }
        }
        // 找得到该地址得还要分为【重复连接】和【重新连接】
        else {
            if(data_clusters_[server_gid].duplicate(server_address)) {
                reply->set_connect_state(StartUpReply::DUPLICATE);
                return Status::OK;
            }
            else {
                data_clusters_[server_gid].reconnect(server_address);
                reply->set_connect_state(StartUpReply::RECONNECT);
            }
        }
        LOG_THIS("Group " << server_gid <<  " dataserver in " << server_address << " connected");
        return Status::OK;
    }

    Status upload_metadata(ServerContext *context,
        ServerReader<StartUpMsg> *reader, Empty *response) override {
        StartUpMsg start_up_msg;
        int server_gid = -1;
        string server_address;

        // 防止多个 dataserver 同时启动上传导致的并发问题
        metadata_rwlocker_.lock();
        while(reader->Read(&start_up_msg)) {
            if(server_gid == -1 && server_address.empty()) {
                server_gid = start_up_msg.gid();
                server_address = start_up_msg.address();
            }
            metadata_ump_[start_up_msg.filename()] = MetaData(start_up_msg.gid(), start_up_msg.file_size());
        }
        metadata_rwlocker_.unlock();
        
        // 组内第一次上传元数据
        data_clusters_[server_gid].set_ready();
        return Status::OK;
    }

    void start_bg() {
        this->t_ = thread([this]() { health_check(); });
        this->group_t_ = thread([this]() { reset_time(); });
    }

private:
    // 每 10s 进行一次检测
    // 60s 没有回复则超时删除
    void health_check() {
        while(true) {
            std::this_thread::sleep_for(std::chrono::seconds(CHECK_INTERVAL));

            std::unordered_set<int> gid_to_remove;
            for(int i = 0; i < MAX_GROUP_NUM; ++i) {
                DataCluster &data_cluster = data_clusters_[i];
                if(data_cluster.empty()) continue;
                data_cluster.check();
                if(data_cluster.empty()) {
                    gid_to_remove.insert(i);
                    LOG_THIS("Delete group " << i);
                }
            }
            
            // 组空了就直接删除该组所有元信息
            // 通过 bg 线程异步删除而不要在这里同步删除（空闲时）？
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
    // 防止新加入的被过多访问
    void reset_time() {
        while(true) {
            std::this_thread::sleep_for(CLEAN_INTERVAL);
            LOG_THIS("Rest access time at " << formatTime(Clock::now()));

            for(int i = 0; i < MAX_GROUP_NUM; ++i) {
                DataCluster &data_cluster = data_clusters_[i];
                if(data_cluster.empty()) continue;
                data_cluster.reset();
            }
        }
    }

    unordered_map<string, MetaData> metadata_ump_;
    // 读写锁防止并发问题
    shared_mutex metadata_rwlocker_;

    vector<DataCluster> data_clusters_;
    thread t_;
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

    // address
    IP = pt.get<string>("Address.IP");
    PORT = pt.get<string>("Address.PORT");

    // time
    TIMEOUT = std::chrono::seconds(pt.get<int>("Time.TIMEOUT"));
    CHECK_BASE = std::chrono::seconds(pt.get<int>("Time.CHECK_BASE"));
    CHECK_INTERVAL = std::chrono::seconds(pt.get<int>("Time.CHECK_INTERVAL"));
    CLEAN_INTERVAL = std::chrono::minutes(pt.get<int>("Time.CLEAN_INTERVAL"));

    // group
    MAX_GROUP_NUM = pt.get<int>("Group.GROUP_NUM");
}

int main() {
    read_ini();
    run_nameserver();
    return 0;
}