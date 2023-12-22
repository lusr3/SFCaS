#include "sindex_root.h"

namespace sindex {
template class Root<index_key_t, uint64_t>;

template <class key_t, class val_t>
Root<key_t, val_t>::~Root() {}

template <class key_t, class val_t>
void Root<key_t, val_t>::init(const std::vector<key_t> &keys,
                                   const std::vector<val_t> &vals,
                                   std::vector<struct needle_index> &indexs) {
  std::vector<size_t> pivot_indexes;
  // 贪心分组得到每个组的 pivot 
  grouping_director(keys, pivot_indexes);
  // grouping_by_partial_key(keys, config.group_error_bound,
  //                         config.partial_len_bound, config.forward_step,
  //                         config.backward_step, config.group_min_size,
  //                         pivot_indexes);

  group_n = pivot_indexes.size();
  COUT_THIS("The number of groups: " << group_n);
  size_t record_n = keys.size();
  // <group_pivot, group>
  groups = std::make_unique<std::pair<key_t, group_t *>[]>(group_n);

  int64_t max_pos_error = 0, max_neg_error = 0, max_error = 0;
  // 填充 groups 的内容
  // 同时会初始化每个 group
  for (size_t group_i = 0; group_i < group_n; group_i++) {
    size_t begin_i = pivot_indexes[group_i];
    size_t end_i =
        group_i + 1 == group_n ? record_n : pivot_indexes[group_i + 1];

    set_group_pivot(group_i, keys[begin_i]);
    group_t *group_ptr = new group_t();
    group_ptr->init(keys.begin() + begin_i, vals.begin() + begin_i,
                    end_i - begin_i, indexs.data() + begin_i, begin_i);
    int64_t pos_error = 0, neg_error = 0;
    group_ptr->get_model_error(pos_error, neg_error);
    max_pos_error = std::max(max_pos_error, pos_error);
    max_neg_error = std::min(max_neg_error, neg_error);
    max_error = std::max(max_error, pos_error - neg_error);
    set_group_ptr(group_i, group_ptr);
    assert(group_ptr == get_group_ptr(group_i));
  }

  COUT_THIS("Max pos error of groups: " << max_pos_error);
  COUT_THIS("Max neg error of groups: " << max_neg_error);
  COUT_THIS("Max error of groups: " << max_error);

  // 训练 root 层次的 model
  train_piecewise_model();
}

// 将 pivot 分组并训练
template <class key_t, class val_t>
inline void Root<key_t, val_t>::train_piecewise_model() {
  std::vector<size_t> indexes;
  std::vector<key_t> pivots(group_n);
  for (size_t g_i = 0; g_i < group_n; ++g_i) {
    pivots[g_i] = get_group_pivot(g_i);
  }
  // 将 pivots 分组
  grouping_director(pivots, indexes);
  // grouping_by_partial_key(pivots, config.group_error_bound,
  //                         config.partial_len_bound, config.forward_step,
  //                         config.backward_step, config.group_min_size, indexes);

  // 限制模型数量在 4 个及以下
  if (indexes.size() > max_root_model_n) {
    // 这里是从 indexes 中看每个 model 要多少
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

  COUT_THIS("The number of root model: " << model_n);

  assert(indexes.size() <= model_n);
  uint32_t p_len = 0, f_len = 0;
  // 得到分组后每个 model 的前缀信息并训练
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
    for (size_t k_i = 0; k_i < m_size; ++k_i) {
      // k_i 是相对于本 model 的首元素的偏移
      // 这里的 m_size 不是 model 数量，是 model 内 pivot 的数量
      get_group_ptr(k_i + b_i)->pivot.get_model_key(
          p_len, f_len, m_keys.data() + f_len * k_i);
      m_key_ptrs[k_i] = m_keys.data() + f_len * k_i;
      ps[k_i] = k_i + b_i;
    }

    models[m_i].weights.resize(f_len + 1, 0);
    models[m_i].p_len = p_len;
    models[m_i].f_len = f_len;
    model_prepare(m_key_ptrs, ps, models[m_i].weights.data(), f_len);

    // 计算误差
    // int64_t pos_error = 0, neg_error = 0;
    // for(size_t i = 0; i < m_size; ++i) {
    //   int64_t pos_pred = predict(m_key_ptrs[i], m_i);
    //   int64_t pos_actual = ps[i];
    //   int64_t error = pos_pred - pos_actual;

    //   if(error > pos_error) pos_error = error;
    //   if(error < neg_error) neg_error = error;
    // }

    // COUT_THIS("Model " << m_i << ":");
    // COUT_THIS("Max pos error: " << pos_error);
    // COUT_THIS("Max neg error: " << neg_error);
    // COUT_THIS("Max error: " << pos_error - neg_error);

    // model_errors[m_i].first = pos_error;
    // model_errors[m_i].second = neg_error;
  }
}

template <class key_t, class val_t>
inline void Root<key_t, val_t>::grouping_director(const std::vector<key_t> &keys,
                                                  std::vector<size_t> &indexes) {
  size_t key_start = 0, key_end = 0;
  if(!config.is_mem_limit) {
    std::vector<double> model_keys(keys.size() * sizeof(key_t));
    for (size_t k_i = 0; k_i < keys.size(); ++k_i) {
      keys[k_i].get_model_key(0, sizeof(key_t),
                              model_keys.data() + sizeof(key_t) * k_i);
    }
    grouping_by_partial_key(false, model_keys, key_start, keys.size(), 
                          keys, config.group_error_bound,
                          config.partial_len_bound, config.forward_step,
                          config.backward_step, config.group_min_size, indexes);
  }
  else {
    while(key_end < keys.size()) {
      key_end = (key_start + config.max_to_group_num < keys.size()) ? 
                  key_start + config.max_to_group_num
                : keys.size();
      size_t key_size = key_end - key_start;
      std::vector<double> model_keys(key_size * sizeof(key_t));
      for (size_t k_i = 0; k_i < key_size; ++k_i) {
        keys[key_start + k_i].get_model_key(0, sizeof(key_t),
                                model_keys.data() + sizeof(key_t) * k_i);
      }
      grouping_by_partial_key(key_end != keys.size(), model_keys, key_start, key_size, 
                          keys, config.group_error_bound,
                          config.partial_len_bound, config.forward_step,
                          config.backward_step, config.group_min_size, indexes);
    }
  }
}

// 最重要的是找到每个组的 pivot 下标
// [key_start, key_end)
template <class key_t, class val_t>
inline void Root<key_t, val_t>::grouping_by_partial_key(bool is_limit,
    const std::vector<double> model_keys, size_t &key_start, size_t key_size,
    const std::vector<key_t> &keys, size_t et, size_t pt, size_t fstep,
    size_t bstep, size_t min_size, std::vector<size_t> &pivot_indexes) const {
  // pivot_indexes.clear();
  size_t start_i = key_start, end_i = key_start, key_end = key_start + key_size;
  size_t common_p_len = 0, max_p_len = 0, f_len = 0;
  double group_error = 0;
  // double avg_group_size = 0, avg_f_len = 0;
  std::unordered_map<size_t, size_t> common_p_history;
  std::unordered_map<size_t, size_t> max_p_history;

  // prepare model keys for training
  // std::vector<double> model_keys(keys.size() * sizeof(key_t));
  // for (size_t k_i = 0; k_i < keys.size(); ++k_i) {
  //   keys[k_i].get_model_key(0, sizeof(key_t),
  //                           model_keys.data() + sizeof(key_t) * k_i);
  // }

  // 遍历当前的所有 keys
  while (end_i < key_end) {
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
      if (end_i > key_end) {
        // DEBUG_THIS("[Grouping] reach end. last group size= "
        //            << (keys.size() - start_i));
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
      if (end_i > key_end) {
        // DEBUG_THIS("[Grouping] reach end. last group size= "
        //            << (keys.size() - start_i));
        break;
      }
      // 达不到最小 group 的阈值需求则直接给够
      // 可能会有 bug 需要注意
      if (end_i - start_i < min_size) {
        end_i = start_i + min_size;
        break;
      }
      end_i -= bstep;
      partial_key_len_by_step(keys, start_i, start_i, end_i, common_p_len,
                              max_p_len, common_p_history, max_p_history);
      INVARIANT(common_p_len <= max_p_len);
      f_len = max_p_len - common_p_len + 1;
      // group_error =
      //     train_and_get_err(model_keys, start_i, end_i, common_p_len, f_len);
    }

    // 只保证满足这两个条件，所计算的前缀长并不保证，所以后面还需要再次计算
    assert(f_len <= pt || end_i - start_i == min_size);
    // 如果内存有限制，且到达当前可用 keys 的末尾
    // 则限定下一次的开头
    if(end_i > key_end && is_limit) key_start = start_i;
    else {
      pivot_indexes.push_back(start_i);
      start_i = end_i;
      key_start = start_i;
    }
  }
}

// 计算每一步的公共和最长前缀和
template <class key_t, class val_t>
inline void Root<key_t, val_t>::partial_key_len_by_step(
    const std::vector<key_t> &keys, const size_t start_i,
    const size_t step_start_i, const size_t step_end_i, size_t &common_p_len,
    size_t &max_p_len, std::unordered_map<size_t, size_t> &common_p_history,
    std::unordered_map<size_t, size_t> &max_p_history) const {
  assert(start_i < step_end_i);

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

// 找到每个 model 组内的前缀信息
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
  
  f_len = max_adjacent_prefix - p_len + 1;
}

template <class key_t, class val_t>
inline result_t Root<key_t, val_t>::get(const key_t &key, val_t &val) {
  group_t *group_ptr = locate_group(key);
  auto res = group_ptr->get(key, val);
  return res;
}

// 先指数查找再二分查找，定位到含有该 key 的 group
template <class key_t, class val_t>
inline typename Root<key_t, val_t>::group_t *
Root<key_t, val_t>::locate_group(const key_t &key) {
  int group_i = predict(key);
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
  
  // now group[begin].pivot <= key && group[end + 1].pivot > key
  // in loop body, the valid search range is actually [begin + 1, end] +1
  // (inclusive range), thus the +1 term in mid is a must
  // this algorithm produces index to the last element that is <= key
  while (end_group_i != begin_group_i) {
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
inline size_t Root<key_t, val_t>::predict(const double *model_key, uint32_t model_i) {
  uint32_t f_len = models[model_i].f_len;
  return model_predict(models[model_i].weights.data(), model_key, f_len);
}

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

