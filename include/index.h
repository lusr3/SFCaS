#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <vector>

#include "helper.h"
#include "sindex.h"
// #include "sindex_buffer.h"
#include "sindex_group.h"
#include "sindex_root.h"
#include "sindex_util.h"
#include "sindex_model.h"
#include "needle.h"

#if !defined(SINDEX_INTERFACE_H)
#define SINDEX_INTERFACE_H


template <size_t len>
class StrKey;

typedef StrKey<256> index_key_t;

typedef sindex::SIndex<index_key_t, uint64_t> sindex_t;

template <size_t len>
class StrKey {
  typedef std::array<double, len> model_key_t;

 public:
  //  constexpr 约束保证其定义的是在编译期可求得的值
  static constexpr size_t model_key_size() { return len; }

  static StrKey max() {
    static StrKey max_key;
    memset(max_key.buf, 255, len);
    return max_key;
  }
  static StrKey min() {
    static StrKey min_key;
    memset(min_key.buf, 0, len);
    return min_key;
  }

  StrKey() { memset(buf, 0, len); }
  // 函数作用？不允许传入 64 位的 key？
  StrKey(uint64_t key) { COUT_N_EXIT("str key no uint64"); }
  StrKey(const char *s) {
    memset(buf, 0, len);
    memcpy(buf, s, strlen(s));
  }
  StrKey(const std::string &s) {
    memset(buf, 0, len);
    memcpy(buf, s.data(), s.size());
  }
  StrKey(const StrKey &other) { memcpy(buf, other.buf, len); }
  StrKey &operator=(const StrKey &other) {
    memcpy(buf, &other.buf, len);
    return *this;
  }

  void set_key(const char *s) {
    memcpy(buf, s, strlen(s));
  }

  std::string to_string() {
    std::string str;
    str.resize(len);
    for(int i = 0; i < len; ++i) {
      str[i] = buf[i];
    }
    return str;
  }

  // 函数作用？
  model_key_t to_model_key() const {
    model_key_t model_key;
    for (size_t i = 0; i < len; i++) {
      model_key[i] = buf[i];
    }
    return model_key;
  }

  void get_model_key(size_t begin_f, size_t l, double *target) const {
    for (size_t i = 0; i < l; i++) {
      target[i] = buf[i + begin_f];
    }
  }

  bool less_than(const StrKey &other, size_t begin_i, size_t l) const {
    return memcmp(buf + begin_i, other.buf + begin_i, l) < 0;
  }

  friend bool operator<(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) < 0;
  }
  friend bool operator>(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) > 0;
  }
  friend bool operator>=(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) >= 0;
  }
  friend bool operator<=(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) <= 0;
  }
  friend bool operator==(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) == 0;
  }
  friend bool operator!=(const StrKey &l, const StrKey &r) {
    return memcmp(&l.buf, &r.buf, len) != 0;
  }

  friend std::ostream &operator<<(std::ostream &os, const StrKey &key) {
    // hex 是指定后续输出为 16 进制
    os << "key [" << std::hex;
    for (size_t i = 0; i < len; i++) {
      os << "0x" << key.buf[i] << " ";
    }
    os << "] (as byte)" << std::dec;
    return os;
  }

  uint8_t buf[len];
} PACKED;

// 装载 index 文件内容并得到大文件的文件指针
// 成功返回 index 数目，失败返回 -1
int init(struct needle_index_list *index_list);
void release_needle(struct needle_index_list *index_list);

// 从 index_list 中找到对应于 filename 的 needle_index
struct needle_index *find_index(struct needle_index_list *index_list, const char *filename, sindex_t *sindex_model);


/*  SIndex  */
sindex_t *get_sindex_model(std::vector<struct needle_index> &indexs);

#endif