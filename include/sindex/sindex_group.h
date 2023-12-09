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

#include "sindex_model.h"
#include "sindex_util.h"

#if !defined(SINDEX_GROUP_H)
#define SINDEX_GROUP_H

namespace sindex {

template <class key_t, class val_t>
class alignas(CACHELINE_SIZE) Group {
  typedef AtomicVal<val_t> atomic_val_t;
  typedef atomic_val_t wrapped_val_t;
  typedef uint64_t version_t;
  typedef std::pair<key_t, wrapped_val_t> record_t;

  template <class key_tt, class val_tt>
  friend class SIndex;
  template <class key_tt, class val_tt>
  friend class Root;

 public:
  Group();
  ~Group();
  void init(const typename std::vector<key_t>::const_iterator &keys_begin,
            const typename std::vector<val_t>::const_iterator &vals_begin,
            uint32_t array_size);
  void init(const typename std::vector<key_t>::const_iterator &keys_begin,
            const typename std::vector<val_t>::const_iterator &vals_begin,
            uint32_t model_n, uint32_t array_size);

  result_t get(const key_t &key, val_t &val);
  double mean_error_est() const;
  double get_mean_error() const;

  void free_data();

 private:
  size_t locate_model(const key_t &key);

  bool get_from_array(const key_t &key, val_t &val);
  result_t update_to_array(const key_t &key, const val_t &val,
                                  const uint32_t worker_id);

  size_t get_pos_from_array(const key_t &key);
  size_t binary_search_key(const key_t &key, size_t pos_hint,
                                  size_t search_begin, size_t search_end);
  void init_models(uint32_t model_n);
  void init_models(uint32_t model_n, size_t p_len, size_t f_len);
  void init_feature_length();
  double train_model(size_t model_i, size_t begin, size_t end);


  double *get_model(size_t model_i) const;
  const uint8_t *get_model_pivot(size_t model_i) const;
  void set_model_pivot(size_t model_i, const key_t &key);
  void get_model_error(size_t model_i, int &error_max,
                              int &error_min) const;
  void set_model_error(size_t model_i, int error_max, int error_min);

  void prepare_last(size_t model_i,
                           const std::vector<double *> &model_keys,
                           const std::vector<size_t> &positions);
  size_t get_error_bound(size_t model_i,
                                const std::vector<double *> &model_key_ptrs,
                                const std::vector<size_t> &positions) const;
  void predict_last(size_t model_i, const key_t &key, size_t &pos,
                           int &error_min, int &error_max) const;
  size_t predict(size_t model_i, const key_t &key) const;
  size_t predict(size_t model_i, const double *model_key) const;
  bool key_less_than(const uint8_t *k1, const uint8_t *k2,
                            size_t len) const;

  key_t pivot;
  // make array_size atomic because we don't want to acquire lock during `get`.
  // it is okay to obtain a stale (smaller) array_size during `get`.
  uint32_t array_size;              // 4B
  uint8_t model_n = 0;              // 1B
  uint8_t prefix_len = 0;           // 1B
  uint8_t feature_len = 0;          // 1B
  record_t *data = nullptr;         // 8B
  Group *next = nullptr;            // 8B
  int32_t capacity = 0;         // 4B
  uint16_t pos_last_pivot = 0;  // 2B
  volatile uint8_t lock = 0;    // 1B
  // model data

  // 先存了 pivot 信息
  // 还有错误信息
  // 多个 model 都是为了 predict 同一个 group？
  // 然后每个 model 都有 feature_len + 1(偏置) 个参数
  std::array<uint8_t,
             (max_model_n - 1) * sizeof(key_t) +
                 max_model_n * sizeof(double) * (key_t::model_key_size() + 1)>
      model_info;
};

}  // namespace sindex

#endif  // SINDEX_GROUP_H
