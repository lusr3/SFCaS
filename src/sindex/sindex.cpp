#include "sindex.h"

namespace sindex {
template class SIndex<index_key_t, uint64_t>;

template <class key_t, class val_t>
SIndex<key_t, val_t>::SIndex(const std::vector<key_t> &keys, const std::vector<val_t> &vals,
         std::vector<struct needle_index> &indexs)
    {
  // sanity checks
  INVARIANT(config.group_error_bound > 0);
  INVARIANT(config.group_error_tolerance > 0);

  // 确保排序
  for (size_t key_i = 1; key_i < keys.size(); key_i++) {
    assert(keys[key_i] >= keys[key_i - 1]);
  }

  // malloc memory for root & init root
  root = new root_t();
  root->init(keys, vals, indexs);
}

template <class key_t, class val_t>
SIndex<key_t, val_t>::~SIndex() {
  delete root;
}

template <class key_t, class val_t>
inline bool SIndex<key_t, val_t>::get(const key_t &key, val_t &val) {
  return root->get(key, val) == result_t::ok;
}

}  // namespace sindex

