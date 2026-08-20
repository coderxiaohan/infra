#pragma once

#include <cstddef>
#include <string>

namespace inferlab_deep_dive_trace {

inline bool trace_record_pointer_has_prefix(
    const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0U) == 0U;
}

inline bool parse_trace_record_pointer(
    const std::string& pointer, const std::string& prefix,
    const std::string& field, const std::size_t maximum_record_count,
    std::size_t& record_index) {
  record_index = 0U;
  if (!trace_record_pointer_has_prefix(pointer, prefix)) return false;
  const std::size_t index_begin = prefix.size();
  const std::size_t separator = pointer.find('/', index_begin);
  if (separator == std::string::npos || separator == index_begin ||
      pointer.substr(separator + 1U) != field) {
    return false;
  }
  for (std::size_t cursor = index_begin; cursor < separator; ++cursor) {
    const char digit_char = pointer[cursor];
    if (digit_char < '0' || digit_char > '9') return false;
    const std::size_t digit =
        static_cast<std::size_t>(digit_char - '0');
    if (digit > maximum_record_count ||
        record_index > (maximum_record_count - digit) / 10U) {
      return false;
    }
    record_index = record_index * 10U + digit;
  }
  return record_index < maximum_record_count;
}

}  // namespace inferlab_deep_dive_trace
