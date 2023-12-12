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
#include <unordered_map>
#include <vector>
#include <unistd.h>
#include <numeric>

#include "sindex_root.h"
#include "index.h"

namespace sindex {
template class Root<index_key_t, uint64_t>;

template <class key_t, class val_t>
Root<key_t, val_t>::~Root() {}

template <class key_t, class val_t>
void Root<key_t, val_t>::init(const std::vector<key_t> &keys,
                                   const std::vector<val_t> &vals) {

  std::vector<size_t> pivot_indexes;
  // 贪心分组得到每个组的 pivot 
  grouping_by_partial_key(keys, config.group_error_bound,
                          config.partial_len_bound, config.forward_step,
                          config.backward_step, config.group_min_size,
                          pivot_indexes);

  group_n = pivot_indexes.size();
  size_t record_n = keys.size();
  // <group_pivot, group>
  groups = std::make_unique<std::pair<key_t, group_t *volatile>[]>(group_n);

  // double max_group_error = 0, avg_group_error = 0;
  // double avg_prefix_len = 0, avg_feature_len = 0;
  // size_t feature_begin_i = std::numeric_limits<uint16_t>::max(),
  //        max_feature_len = 0;

  // 填充 groups 的内容
  // 同时会初始化每个 group
  for (size_t group_i = 0; group_i < group_n; group_i++) {
    size_t begin_i = pivot_indexes[group_i];
    size_t end_i =
        group_i + 1 == group_n ? record_n : pivot_indexes[group_i + 1];

    set_group_pivot(group_i, keys[begin_i]);
    group_t *group_ptr = new group_t();
    group_ptr->init(keys.begin() + begin_i, vals.begin() + begin_i,
                    end_i - begin_i);
    set_group_ptr(group_i, group_ptr);
    assert(group_ptr == get_group_ptr(group_i));

    // avg_prefix_len += get_group_ptr(group_i)->prefix_len;
    // avg_feature_len += get_group_ptr(group_i)->feature_len;
    // 最小的起始位置和最大的特征长度
    // feature_begin_i =
    //     std::min(feature_begin_i, (size_t)groups[group_i].second->prefix_len);
    // max_feature_len =
    //     std::max(max_feature_len, (size_t)groups[group_i].second->feature_len);
    // double err = get_group_ptr(group_i)->get_mean_error();
    // max_group_error = std::max(max_group_error, err);
    // avg_group_error += err;
  }

  // then decide # of 2nd stage model of root RMI
  adjust_root_model();

  // DEBUG_THIS("------ Final SIndex Paramater: group_n="
  //            << group_n << ", avg_group_size=" << keys.size() / group_n
  //            << ", feature_begin_i=" << feature_begin_i
  //            << ", max_feature_len=" << max_feature_len);
  // DEBUG_THIS("------ Final SIndex Errors: max_error="
  //            << max_group_error
  //            << ", avg_group_error=" << avg_group_error / group_n
  //            << ", avg_prefix_len=" << avg_prefix_len / group_n
  //            << ", avg_feature_len=" << avg_feature_len / group_n);
  // DEBUG_THIS("------ Final SIndex Memory: sizeof(root)="
  //            << sizeof(*this) << ", sizeof(group_t)=" << sizeof(group_t)
  //            << ", sizeof(group_t::record_t)="
  //            << sizeof(typename group_t::record_t)
  //            << ", sizeof(group_t::wrapped_val_t)="
  //            << sizeof(typename group_t::wrapped_val_t));
}

template <class key_t, class val_t>
inline result_t Root<key_t, val_t>::get(const key_t &key, val_t &val) {
  return locate_group(key)->get(key, val);
}

// 训练 root 处的模型用于查找对应的 group
template <class key_t, class val_t>
void Root<key_t, val_t>::adjust_root_model() {
  train_piecewise_model();

  std::vector<double> errors(group_n);
  for (size_t group_i = 0; group_i < group_n; group_i++) {
    // +1 是为什么？？？
    errors[group_i] = std::abs(
        (double)group_i - (double)predict(get_group_ptr(group_i)->pivot) + 1);
  }
  double mean_error =
      std::accumulate(errors.begin(), errors.end(), 0.0) / errors.size();

  DEBUG_THIS("------ Final SIndex Root Piecewise Model: model_n="
             << this->root_model_n << ", error=" << mean_error);
}

// 逐个比较 pivot 然后找到对应的模型去 predict
template <class key_t, class val_t>
inline size_t Root<key_t, val_t>::predict(const key_t &key) {
  uint32_t m_i = 0;
  while (m_i < root_model_n - 1 && key >= model_pivots[m_i + 1]) {
    m_i++;
  }
  uint32_t p_len = models[m_i].p_len, f_len = models[m_i].f_len;
  double model_key[f_len];
  key.get_model_key(p_len, f_len, model_key);
  return model_predict(models[m_i].weights.data(), model_key, f_len);
}

template <class key_t, class val_t>
inline void Root<key_t, val_t>::train_piecewise_model() {
  std::vector<size_t> indexes;
  std::vector<key_t> pivots(group_n);
  for (size_t g_i = 0; g_i < group_n; ++g_i) {
    pivots[g_i] = get_group_pivot(g_i);
  }
  // 将 pivots 分组
  grouping_by_partial_key(pivots, config.group_error_bound,
                          config.partial_len_bound, config.forward_step,
                          config.backward_step, config.group_min_size, indexes);

  // 限制模型数量在 4 个及以下
  if (indexes.size() > max_root_model_n) {
    // 注意这里是从 indexes 中看每个 model 要多少
    size_t index_per_model = indexes.size() / max_root_model_n;
    std::vector<size_t> new_indexes;
    size_t trailing = indexes.size() - index_per_model * max_root_model_n;
    size_t start_i = 0;
    // 将 trailing 的部分依次添加到前面的 model 中去
    for (size_t i = 0; i < max_root_model_n; ++i) {
      new_indexes.push_back(indexes[start_i]);
      start_i += index_per_model;
      if (trailing > 0) {
        trailing--;
        start_i++;
      }
    }
    indexes.swap(new_indexes);
  }

  uint32_t model_n = indexes.size();
  INVARIANT(model_n <= max_root_model_n);
  this->root_model_n = model_n;
  DEBUG_THIS("----- Root model n after grouping: " << model_n);

  assert(indexes.size() <= model_n);
  uint32_t p_len = 0, f_len = 0;
  // 得到分组后每个组的前缀信息并训练
  for (size_t m_i = 0; m_i < indexes.size(); ++m_i) {
    size_t b_i = indexes[m_i];
    size_t e_i = (m_i == indexes.size() - 1) ? group_n : indexes[m_i + 1];
    model_pivots[m_i] = get_group_ptr(b_i)->pivot;
    partial_key_len_of_pivots(b_i, e_i, p_len, f_len);
    size_t m_size = e_i - b_i;
    DEBUG_THIS("------ SIndex Root Model(" << m_i << "): size=" << m_size
                                           << ", p_len=" << p_len
                                           << ", f_len=" << f_len);
    std::vector<double> m_keys(m_size * f_len);
    std::vector<double *> m_key_ptrs(m_size);
    std::vector<size_t> ps(m_size);
    // 至少是 400-550 个元素，所以 f_len 大一点不会访问非法内存
    // 因为这个 +2？？？
    for (size_t k_i = 0; k_i < m_size; ++k_i) {
      // k_i 是相对于本 model 的首元素的偏移
      // 这里的 m_size 不是 model 数量，是 mode 内 pivot 的数量
      get_group_ptr(k_i + b_i)->pivot.get_model_key(
          p_len, f_len, m_keys.data() + f_len * k_i);
      m_key_ptrs[k_i] = m_keys.data() + f_len * k_i;
      ps[k_i] = k_i + b_i;
    }

    for (size_t i = 0; i < key_t::model_key_size() + 1; ++i) {
      models[m_i].weights[i] = 0;
    }
    models[m_i].p_len = p_len;
    models[m_i].f_len = f_len;
    model_prepare(m_key_ptrs, ps, models[m_i].weights.data(), f_len);
  }
}

// 找到 root 里的 pivot 组内的前缀信息
// 这里 +2 是偏置？
template <class key_t, class val_t>
inline void Root<key_t, val_t>::partial_key_len_of_pivots(
    const size_t start_i, const size_t end_i, uint32_t &p_len,
    uint32_t &f_len) {
  assert(start_i < end_i);
  // pivot 组内只有一个元素
  if (end_i == start_i + 1) {
    p_len = 0;
    f_len = 1;
    return;
  }
  p_len = common_prefix_length(
      0, (uint8_t *)&(get_group_ptr(start_i)->pivot), sizeof(key_t),
      (uint8_t *)&(get_group_ptr(start_i + 1)->pivot), sizeof(key_t));
  size_t max_adjacent_prefix = p_len;

  for (size_t k_i = start_i + 2; k_i < end_i; ++k_i) {
    p_len = common_prefix_length(0, (uint8_t *)&(get_group_ptr(k_i - 1)->pivot),
                                 p_len, (uint8_t *)&(get_group_ptr(k_i)->pivot),
                                 sizeof(key_t));
    size_t adjacent_prefix = common_prefix_length(
        p_len, (uint8_t *)&(get_group_ptr(k_i - 1)->pivot), sizeof(key_t),
        (uint8_t *)&(get_group_ptr(k_i)->pivot), sizeof(key_t));
    assert(adjacent_prefix <= sizeof(key_t) - p_len);
    if (adjacent_prefix < sizeof(key_t) - p_len) {
      max_adjacent_prefix =
          std::max(max_adjacent_prefix, p_len + adjacent_prefix);
    }
  }
  // 为什么 +2？
  // 这里是偏置？
  f_len = max_adjacent_prefix - p_len + 2;
}

/*
 * Root::locate_group
 */
template <class key_t, class val_t>
inline typename Root<key_t, val_t>::group_t *
Root<key_t, val_t>::locate_group(const key_t &key) {
  int group_i;  // unused
  group_t *head = locate_group_pt1(key, group_i);
  return head;
  // return locate_group_pt2(key, head);
}

// 看不懂？？？和 group 中的 next 即分裂合并模型相关？
template <class key_t, class val_t>
inline typename Root<key_t, val_t>::group_t *
Root<key_t, val_t>::locate_group_pt1(const key_t &key, int &group_i) {
  group_i = predict(key);
  group_i = group_i > (int)group_n - 1 ? group_n - 1 : group_i;
  group_i = group_i < 0 ? 0 : group_i;

  // exponential search
  int begin_group_i, end_group_i;
  // key 在该预测的 group pivot 的右边（可能在该 group，也可能在更右边）
  if (get_group_pivot(group_i) <= key) {
    size_t step = 1;
    begin_group_i = group_i;
    end_group_i = begin_group_i + step;
    while (end_group_i < (int)group_n && get_group_pivot(end_group_i) <= key) {
      step = step * 2;
      begin_group_i = end_group_i;
      end_group_i = begin_group_i + step;
    }  // after this while loop, end_group_i might be >= group_n
    if (end_group_i > (int)group_n - 1) {
      end_group_i = group_n - 1;
    }
  } else {
    size_t step = 1;
    end_group_i = group_i;
    begin_group_i = end_group_i - step;
    while (begin_group_i >= 0 && get_group_pivot(begin_group_i) > key) {
      step = step * 2;
      end_group_i = begin_group_i;
      begin_group_i = end_group_i - step;
    }  // after this while loop, begin_group_i might be < 0
    if (begin_group_i < 0) {
      begin_group_i = -1;
    }
  }
  // ？？？？？？？？？？
  // now group[begin].pivot <= key && group[end + 1].pivot > key
  // in loop body, the valid search range is actually [begin + 1, end] +1？？？
  // (inclusive range), thus the +1 term in mid is a must
  // this algorithm produces index to the last element that is <= key
  while (end_group_i != begin_group_i) {
    // +2？？？
    // the "+2" term actually should be a "+1" after "/2", this is due to the
    // rounding in c++ when the first operant of "/" operator is negative
    int mid = (end_group_i + begin_group_i + 2) / 2;
    if (get_group_pivot(mid) <= key) {
      begin_group_i = mid;
    } else {
      end_group_i = mid - 1;
    }
  }
  // the result falls in [-1, group_n - 1]
  // now we ensure the pointer is not null
  group_i = end_group_i < 0 ? 0 : end_group_i;
  group_t *group = get_group_ptr(group_i);
  while (group_i > 0 && group == nullptr) {
    group_i--;
    group = get_group_ptr(group_i);
  }
  // however, we treat the pivot key of the 1st group as -inf, thus we return
  // 0 when the search result is -1
  assert(get_group_ptr(0) != nullptr);

  return group;
}

// template <class key_t, class val_t>
// inline typename Root<key_t, val_t>::group_t *
// Root<key_t, val_t>::locate_group_pt2(const key_t &key, group_t *begin) {
//   group_t *group = begin;
//   group_t *next = group->next;
//   while (next != nullptr && next->pivot <= key) {
//     group = next;
//     next = group->next;
//   }
//   return group;
// }

template <class key_t, class val_t>
inline void Root<key_t, val_t>::set_group_ptr(size_t group_i,
                                                   group_t *g_ptr) {
  groups[group_i].second = g_ptr;
}

template <class key_t, class val_t>
inline typename Root<key_t, val_t>::group_t *
Root<key_t, val_t>::get_group_ptr(size_t group_i) const {
  return groups[group_i].second;
}

template <class key_t, class val_t>
inline void Root<key_t, val_t>::set_group_pivot(size_t group_i,
                                                     const key_t &key) {
  groups[group_i].first = key;
}

template <class key_t, class val_t>
inline key_t &Root<key_t, val_t>::get_group_pivot(size_t group_i) const {
  return groups[group_i].first;
}

// 最重要的是找到每个组的 pivot 下标
template <class key_t, class val_t>
inline void Root<key_t, val_t>::grouping_by_partial_key(
    const std::vector<key_t> &keys, size_t et, size_t pt, size_t fstep,
    size_t bstep, size_t min_size, std::vector<size_t> &pivot_indexes) const {
  pivot_indexes.clear();
  size_t start_i = 0, end_i = 0;
  size_t common_p_len = 0, max_p_len = 0, f_len = 0;
  double group_error = 0;
  double avg_group_size = 0, avg_f_len = 0;
  std::unordered_map<size_t, size_t> common_p_history;
  std::unordered_map<size_t, size_t> max_p_history;

  // prepare model keys for training
  std::vector<double> model_keys(keys.size() * sizeof(key_t));
  for (size_t k_i = 0; k_i < keys.size(); ++k_i) {
    keys[k_i].get_model_key(0, sizeof(key_t),
                            model_keys.data() + sizeof(key_t) * k_i);
  }

  // 遍历所有 keys
  while (end_i < keys.size()) {
    common_p_len = 0;
    max_p_len = 0;
    f_len = 0;
    group_error = 0;
    common_p_history.clear();
    max_p_history.clear();

    // group_error 没有作用
    while (f_len < pt && group_error < et) {
      size_t pre_end_i = end_i;
      end_i += fstep;
      // 到达末尾的部分并入最后一个 group 中（不用管阈值）
      if (end_i >= keys.size()) {
        DEBUG_THIS("[Grouping] reach end. last group size= "
                   << (keys.size() - start_i));
        break;
      }
      // 得到公共前缀和最长公共前缀
      partial_key_len_by_step(keys, start_i, pre_end_i, end_i, common_p_len,
                              max_p_len, common_p_history, max_p_history);
      INVARIANT(common_p_len <= max_p_len);
      f_len = max_p_len - common_p_len + 1;
      if (f_len >= pt) break;
      // group_error =
      //     train_and_get_err(model_keys, start_i, end_i, common_p_len, f_len);
    }

    // group_error 没有作用
    // 如果是到达尾部 break 不会进入这里
    while (f_len > pt || group_error > et) {
      if (end_i >= keys.size()) {
        DEBUG_THIS("[Grouping] reach end. last group size= "
                   << (keys.size() - start_i));
        break;
      }
      // 达不到最小 group 的阈值需求则直接给够 break
      if (end_i - start_i < min_size) {
        end_i = start_i + min_size;
        break;
      }
      end_i -= bstep;
      // 从 start_i 开始计算，这里就可以用到 history(好像又用不到？？？)
      // 可不可以直接从最后一个 batch 开始计算？
      partial_key_len_by_step(keys, start_i, start_i, end_i, common_p_len,
                              max_p_len, common_p_history, max_p_history);
      INVARIANT(common_p_len <= max_p_len);
      f_len = max_p_len - common_p_len + 1;
      // group_error =
      //     train_and_get_err(model_keys, start_i, end_i, common_p_len, f_len);
    }

    // 只保证满足这两个条件，所计算的前缀长并不保证，所以后面还需要再次计算
    assert(f_len <= pt || end_i - start_i == min_size);
    pivot_indexes.push_back(start_i);
    avg_group_size += (end_i - start_i);
    avg_f_len += f_len;
    start_i = end_i;

    if (pivot_indexes.size() % 1000 == 0) {
      DEBUG_THIS("[Grouping] current size="
                 << pivot_indexes.size()
                 << ", avg_group_size=" << avg_group_size / pivot_indexes.size()
                 << ", avg_f_len=" << avg_f_len / pivot_indexes.size()
                 << ", current_group_error=" << group_error);
    }
  }

  DEBUG_THIS("[Grouping] group number="
             << pivot_indexes.size()
             << ", avg_group_size=" << avg_group_size / pivot_indexes.size()
             << ", avg_f_len=" << avg_f_len / pivot_indexes.size());
}

// 计算每一步的公共和最长前缀和
template <class key_t, class val_t>
inline void Root<key_t, val_t>::partial_key_len_by_step(
    const std::vector<key_t> &keys, const size_t start_i,
    const size_t step_start_i, const size_t step_end_i, size_t &common_p_len,
    size_t &max_p_len, std::unordered_map<size_t, size_t> &common_p_history,
    std::unordered_map<size_t, size_t> &max_p_history) const {
  assert(start_i < step_end_i);

  // fstep 是一致的，所以当 end 相同时，共同和最长前缀可以利用历史计算过的
  // 最后一个？？？
  if (common_p_history.count(step_end_i) > 0) {
    INVARIANT(max_p_history.count(step_end_i) > 0);
    common_p_len = common_p_history[step_end_i];
    max_p_len = max_p_history[step_end_i];
    return;
  }

  size_t offset = 0;  // we set offset to 0 intentionally! if it is not the
                      // first batch of calculate partial key length, we need to
                      // take the last element of last batch into account.

  // 如果第一轮的 fstep 就超界了需要清空
  if (step_start_i == start_i) {
    common_p_history.clear();
    max_p_history.clear();
    common_p_len =
        common_prefix_length(0, (uint8_t *)&keys[step_start_i], sizeof(key_t),
                             (uint8_t *)&keys[step_start_i + 1], sizeof(key_t));
    max_p_len = common_p_len;
    common_p_history[step_start_i + 1] = common_p_len;
    max_p_history[step_start_i + 1] = max_p_len;
    offset = 2;
  }

  for (size_t k_i = step_start_i + offset; k_i < step_end_i; ++k_i) {
    common_p_len =
        common_prefix_length(0, (uint8_t *)&keys[k_i - 1], common_p_len,
                             (uint8_t *)&keys[k_i], sizeof(key_t));
    size_t adjacent_prefix = common_prefix_length(
        common_p_len, (uint8_t *)&keys[k_i - 1], sizeof(key_t),
        (uint8_t *)&keys[k_i], sizeof(key_t));
    assert(adjacent_prefix <= sizeof(key_t) - common_p_len);
    if (adjacent_prefix < sizeof(key_t) - common_p_len) {
      max_p_len = std::max(max_p_len, common_p_len + adjacent_prefix);
    }
    common_p_history[k_i] = common_p_len;
    max_p_history[k_i] = max_p_len;
  }
}

template <class key_t, class val_t>
inline double Root<key_t, val_t>::train_and_get_err(
    std::vector<double> &model_keys, size_t start_i, size_t end_i, size_t p_len,
    size_t f_len) const {
  size_t key_n = end_i - start_i;
  assert(key_n > 2);
  std::vector<double *> model_key_ptrs(key_n);
  std::vector<size_t> positions(key_n);
  for (size_t k_i = 0; k_i < key_n; ++k_i) {
    model_key_ptrs[k_i] =
        model_keys.data() + (start_i + k_i) * sizeof(key_t) + p_len;
    positions[k_i] = k_i;
  }

  // f_len 再加 1 就是 bias 了
  std::vector<double> weights(f_len + 1, 0);
  model_prepare(model_key_ptrs, positions, weights.data(), f_len);

  double errors = 0;
  for (size_t k_i = 0; k_i < key_n; ++k_i) {
    size_t predict_i =
        model_predict(weights.data(), model_key_ptrs[k_i], f_len);
    errors += predict_i >= k_i ? (predict_i - k_i) : (k_i - predict_i);
  }
  return errors / key_n;
}

}  // namespace sindex

