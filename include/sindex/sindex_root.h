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

#include "sindex_group.h"
#include <unordered_map>

#if !defined(SINDEX_ROOT_H)
#define SINDEX_ROOT_H

namespace sindex {

template <class key_t, class val_t>
class Root {
  struct PartialModelMeta;
  typedef Group<key_t, val_t> group_t;
  typedef PartialModelMeta model_meta_t;

  template <class key_tt, class val_tt>
  friend class SIndex;

  struct PartialModelMeta {
    uint32_t p_len;
    uint32_t f_len;
    std::array<double, key_t::model_key_size() + 1> weights;
  };

 public:
  ~Root();
  void init(const std::vector<key_t> &keys, const std::vector<val_t> &vals);

 result_t get(const key_t &key, val_t &val);
  Root *create_new_root();
  void trim_root();

 private:
  void adjust_root_model();
  void train_piecewise_model();
 void partial_key_len_of_pivots(const size_t start_i,
                                        const size_t end_i, uint32_t &p_len,
                                        uint32_t &f_len);
 size_t predict(const key_t &key);
 size_t predict(const double *model_key);
 size_t pick_next_stage_model(size_t pos_pred);
 double *get_2nd_stage_model(const size_t model_i);
 group_t *locate_group(const key_t &key);
 group_t *locate_group_pt1(const key_t &key, int &group_i);
 group_t *locate_group_pt2(const key_t &key, group_t *begin);

 void set_group_ptr(size_t group_i, group_t *g_ptr);
 group_t *get_group_ptr(size_t group_i) const;
 void set_group_pivot(size_t group_i, const key_t &key);
 key_t &get_group_pivot(size_t group_i) const;

 void grouping_by_partial_key(const std::vector<key_t> &keys, size_t et,
                                      size_t pt, size_t fstep, size_t bstep,
                                      size_t min_size,
                                      std::vector<size_t> &pivot_indexes) const;
 void partial_key_len_by_step(
      const std::vector<key_t> &keys, const size_t start,
      const size_t step_start, const size_t step_end, size_t &common_p_len,
      size_t &max_p_len, std::unordered_map<size_t, size_t> &common_p_history,
      std::unordered_map<size_t, size_t> &max_p_history) const;
 double train_and_get_err(std::vector<double> &model_key, size_t start,
                                  size_t end, size_t p_len, size_t f_len) const;

  std::unique_ptr<std::pair<key_t, group_t *volatile>[]> groups;  // 8B
  uint32_t group_n = 0;                                           // 4B
  uint32_t root_model_n = 0;                                      // 4B
  std::array<key_t, max_root_model_n> model_pivots;
  std::array<model_meta_t, max_root_model_n> models;
};

}  // namespace sindex

#endif  // SINDEX_ROOT_H
