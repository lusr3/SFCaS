#include "sindex_util.h"
#include "helper.h"

namespace sindex {

index_config_t config;
std::mutex config_mutex;

void rcu_init() {
  config_mutex.lock();
  if (config.rcu_status.get() == nullptr) {
    config.rcu_status = std::make_unique<rcu_status_t[]>(config.worker_n);
    for (size_t worker_i = 0; worker_i < config.worker_n; worker_i++) {
      config.rcu_status[worker_i].status = 0;
      config.rcu_status[worker_i].waiting = false;
    }
  }
  config_mutex.unlock();
}

void rcu_progress(const uint32_t worker_id) {
  config.rcu_status[worker_id].status++;
}

// wait for all workers
void rcu_barrier() {
  int64_t prev_status[config.worker_n];
  for (size_t w_i = 0; w_i < config.worker_n; w_i++) {
    prev_status[w_i] = config.rcu_status[w_i].status;
  }
  for (size_t w_i = 0; w_i < config.worker_n; w_i++) {
    while (config.rcu_status[w_i].status <= prev_status[w_i] && !config.exited)
      ;
  }
}

// wait for workers whose 'waiting' is false
void rcu_barrier(const uint32_t worker_id) {
  // set myself to waiting for barrier
  config.rcu_status[worker_id].waiting = true;

  int64_t prev_status[config.worker_n];
  for (size_t w_i = 0; w_i < config.worker_n; w_i++) {
    prev_status[w_i] = config.rcu_status[w_i].status;
  }
  for (size_t w_i = 0; w_i < config.worker_n; w_i++) {
    // skipped workers that is wating for barrier (include myself)
    while (config.rcu_status[w_i].status <= prev_status[w_i] &&
           !config.rcu_status[w_i].waiting && !config.exited)
      ;
  }
  config.rcu_status[worker_id].waiting = false;  // restore my state
}

}