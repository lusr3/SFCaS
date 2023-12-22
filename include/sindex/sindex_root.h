#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <numeric>

#include "sindex_group.h"
#include "needle.h"

#if !defined(SINDEX_ROOT_H)
#define SINDEX_ROOT_H

namespace sindex {

template <class key_t, class val_t>
class Root {
  struct PartialModelMeta;
  typedef Group<key_t, val_t> group_t;
  typedef PartialModelMeta model_meta_t;

  struct PartialModelMeta {
    uint32_t p_len;
    uint32_t f_len;
    // 加 1 是指偏置
    std::vector<double> weights;
  };

public:
  ~Root();
  void init(const std::vector<key_t> &keys, const std::vector<val_t> &vals, 
    std::vector<struct needle_index> &indexs);

  result_t get(const key_t &key, val_t &val);

private:
  // train model
  void grouping_director(const std::vector<key_t> &keys, std::vector<size_t> &indexes);
  void grouping_by_partial_key(bool is_limit, const std::vector<double> model_keys, 
                                      size_t &key_start, size_t key_size,
                                      const std::vector<key_t> &keys, size_t et,
                                      size_t pt, size_t fstep, size_t bstep,
                                      size_t min_size,
                                      std::vector<size_t> &pivot_indexes) const;
  void train_piecewise_model();
  void partial_key_len_of_pivots(const size_t start_i,
                                          const size_t end_i, uint32_t &p_len,
                                          uint32_t &f_len);
  void partial_key_len_by_step(
      const std::vector<key_t> &keys, const size_t start,
      const size_t step_start, const size_t step_end, size_t &common_p_len,
      size_t &max_p_len, std::unordered_map<size_t, size_t> &common_p_history,
      std::unordered_map<size_t, size_t> &max_p_history) const;
  double train_and_get_err(std::vector<double> &model_key, size_t start,
                                  size_t end, size_t p_len, size_t f_len) const;
  
  // get operation
  size_t predict(const key_t &key);
  size_t predict(const double *model_key, uint32_t model_i);
  group_t *locate_group(const key_t &key);

  void set_group_ptr(size_t group_i, group_t *g_ptr);
  group_t *get_group_ptr(size_t group_i) const;
  void set_group_pivot(size_t group_i, const key_t &key);
  key_t &get_group_pivot(size_t group_i) const;

  std::unique_ptr<std::pair<key_t, group_t *>[]> groups;  // 8B
  uint32_t group_n = 0;                                           // 4B
  uint32_t root_model_n = 0;                                      // 4B
  std::array<key_t, max_root_model_n> model_pivots;
  std::array<model_meta_t, max_root_model_n> models;
  // std::array<std::pair<int64_t, int64_t>, max_root_model_n> model_errors;
};

}  // namespace sindex

#endif  // SINDEX_ROOT_H
