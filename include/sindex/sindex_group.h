#include "sindex_model.h"
#include "sindex_util.h"
#include "needle.h"

#if !defined(SINDEX_GROUP_H)
#define SINDEX_GROUP_H

namespace sindex {

template <class key_t, class val_t>
class Group {
  typedef std::pair<key_t, val_t> record_t;

  template <class key_tt, class val_tt>
  friend class Root;

 public:
  Group();
  ~Group();
  void init(const typename std::vector<key_t>::const_iterator &keys_begin,
            const typename std::vector<val_t>::const_iterator &vals_begin,
            uint32_t array_size, struct needle_index *needle_begin,
            uint64_t start);

  result_t get(const key_t &key, val_t &val) const;

  void save_group_model(FILE *model_file) const;
  void read_group_model(FILE *model_file, struct needle_index *needle_begin, uint64_t start);

 private:
  // train model
  void init_models();
  void init_feature_length();
  void train_model(size_t begin, size_t end);
  void free_data();

  // get operation
  size_t binary_search_key(const key_t &key, size_t pos_hint,
                                  size_t search_begin, size_t search_end) const;

  void get_model_error(int64_t &error_pos,
                              int64_t &error_neg) const;
  void set_model_error(int64_t error_pos, int64_t error_neg);

  void prepare_last(const std::vector<double *> &model_keys,
                    const std::vector<size_t> &positions);
  size_t predict(const key_t &key) const;
  size_t predict(const double *model_key) const;

  key_t pivot;
  record_t *data = nullptr;         // 8B

  double *model_weights;  
  struct needle_index *needle_begin;
  uint64_t start;

  // 保存的是最大范围的误差
  int64_t max_neg_error;
  int64_t max_pos_error;
  uint32_t array_size;              // 4B
  uint32_t prefix_len = 0;           // 4B
  uint32_t feature_len = 0;          // 4B
};

}  // namespace sindex

#endif  // SINDEX_GROUP_H
