#pragma once

#include "checked_index.hpp"

#include <cstddef>
#include <stdexcept>

namespace inferlab_native_audit {

inline void checked_increment_count(std::size_t& value, const char* label) {
  if (label == nullptr || label[0] == '\0') {
    throw std::invalid_argument(
        "native audit checked counter label must be nonempty");
  }
  value = inferlab::internal::checked_add_size(value, 1U, label);
}

}  // namespace inferlab_native_audit
