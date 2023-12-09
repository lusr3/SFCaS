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

#include <climits>
#include <vector>

#include "sindex_group.h"
#include "index.h"

namespace sindex {
template class Group<index_key_t, uint64_t>;

template <class key_t, class val_t>
Group<key_t, val_t>::Group() {}

template <class key_t, class val_t>
Group<key_t, val_t>::~Group() {}

template <class key_t, class val_t>
void Group<key_t, val_t>::init(
    const typename std::vector<key_t>::const_iterator &keys_begin,
    const typename std::vector<val_t>::const_iterator &vals_begin,
    uint32_t array_size) {
  init(keys_begin, vals_begin, 1, array_size);
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init(
    const typename std::vector<key_t>::const_iterator &keys_begin,
    const typename std::vector<val_t>::const_iterator &vals_begin,
    uint32_t model_n, uint32_t array_size) {
  assert(array_size > 0);
  this->pivot = *keys_begin;
  this->array_size = array_size;
  this->capacity = array_size;
  this->model_n = model_n;
  // 为什么要两倍的 array_size
  data = new record_t[this->capacity]();
  // buffer = new buffer_t();

  for (size_t rec_i = 0; rec_i < array_size; rec_i++) {
    data[rec_i].first = *(keys_begin + rec_i);
    data[rec_i].second = wrapped_val_t(*(vals_begin + rec_i));
  }

  for (size_t rec_i = 1; rec_i < array_size; rec_i++) {
    assert(data[rec_i].first >= data[rec_i - 1].first);
  }

  init_models(model_n);
}

template <class key_t, class val_t>
inline result_t Group<key_t, val_t>::get(const key_t &key,
                                                           val_t &val) {
  if (get_from_array(key, val)) {
    return result_t::ok;
  }
  return result_t::failed;
}

template <class key_t, class val_t>
double Group<key_t, val_t>::mean_error_est() const {
  // we did not disable seq op here so array_size can be changed.
  // however, we only need an estimated error
  uint32_t array_size = this->array_size;
  size_t model_data_size = array_size - pos_last_pivot;
  std::vector<double> model_keys(model_data_size * feature_len);
  std::vector<double *> model_key_ptrs(model_data_size);
  std::vector<size_t> positions(model_data_size);
  for (size_t rec_i = 0; rec_i < model_data_size; rec_i++) {
    data[pos_last_pivot + rec_i].first.get_model_key(
        prefix_len, feature_len, model_keys.data() + feature_len * rec_i);
    model_key_ptrs[rec_i] = model_keys.data() + feature_len * rec_i;
    positions[rec_i] = pos_last_pivot + rec_i;
  }
  double error_last_model_now =
      get_error_bound(model_n - 1, model_key_ptrs, positions);

  if (model_n == 1) {
    return error_last_model_now;
  } else {
    // est previous last model error
    size_t model_data_size_prev_est = pos_last_pivot / (model_n - 1);
    if (model_data_size_prev_est > model_data_size) {
      model_data_size_prev_est = model_data_size;
    }
    model_key_ptrs.resize(model_data_size_prev_est);
    positions.resize(model_data_size_prev_est);
    double error_last_model_prev =
        get_error_bound(model_n - 1, model_key_ptrs, positions);

    return (error_last_model_now - error_last_model_prev) / model_n;
  }
}

template <class key_t, class val_t>
double Group<key_t, val_t>::get_mean_error() const {
  double mean_err = 0;
  int err_max = 0, err_min = 0;
  for (size_t m_i = 0; m_i < model_n; ++m_i) {
    get_model_error(m_i, err_max, err_min);
    mean_err += (err_max < err_min ? INT_MAX : err_max - err_min + 1);
  }
  return mean_err / model_n;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::free_data() {
  delete[] data;
  data = nullptr;
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::locate_model(
    const key_t &key) {
  assert(model_n >= 1);

  int model_i = 0;
  while (model_i < model_n - 1 &&
         !key_less_than((uint8_t *)&key + prefix_len,
                        get_model_pivot(model_i + 1), feature_len)) {
    model_i++;
  }
  return model_i;
}

// semantics: atomically read the value
// only when the key exists and the record (record_t) is not logical removed,
// return true on success
template <class key_t, class val_t>
inline bool Group<key_t, val_t>::get_from_array(
    const key_t &key, val_t &val) {
  size_t pos = get_pos_from_array(key);
  return pos != array_size &&  // position is valid (not out-of-range)
         data[pos].first ==
             key &&  // key matches, must use full key comparison here
         data[pos].second.read(val);  // value is not removed
}

template <class key_t, class val_t>
inline result_t Group<key_t, val_t>::update_to_array(
    const key_t &key, const val_t &val, const uint32_t worker_id) {
    size_t pos = get_pos_from_array(key);
    return pos != array_size && data[pos].first == key &&
                   data[pos].second.update(val)
               ? result_t::ok
               : result_t::failed;
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::get_pos_from_array(
    const key_t &key) {
  size_t model_i = locate_model(key);
  size_t pos;
  int error_min, error_max;
  predict_last(model_i, key, pos, error_min, error_max);
  // error_min + pos 可能小于 pos 吗？？？
  size_t search_begin = unlikely((error_min < 0 && (error_min + pos) > pos))
                            ? 0
                            : pos + error_min;
  size_t search_end = pos + error_max + 1;
  if (error_min > error_max) {
    search_begin = 0;
    search_end = array_size;
  }
  // 在预测出来的 pos 和误差范围内二分查找
  return binary_search_key(key, pos, search_begin, search_end);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::binary_search_key(
    const key_t &key, size_t pos, size_t search_begin, size_t search_end) {
  // search within the range
  if (unlikely(search_begin > array_size)) {
    search_begin = array_size;
  }
  if (unlikely(search_end > array_size)) {
    search_end = array_size;
  }
  size_t mid = pos >= search_begin && pos < search_end
                   ? pos
                   : (search_begin + search_end) / 2;
  while (search_end != search_begin) {
    if (data[mid].first.less_than(key, prefix_len, feature_len)) {
      search_begin = mid + 1;
    } else {
      search_end = mid;
    }
    mid = (search_begin + search_end) / 2;
  }
  assert(search_begin == search_end);
  assert(search_begin == mid);

  return mid;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init_models(uint32_t model_n) {
  // not need to init prefix and models every time init_models
  init_feature_length();
  init_models(model_n, prefix_len, feature_len);
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init_models(uint32_t model_n,
                                                        size_t p_len,
                                                        size_t f_len) {
  assert(model_n == 1);
  this->model_n = model_n;
  prefix_len = p_len;
  feature_len = f_len;

  size_t records_per_model = array_size / model_n;
  size_t trailing_n = array_size - records_per_model * model_n;
  size_t begin = 0;
  for (size_t model_i = 0; model_i < model_n; ++model_i) {
    size_t end = begin + records_per_model;
    if (trailing_n > 0) {
      end++;
      trailing_n--;
    }
    assert(end <= array_size);
    assert((model_i == model_n - 1 && end == array_size) ||
           model_i < model_n - 1);

    set_model_pivot(model_i, data[begin].first);
    train_model(model_i, begin, end);
    begin = end;
    if (model_i == model_n - 1) pos_last_pivot = begin;
  }
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init_feature_length() {
  const size_t key_size = sizeof(key_t);
  if (array_size < 2) {
    prefix_len = key_size;
    return;
  }

  prefix_len = common_prefix_length(0, (uint8_t *)&data[0].first, key_size,
                                    (uint8_t *)&data[1].first, key_size);
  size_t max_adjacent_prefix = prefix_len;

  for (size_t k_i = 2; k_i < array_size; ++k_i) {
    prefix_len =
        common_prefix_length(0, (uint8_t *)&data[k_i - 1].first, prefix_len,
                             (uint8_t *)&data[k_i].first, key_size);
    size_t adjacent_prefix =
        common_prefix_length(prefix_len, (uint8_t *)&data[k_i - 1].first,
                             key_size, (uint8_t *)&data[k_i].first, key_size);
    assert(adjacent_prefix <= sizeof(key_t) - prefix_len);
    // == 意味着有两个相同的 key
    if (adjacent_prefix < sizeof(key_t) - prefix_len) {
      max_adjacent_prefix =
          std::max(max_adjacent_prefix, prefix_len + adjacent_prefix);
    }
  }
  // +1 是 bias
  feature_len = max_adjacent_prefix - prefix_len + 1;
  assert(prefix_len <= sizeof(key_t));
  assert(feature_len <= sizeof(key_t));
}

template <class key_t, class val_t>
inline double Group<key_t, val_t>::train_model(size_t model_i,
                                                                 size_t begin,
                                                                 size_t end) {
  assert(end >= begin);
  assert(array_size >= end);

  size_t model_data_size = end - begin;
  std::vector<double> model_keys(model_data_size * feature_len);
  std::vector<double *> model_key_ptrs(model_data_size);
  std::vector<size_t> positions(model_data_size);

  for (size_t rec_i = 0; rec_i < model_data_size; rec_i++) {
    data[begin + rec_i].first.get_model_key(
        prefix_len, feature_len, model_keys.data() + rec_i * feature_len);
    model_key_ptrs[rec_i] = model_keys.data() + rec_i * feature_len;
    positions[rec_i] = begin + rec_i;
  }

  prepare_last(model_i, model_key_ptrs, positions);
  int err_max = 0, err_min = 0;
  get_model_error(model_i, err_max, err_min);
  return err_max < err_min ? INT_MAX : err_max - err_min;
}

template <class key_t, class val_t>
inline double *Group<key_t, val_t>::get_model(
    size_t model_i) const {
  assert(model_i < model_n);
  size_t aligned_f_len = (feature_len + 7) & (~7);
  size_t pivots_size = (model_n - 1) * aligned_f_len;
  return (double *)(model_info.data() + pivots_size) +  // skip pivots
         ((size_t)feature_len + 2) * model_i;
}

template <class key_t, class val_t>
inline const uint8_t *Group<key_t, val_t>::get_model_pivot(
    size_t model_i) const {
  assert(model_i < model_n);
  size_t aligned_f_len = (feature_len + 7) & (~7);
  return model_i == 0 ? (uint8_t *)&pivot + prefix_len
                      : model_info.data() + aligned_f_len * (model_i - 1);
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::set_model_pivot(
    size_t model_i, const key_t &key) {
  assert(model_i < model_n);
  if (model_i == 0) return;
  size_t aligned_f_len = (feature_len + 7) & (~7);
  memcpy(model_info.data() + aligned_f_len * (model_i - 1),
         (uint8_t *)&key + prefix_len, feature_len);
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::get_model_error(
    size_t model_i, int &error_max, int &error_min) const {
  int32_t *error_info =
      (int32_t *)(get_model(model_i) + (size_t)feature_len + 1);
  error_max = error_info[0];
  error_min = error_info[1];
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::set_model_error(
    size_t model_i, int error_max, int error_min) {
  int32_t *error_info =
      (int32_t *)(get_model(model_i) + (size_t)feature_len + 1);
  error_info[0] = error_max;
  error_info[1] = error_min;
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::prepare_last(
    size_t model_i, const std::vector<double *> &model_key_ptrs,
    const std::vector<size_t> &positions) {
  model_prepare(model_key_ptrs, positions, get_model(model_i), feature_len);

  // calculate error info
  int error_max = INT_MIN, error_min = INT_MAX;
  for (size_t key_i = 0; key_i < model_key_ptrs.size(); ++key_i) {
    long long int pos_actual = positions[key_i];
    long long int pos_pred = predict(model_i, model_key_ptrs[key_i]);
    long long int error = pos_actual - pos_pred;
    // when int error is overflowed, set max<min, so user can know
    if (error > INT_MAX || error < INT_MIN) {
      error_max = -1;
      error_min = 1;
      return;
    }
    if (error > error_max) error_max = error;
    if (error < error_min) error_min = error;
  }
  set_model_error(model_i, error_max, error_min);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::get_error_bound(
    size_t model_i, const std::vector<double *> &model_key_ptrs,
    const std::vector<size_t> &positions) const {
  long long int max = 0;
  for (size_t key_i = 0; key_i < model_key_ptrs.size(); ++key_i) {
    long long int pos_actual = positions[key_i];
    long long int pos_pred = predict(model_i, model_key_ptrs[key_i]);
    long long int error = std::abs(pos_actual - pos_pred);
    if (error > INT_MAX) return INT_MAX;
    if (error > max) max = error;
  }
  return max;
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::predict_last(
    size_t model_i, const key_t &key, size_t &pos, int &error_min,
    int &error_max) const {
  pos = predict(model_i, key);
  get_model_error(model_i, error_max, error_min);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::predict(
    size_t model_i, const key_t &key) const {
  double model_key[feature_len];
  key.get_model_key(prefix_len, feature_len, model_key);
  return model_predict(get_model(model_i), model_key, feature_len);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::predict(
    size_t model_i, const double *model_key) const {
  return model_predict(get_model(model_i), model_key, feature_len);
}

// TODO unify compare interface
template <class key_t, class val_t>
inline bool Group<key_t, val_t>::key_less_than(
    const uint8_t *k1, const uint8_t *k2, size_t len) const {
  return memcmp(k1, k2, len) < 0;
}

}  // namespace sindex
