#pragma once

#include "artifact_metadata_reader.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <limits>
#include <string>

namespace inferlab_release_pipeline {

inline int json_number_or_default(const std::string& text,
                                  const std::string& key,
                                  const int fallback) {
  std::size_t parsed = 0;
  if (!inferlab_artifact::extract_unsigned_key_fallback(text, key, parsed) ||
      parsed > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return fallback;
  }
  return static_cast<int>(parsed);
}

inline std::string json_string_or_default_json(
    const std::string& text,
    const std::string& key,
    const std::string& fallback) {
  const std::string value = inferlab_artifact::json_string_or_null(text, key);
  return value == "null" ? inferlab_plan::json_string(fallback) : value;
}

inline std::string json_string_or_empty(const std::string& text,
                                        const std::string& key) {
  return inferlab_artifact::extract_string_key_fallback(text, key);
}

}  // namespace inferlab_release_pipeline
