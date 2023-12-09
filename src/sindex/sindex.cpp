/*
 * The code is part of the SIndex project.
 *
 *    Copyright (C) 2020 Institute of Parallel and Distributed Systems (IPADS),
 * Shanghai Jiao Tong University. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <vector>
#include <unistd.h>

#include "sindex.h"
#include "index.h"

namespace sindex {
template class SIndex<index_key_t, uint64_t>;

template <class key_t, class val_t>
SIndex<key_t, val_t>::SIndex(const std::vector<key_t> &keys,
                                  const std::vector<val_t> &vals,
                                  size_t worker_num)
    {
  config.worker_n = worker_num;
  // sanity checks
  INVARIANT(config.root_error_bound > 0);
  INVARIANT(config.root_memory_constraint > 0);
  INVARIANT(config.group_error_bound > 0);
  INVARIANT(config.group_error_tolerance > 0);
  INVARIANT(config.buffer_size_bound > 0);
  INVARIANT(config.buffer_size_tolerance > 0);
  INVARIANT(config.buffer_compact_threshold > 0);
  INVARIANT(config.worker_n > 0);

  // 确保是排序好的
  for (size_t key_i = 1; key_i < keys.size(); key_i++) {
    assert(keys[key_i] >= keys[key_i - 1]);
  }

  // ?
  rcu_init();

  // malloc memory for root & init root
  root = new root_t();
  root->init(keys, vals);
  // start_bg();
}

template <class key_t, class val_t>
SIndex<key_t, val_t>::~SIndex() {
  // delete root?
  // terminate_bg();
  delete root;
}

template <class key_t, class val_t>
inline bool SIndex<key_t, val_t>::get(const key_t &key, val_t &val,
                                           const uint32_t worker_id) {
  rcu_progress(worker_id);
  return root->get(key, val) == result_t::ok;
}

}  // namespace sindex

