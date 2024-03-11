#include "util.h"

static string IP = "localhost";
static string PORT = "50001";
static const std::chrono::seconds TIMEOUT(60);
static const std::chrono::seconds CHECK_BASE(8);
static const std::chrono::seconds CHECK_INTERVAL(10);
static const std::chrono::minutes TIME_CLEAN(10);

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

// 多个 dataserver 组成一个组，形成 datacluster
class DataCluster {
public:
    DataCluster() : available_num_(0) {}

    // 加入新 server 到可用末尾并重新建堆
    void add_server(const AccessTime &access_time) {
        access_locker_.lock();
        access_times_.push_back(access_time);
        // 表示当前有 濒死 状态的 dataserver
        if(available_num_ < access_times_.size() - 1)
            std::swap(access_times_.back(), access_times_[available_num_]);
        ++available_num_;
        std::push_heap(access_times_.begin(), access_times_.begin() + available_num_);
        access_locker_.unlock();
    }

    // 将 濒死 dataserver 放入 unavail list 中
    void unavail_server(const string &address) {
        access_locker_.lock();
        for(int i = 0; i < available_num_; ++i) {
            if(access_times_[i].address == address) {
                if(i < available_num_ - 1)
                    std::swap(access_times_[i], access_times_[available_num_ - 1]);
                --available_num_;
                std::make_heap(access_times_.begin(), access_times_.begin() + available_num_);
            }
        }
        access_locker_.unlock();
    }

    // 在 unavail list 中找到对应地址并删除
    void remove_server(const string &address) {
        access_locker_.lock();
        // 放到最后删掉
        for(int i = access_times_.size() - 1; i >= available_num_; --i) {
            if(access_times_[i].address == address) {
                if(i < access_times_.size() - 1)
                    std::swap(access_times_[access_times_.size() - 1], access_times_[i]);
                access_times_.pop_back();
                break;
            }
        }
        access_locker_.unlock();
    }

    // 在 unavail list 中找到对应地址放入堆中
    void reconnect(const string &address) {
        access_locker_.lock();
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
        access_locker_.unlock();
    }

    // 返回空说明没得用
    // 可能是有但是 濒死 状态了
    string get_least_access() {
        string address = "";
        access_locker_.lock();
        assert(!access_times_.empty());
        // 可用取最少的一个
        // ++访问次数后推入堆
        if(available_num_ != 0) {
            address = access_times_.front().address;
            std::pop_heap(access_times_.begin(), access_times_.begin() + available_num_);
            ++access_times_[available_num_ - 1].times;
            std::push_heap(access_times_.begin(), access_times_.begin() + available_num_);
        }
        access_locker_.unlock();
        return address;
    }

    inline bool empty() { return access_times_.empty(); }
    string get_single_address() {
        access_locker_.lock();
        assert(access_times_.size() == 1);
        access_locker_.unlock();
        return access_times_[0].address;
    }

    // 定时清空所有的访问次数 防止新加入的 dataserver 请求过多
    void reset() {
        access_locker_.lock();
        for(AccessTime &access_time : access_times_) access_time.times = 0;
        access_locker_.unlock();
    }

private:
    mutex access_locker_;
    int available_num_;
    // [avail_list](heap) [unavail_list(near_dead / dead)](unheap)
    vector<AccessTime> access_times_;
};