#include <climits>
#include <vector>

#include "sindex_group.h"
#include "index.h"

namespace sindex {
template class Group<index_key_t, uint64_t>;

template <class key_t, class val_t>
Group<key_t, val_t>::Group() {}

template <class key_t, class val_t>
Group<key_t, val_t>::~Group() {
  free_data();
  delete[] model_weights;
  model_weights = nullptr;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init(
    const typename std::vector<key_t>::const_iterator &keys_begin,
    const typename std::vector<val_t>::const_iterator &vals_begin,
    uint32_t array_size) {
  assert(array_size > 0);
  this->pivot = *keys_begin;
  this->array_size = array_size;
  // 存放 key - pos
  data = new record_t[this->array_size];

  for (size_t rec_i = 0; rec_i < array_size; rec_i++) {
    data[rec_i].first = *(keys_begin + rec_i);
    data[rec_i].second = *(vals_begin + rec_i);
  }
  for (size_t rec_i = 1; rec_i < array_size; rec_i++) {
    assert(data[rec_i].first >= data[rec_i - 1].first);
  }

  init_models();
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init_models() {
  init_feature_length();
  // for bias?
  model_weights = new double[feature_len + 1];

  train_model(0, array_size);
}

template <class key_t, class val_t>
inline result_t Group<key_t, val_t>::get(
  const key_t &key, val_t &val) {
  if (get_from_array(key, val)) {
    return result_t::ok;
  }
  return result_t::failed;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::free_data() {
  delete[] data;
  data = nullptr;
}

// semantics: atomically read the value
// only when the key exists and the record (record_t) is not logical removed,
// return true on success
template <class key_t, class val_t>
inline bool Group<key_t, val_t>::get_from_array(
    const key_t &key, val_t &val) {
  size_t pos = get_pos_from_array(key);
  // position is valid (not out-of-range)
  // key matches, must use full key comparison here
  if(pos != array_size && data[pos].first == key) {
    val = data[pos].second;
    return true;
  }
  return false;
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::get_pos_from_array(
    const key_t &key) {
  size_t pos = predict(key);
  // error_neg + pos 可能小于 pos 吗？？？
  // size_t search_begin = unlikely((error_neg < 0 && (error_neg + pos) > pos))
  //                           ? 0
  //                           : pos + error_neg;
  // size_t search_end = pos + error_pos + 1;
  // if (error_neg > error_pos) {
  //   search_begin = 0;
  //   search_end = array_size;
  // }
  print_error("pos %ld, pos_err %ld, neg_err %ld\n", pos, my_error_pos, my_error_neg);
  // 注意符号问题！！！
  int64_t search_begin = pos + my_error_neg,
  search_end = pos + my_error_pos + 1;
  if(search_begin < 0) search_begin = 0;
  if(search_end > array_size) search_end = array_size;
  // 在预测出来的 pos 和误差范围内二分查找
  print_error("begin %ld, end %ld\n", search_begin, search_end);
  return binary_search_key(key, pos, search_begin, search_end);
}

// [search_begin, search_end)
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
  // std::cout << search_begin << " " << search_end << std::endl;
  assert(search_begin <= search_end);
  size_t mid = pos >= search_begin && pos < search_end
                   ? pos
                   : (search_begin + search_end) / 2;
  // [left, right)
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


// 为了获取该组内的 prefixlen 和 feature_len
// feature_len 是实际使用字符长度
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
  // 不是偏置，是为了区分才 +1
  feature_len = max_adjacent_prefix - prefix_len + 1;
  assert(prefix_len <= sizeof(key_t));
  assert(feature_len <= sizeof(key_t));
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::train_model(size_t begin, size_t end) {
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

  prepare_last(model_key_ptrs, positions);
  // int err_max = 0, err_min = 0;
  // get_model_error(err_max, err_min);
  // return err_max < err_min ? INT_MAX : err_max - err_min;
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::get_model_error(
  int &error_pos, int &error_neg) const {
  error_pos = my_error_pos;
  error_neg = my_error_neg;
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::set_model_error(
  int error_pos, int error_neg) {
  my_error_pos = error_pos;
  my_error_neg = error_neg;
}

// 训练模型参数同时得到误差范围
template <class key_t, class val_t>
inline void Group<key_t, val_t>::prepare_last(
    const std::vector<double *> &model_key_ptrs,
    const std::vector<size_t> &positions) {
  model_prepare(model_key_ptrs, positions, model_weights, feature_len);

  // calculate error info
  int64_t error_pos = 0, error_neg = 0;
  for (size_t key_i = 0; key_i < model_key_ptrs.size(); ++key_i) {
    long long int pos_actual = positions[key_i];
    long long int pos_pred = predict(model_key_ptrs[key_i]);
    long long int error = pos_pred - pos_actual;
    // print_error("pos_pred %lld, pos_actual %lld\n", pos_pred, pos_actual);
    // TODO: 考虑溢出
    // when int error is overflowed, set max<min, so user can know
    // if (error > INT_MAX || error < INT_MIN) {
    //   error_pos = -1;
    //   error_neg = 1;
    //   return;
    // }
    if (error > error_pos) error_pos = error;
    if (error < error_neg) error_neg = error;
  }
  set_model_error(error_pos, error_neg);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::predict(const key_t &key) const {
  double model_key[feature_len];
  key.get_model_key(prefix_len, feature_len, model_key);
  return model_predict(model_weights, model_key, feature_len);
}

template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::predict(const double *model_key) const {
  return model_predict(model_weights, model_key, feature_len);
}

// TODO unify compare interface
// template <class key_t, class val_t>
// inline bool Group<key_t, val_t>::key_less_than(
//     const uint8_t *k1, const uint8_t *k2, size_t len) const {
//   return memcmp(k1, k2, len) < 0;
// }

}  // namespace sindex
