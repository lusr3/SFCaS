#include <vector>
#include <unistd.h>

#include "helper.h"
#include "needle.h"
#include "sindex_group.h"
#include "sindex_model.h"
#include "sindex_root.h"
#include "sindex_util.h"

#if !defined(SINDEX_H)
#define SINDEX_H

namespace sindex {

template <class key_t, class val_t>
class SIndex {
  typedef Group<key_t, val_t> group_t;
  typedef Root<key_t, val_t> root_t;

 public:
  SIndex(const std::vector<key_t> &keys, const std::vector<val_t> &vals,
         std::vector<struct needle_index> &indexs);
  ~SIndex();

  bool get(const key_t &key, val_t &val);
  
private:
  root_t *root = nullptr;
};

}  // namespace sindex

#endif  // SINDEX_H
