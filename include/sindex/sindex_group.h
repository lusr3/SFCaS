#include "sindex_model.h"
#include "sindex_util.h"

#if !defined(SINDEX_GROUP_H)
#define SINDEX_GROUP_H

namespace sindex {

template <class key_t, class val_t>
class alignas(CACHELINE_SIZE) Group {
  typedef std::pair<key_t, val_t> record_t;

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
  // void init(const typename std::vector<key_t>::const_iterator &keys_begin,
  //           const typename std::vector<val_t>::const_iterator &vals_begin,
  //           uint32_t model_n, uint32_t array_size);

  result_t get(const key_t &key, val_t &val);

  void free_data();

 private:

  bool get_from_array(const key_t &key, val_t &val);

  size_t get_pos_from_array(const key_t &key);
  size_t binary_search_key(const key_t &key, size_t pos_hint,
                                  size_t search_begin, size_t search_end);
  void init_models();
  void init_feature_length();
  void train_model(size_t begin, size_t end);

  void get_model_error(int &error_max,
                              int &error_min) const;
  void set_model_error(int error_max, int error_min);

  void prepare_last(const std::vector<double *> &model_keys,
                    const std::vector<size_t> &positions);
  size_t predict(const key_t &key) const;
  size_t predict(const double *model_key) const;
  // bool key_less_than(const uint8_t *k1, const uint8_t *k2,
  //                           size_t len) const;

  key_t pivot;
  // make array_size atomic because we don't want to acquire lock during `get`.
  // it is okay to obtain a stale (smaller) array_size during `get`.
  uint32_t array_size;              // 4B
  uint8_t prefix_len = 0;           // 1B
  uint8_t feature_len = 0;          // 1B
  record_t *data = nullptr;         // 8B
  // 保存的是最大范围的误差
  int64_t my_error_neg;
  int64_t my_error_pos;

  // 先存了 pivot 信息
  // 还有错误信息
  // 多个 model 都是为了 predict 同一个 group？
  // 然后每个 model 都有 feature_len + 1(偏置) 个参数
  // std::array<uint8_t,
  //            (max_model_n - 1) * sizeof(key_t) +
  //                max_model_n * sizeof(double) * (key_t::model_key_size() + 1)>
  //     model_info;
  double *model_weights;
};

}  // namespace sindex

#endif  // SINDEX_GROUP_H
