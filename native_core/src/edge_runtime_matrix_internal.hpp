#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace inferlab::edge_runtime_matrix::matrix_detail {

inline bool append_unique(std::vector<std::string>* values,
                          const std::string& value) {
  if (values == nullptr || value.empty() ||
      std::find(values->begin(), values->end(), value) != values->end()) {
    return false;
  }
  values->push_back(value);
  return true;
}

}  // namespace inferlab::edge_runtime_matrix::matrix_detail
