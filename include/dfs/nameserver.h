#include "util.h"
#if !defined(NAMESERVER_H)
#define NAMESERVER_H

static string IP = "localhost";
static string PORT = "50001";
static int MAX_GROUP_NUM = 100;
static std::chrono::seconds TIMEOUT(60);
static std::chrono::seconds CHECK_BASE(8);
static std::chrono::seconds CHECK_INTERVAL(10);
static std::chrono::minutes CLEAN_INTERVAL(10);

// 记录文件名对应的 组 id 和 大小
struct MetaData {
    int gid;
    uint64_t file_size;

    MetaData() {}
    MetaData(int _gid, uint64_t _file_size) 
        : gid(_gid), file_size(_file_size) {}
};

// 记录对应组内 dataserver 的访问次数和地址
struct AccessTime {
    int times;
    string address;
    AccessTime(int _times = 0, const string &_addr = "")
        : times(_times), address(_addr) {}
    bool operator<(const AccessTime &other) {
        return this->times > other.times;
    }
};

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
    void check() {
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
            return;
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

// 多个 dataserver 组成一个组，形成 datacluster
class DataCluster {
public:
    DataCluster() : available_num_(0) {}
    DataCluster(const DataCluster &data_cluster) : available_num_(0) {}

    // 加入新 server 到可用末尾并重新建堆
    // 加入心跳检测
    void add_server(const string &address, const int gid) {
        locker_.lock();
        access_times_.push_back(AccessTime(0, address));
        // 表示当前有 濒死 状态的 dataserver
        if(available_num_ < access_times_.size() - 1)
            std::swap(access_times_.back(), access_times_[available_num_]);
        ++available_num_;
        std::push_heap(access_times_.begin(), access_times_.begin() + available_num_);
        health_ump_[address] = HealthCheckClient(grpc::CreateChannel(
                address, grpc::InsecureChannelCredentials()), Clock::now(), gid);
        locker_.unlock();
    }

    // 在 unavail list 中找到对应地址放入堆中
    void reconnect(const string &address) {
        locker_.lock();
        // [available_num, end) 都是 濒死 的
        for(int i = access_times_.size() - 1; i >= available_num_; --i) {
            if(access_times_[i].address == address) {
                if(i > available_num_)
                    std::swap(access_times_[available_num_], access_times_[i]);
                ++available_num_;
                std::push_heap(access_times_.begin(), access_times_.begin() + available_num_);
                break;
            }
        }
        health_ump_[address].reconnect();
        locker_.unlock();
    }

    // 返回空说明没得用
    // 可能是有但是 濒死 状态了
    string get_least_access() {
        string address = "";
        locker_.lock();
        assert(!access_times_.empty());
        // 可用取最少的一个
        // ++访问次数后推入堆
        if(available_num_ != 0) {
            address = access_times_.front().address;
            std::pop_heap(access_times_.begin(), access_times_.begin() + available_num_);
            ++access_times_[available_num_ - 1].times;
            std::push_heap(access_times_.begin(), access_times_.begin() + available_num_);
        }
        locker_.unlock();
        return address;
    }

    inline bool empty() {
        locker_.lock_shared();
        bool flag = access_times_.empty();
        locker_.unlock_shared();
        return flag;
    }
    inline bool single() {
        locker_.lock_shared();
        bool flag = access_times_.size() == 1;
        locker_.unlock_shared();
        return flag;
    }
    // 检测是否是重复连接（相同地址但是不是 reconnect）
    inline bool duplicate(const string &address) {
        locker_.lock_shared();
        bool flag = health_ump_.find(address) != health_ump_.end() && health_ump_[address].is_living();
        locker_.unlock_shared();
        return flag;
    }

    // 定时清空所有的访问次数 防止新加入的 dataserver 请求过多
    void reset() {
        locker_.lock();
        for(AccessTime &access_time : access_times_) access_time.times = 0;
        locker_.unlock();
    }

    // 检查组内 dataserver 的健康状况
    // 并进行 dataserver 的修改
    void check() {
        locker_.lock();
        for(auto it = health_ump_.begin(); it != health_ump_.end(); ) {
            const string server_address = it->first;
            HealthCheckClient &health_client = it->second;
            health_client.check();

            // 濒死 && 超时 要删除
            if(!health_client.is_living() && health_client.is_timeout()) {
                LOG_THIS("Delete group " << health_ump_[server_address].get_gid() << " dataserver in " 
                    << server_address << " at " << formatTime(Clock::now()));
                remove_server(server_address);
                health_ump_.erase(it++);
            }
            // 仅濒死要 unavail
            else if(!health_client.is_living()) {
                LOG_THIS("Unavail group " << health_ump_[server_address].get_gid() << " dataserver in " 
                    << server_address << " at " << formatTime(Clock::now()));
                unavail_server(server_address);
                ++it;
            }
            else ++it;
        }
        locker_.unlock();
    }

    bool is_new_connect(const string &address) {
        locker_.lock_shared();
        bool flag = health_ump_.find(address) == health_ump_.end();
        locker_.unlock_shared();
        return flag;
    }

    // 只有上传完数据，组内才是 ready 状态
    void set_ready() {
        locker_.lock();
        for(auto &health_item : health_ump_) {
            health_item.second.set_ready();
        }
        locker_.unlock();
    }

    inline void lock() { locker_.lock(); }
    inline void unlock() { locker_.unlock(); }

private:
    // 将 濒死 dataserver 放入 unavail list 中
    void unavail_server(const string &address) {
        for(int i = 0; i < available_num_; ++i) {
            if(access_times_[i].address == address) {
                if(i < available_num_ - 1)
                    std::swap(access_times_[i], access_times_[available_num_ - 1]);
                --available_num_;
                std::make_heap(access_times_.begin(), access_times_.begin() + available_num_);
            }
        }
    }

    // 在 unavail list 中找到对应地址并删除
    void remove_server(const string &address) {
        // 放到最后删掉
        for(int i = access_times_.size() - 1; i >= available_num_; --i) {
            if(access_times_[i].address == address) {
                if(i < access_times_.size() - 1)
                    std::swap(access_times_[access_times_.size() - 1], access_times_[i]);
                access_times_.pop_back();
                break;
            }
        }
    }

    shared_mutex locker_;
    int available_num_;
    // [avail_list](heap) [unavail_list(near_dead / dead)](unheap)
    vector<AccessTime> access_times_;
    unordered_map<string, HealthCheckClient> health_ump_;
};

#endif