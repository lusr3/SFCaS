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
  delete[] model_weights;
  model_weights = nullptr;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init(
    const typename std::vector<key_t>::const_iterator &keys_begin,
    const typename std::vector<val_t>::const_iterator &vals_begin,
    uint32_t array_size, struct needle_index *needle_begin,
    uint64_t start) {
  assert(array_size > 0);
  this->pivot = *keys_begin;
  this->array_size = array_size;

  this->data = new record_t[this->array_size];

  for (size_t rec_i = 0; rec_i < array_size; rec_i++) {
    data[rec_i].first = *(keys_begin + rec_i);
    data[rec_i].second = *(vals_begin + rec_i);
  }

  for (size_t rec_i = 1; rec_i < array_size; rec_i++) {
    assert(data[rec_i].first >= data[rec_i - 1].first);
  }

  this->needle_begin = needle_begin;
  this->start = start;
  init_models();

  free_data();
}

template <class key_t, class val_t>
void Group<key_t, val_t>::free_data() {
  delete[] data;
  data = nullptr;
}

template <class key_t, class val_t>
void Group<key_t, val_t>::init_models() {
  init_feature_length();

  // +1 for bias
  model_weights = new double[feature_len + 1];

  train_model(0, array_size);
}

// 为了获取该组内的 prefixlen 和 feature_len
// feature_len 是实际使用字符长度
template <class key_t, class val_t>
void Group<key_t, val_t>::init_feature_length() {
  const size_t key_size = sizeof(key_t);
  if (array_size < 2) {
    prefix_len = (uint32_t)key_size;
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

  // 为了区分两个 key 需要 +1
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
}

// 训练模型参数同时得到误差范围
template <class key_t, class val_t>
inline void Group<key_t, val_t>::prepare_last(
    const std::vector<double *> &model_key_ptrs,
    const std::vector<size_t> &positions) {
  model_prepare(model_key_ptrs, positions, model_weights, feature_len);

  // calculate error info
  int64_t pos_error = 0, neg_error = 0;
  for (size_t key_i = 0; key_i < model_key_ptrs.size(); ++key_i) {
    long long int pos_actual = positions[key_i];
    long long int pos_pred = predict(model_key_ptrs[key_i]);
    long long int error = pos_actual - pos_pred;
    if (error > pos_error) pos_error = error;
    if (error < neg_error) neg_error = error;
  }
  set_model_error(pos_error, neg_error);
}

template <class key_t, class val_t>
inline result_t Group<key_t, val_t>::get(
  const key_t &key, val_t &val) const {
  int64_t pos_pred = predict(key);
  int64_t search_begin = pos_pred + max_neg_error,
  search_end = pos_pred + max_pos_error;
  // search within the range
  if(search_begin < 0) search_begin = 0;
  if(search_begin >= (int64_t)this->array_size) search_begin = this->array_size - 1;
  if(search_end < 0) search_end = 0;
  if(search_end >= (int64_t)this->array_size) search_end = this->array_size - 1;
  // 在预测出来的 pos 和误差范围内二分查找
  size_t pos = binary_search_key(key, pos_pred, search_begin, search_end);
  DEBUG_THIS("predict pos: " << pos);
  DEBUG_THIS("predict name: " << needle_begin[pos].filename);
  DEBUG_THIS("actual name: " << key);
  if(pos != array_size && needle_begin[pos].filename == key) {
    val = start + pos;
    return result_t::ok;
  }
  return result_t::failed;
}

// [search_begin, search_end]
template <class key_t, class val_t>
inline size_t Group<key_t, val_t>::binary_search_key(
    const key_t &key, size_t pos, size_t search_begin, size_t search_end) const {
  assert(search_begin <= search_end);
  if(search_begin == search_end) return search_begin;
  size_t mid = (pos >= search_begin && pos <= search_end) ? pos : (search_begin + search_end) / 2;;
  while (search_end != search_begin) {
    if (needle_begin[mid].filename.less_than(key, prefix_len, feature_len)) {
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
inline void Group<key_t, val_t>::get_model_error(
  int64_t &error_pos, int64_t &error_neg) const {
  error_pos = max_pos_error;
  error_neg = max_neg_error;
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::set_model_error(
  int64_t error_pos, int64_t error_neg) {
  max_pos_error = error_pos;
  max_neg_error = error_neg;
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

template <class key_t, class val_t>
inline void Group<key_t, val_t>::save_group_model(FILE *model_file) const {
  fwrite(&array_size, sizeof(array_size), 1, model_file);
  fwrite(&prefix_len, sizeof(prefix_len), 1, model_file);
  fwrite(&feature_len, sizeof(feature_len), 1, model_file);
  fwrite(&max_pos_error, sizeof(max_pos_error), 1, model_file);
  fwrite(&max_neg_error, sizeof(max_neg_error), 1, model_file);
  fwrite(model_weights, sizeof(double), feature_len + 1, model_file);
}

template <class key_t, class val_t>
inline void Group<key_t, val_t>::read_group_model(FILE *model_file, needle_index *needle_begin, uint64_t start) {
  this->pivot = needle_begin->filename;
  this->needle_begin = needle_begin;
  this->start = start;
  fread(&array_size, sizeof(array_size), 1, model_file);
  fread(&prefix_len, sizeof(prefix_len), 1, model_file);
  fread(&feature_len, sizeof(feature_len), 1, model_file);
  fread(&max_pos_error, sizeof(max_pos_error), 1, model_file);
  fread(&max_neg_error, sizeof(max_neg_error), 1, model_file);
  model_weights = new double[feature_len + 1];
  fread(model_weights, sizeof(double), feature_len + 1, model_file);
}

}  // namespace sindex
