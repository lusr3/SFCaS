#include <cstdint>
#include <cstdlib>
#include <memory>

#include "helper.h"

#if !defined(SINDEX_UTIL_H)
#define SINDEX_UTIL_H

namespace sindex {

const size_t max_root_model_n = 4;
enum class Result;
struct IndexConfig;

typedef Result result_t;
typedef IndexConfig index_config_t;

enum class Result { ok, failed };

struct IndexConfig {
  double group_error_bound = 32;
  double group_error_tolerance = 4;

  // partial_len_bound 和 max_to_group_num 必须相适应
  // 不然分批次 group 在 pt 过大时会有死循环（因为全部都能 group 进去）
  // greedy grouping related
  size_t partial_len_bound = 4;
  size_t forward_step = 550;
  size_t backward_step = 50;
  size_t group_min_size = 400;
  // for limited memory
  // 小内存机器上参数越大时间越久
  // size_t max_to_group_num = 100000;
  // bool is_mem_limit = true;
};

const index_config_t config;

}  // namespace sindex

inline size_t common_prefix_length(size_t start_i, const uint8_t *key1,
                                   size_t k1_len, const uint8_t *key2,
                                   size_t k2_len) {
  for (size_t f_i = start_i; f_i < std::min(k1_len, k2_len); ++f_i) {
    if (key1[f_i] != key2[f_i]) return f_i - start_i;
  }
  return std::min(k1_len, k2_len) - start_i;
}

inline double dot_product(const double *a, const double *b, size_t len) {
  if (len < 4) {
    double res = 0;
    for (size_t feat_i = 0; feat_i < len; feat_i++) {
      res += a[feat_i] * b[feat_i];
    }
    return res;
  }

  // 得到一个 256bit 的 64bit 浮点数向量
  // 值为 [0.0, 0.0, 0.0, 0.0]
  __m256d sum_vec = _mm256_set_pd(0.0, 0.0, 0.0, 0.0);

  // 每次计算四个长度单位
  for (size_t ii = 0; ii < (len >> 2); ++ii) {
    __m256d x = _mm256_loadu_pd(a + 4 * ii);
    __m256d y = _mm256_loadu_pd(b + 4 * ii);
    // 不支持 fma 时使用下面这两条指令
    __m256d z = _mm256_mul_pd(x, y);
    sum_vec = _mm256_add_pd(sum_vec, z);
    // sum_vec = _mm256_fmadd_pd(x, y, sum_vec);
  }

  // the partial dot-product for the remaining elements
  double trailing = 0.0;
  for (size_t ii = (len & (~3)); ii < len; ++ii) trailing += a[ii] * b[ii];

  // _mm256_hadd_pd(a, b)
  // 将 a 和 b 之中相邻的两位加起来，按照 a b a b 的顺序放入新向量中
  // res = [a0 + a1, b0 + b1, a2 + a3, b2 + b3]
  __m256d temp = _mm256_hadd_pd(sum_vec, sum_vec);
  return ((double *)&temp)[0] + ((double *)&temp)[2] + trailing;
}

#endif  // SINDEX_UTIL_H
