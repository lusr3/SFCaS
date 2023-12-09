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

#include "helper.h"
#include "sindex_group.h"
#include "sindex_model.h"
#include "sindex_root.h"
#include "sindex_util.h"

#if !defined(SINDEX_H)
#define SINDEX_H

namespace sindex {

// seq 的作用？
template <class key_t, class val_t>
class SIndex {
  typedef Group<key_t, val_t> group_t;
  typedef Root<key_t, val_t> root_t;
  typedef void iterator_t;

 public:
  SIndex(const std::vector<key_t> &keys, const std::vector<val_t> &vals,
         size_t worker_num);
  ~SIndex();

  // worker_id 的作用
  // 多个线程读取、插入、删除、扫描
  bool get(const key_t &key, val_t &val, const uint32_t worker_id);
 private:

  root_t *volatile root = nullptr;
};

}  // namespace sindex

#endif  // SINDEX_H
