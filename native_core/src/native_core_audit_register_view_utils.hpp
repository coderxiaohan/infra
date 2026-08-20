#pragma once

#include "checked_index.hpp"

#include <stdexcept>

namespace inferlab_native_audit {

inline int checked_view_indent(const int indent, const int increment,
                               const char* label) {
  if (label == nullptr || label[0] == '\0') {
    throw std::invalid_argument(
        "native audit view indent label must be nonempty");
  }
  return inferlab::internal::checked_add_int(indent, increment, label);
}

}  // namespace inferlab_native_audit
