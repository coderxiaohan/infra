#pragma once

#include "artifact_metadata_reader.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>

namespace inferlab_artifact {

struct UnsignedFieldProjection {
  std::size_t value = 0;
  bool present = false;
  bool invalid = false;
};

struct StringFieldProjection {
  std::string value;
  bool present = false;
  bool invalid = false;
};

struct BoolFieldProjection {
  bool value = false;
  bool present = false;
  bool invalid = false;
};

inline UnsignedFieldProjection project_unsigned_field(
    const std::initializer_list<const std::string*> sources,
    const std::initializer_list<const char*> keys) {
  UnsignedFieldProjection projection;
  for (const std::string* source : sources) {
    if (source == nullptr) continue;
    for (const char* key : keys) {
      if (!contains_top_level_key_fallback(*source, key)) continue;
      projection.present = true;
      projection.invalid =
          !extract_unsigned_key_fallback(*source, key, projection.value);
      return projection;
    }
  }
  return projection;
}

inline UnsignedFieldProjection project_unsigned_field(
    const std::string& source,
    const std::initializer_list<const char*> keys) {
  return project_unsigned_field({&source}, keys);
}

inline StringFieldProjection project_string_field(
    const std::initializer_list<const std::string*> sources,
    const std::initializer_list<const char*> keys) {
  StringFieldProjection projection;
  for (const std::string* source : sources) {
    if (source == nullptr) continue;
    for (const char* key : keys) {
      if (!contains_top_level_key_fallback(*source, key)) continue;
      projection.present = true;
      projection.invalid =
          !extract_top_level_string_key_fallback(*source, key,
                                                 projection.value);
      return projection;
    }
  }
  return projection;
}

inline StringFieldProjection project_string_field(
    const std::string& source,
    const std::initializer_list<const char*> keys) {
  return project_string_field({&source}, keys);
}

inline BoolFieldProjection project_bool_field(
    const std::initializer_list<const std::string*> sources,
    const std::initializer_list<const char*> keys) {
  BoolFieldProjection projection;
  for (const std::string* source : sources) {
    if (source == nullptr) continue;
    for (const char* key : keys) {
      if (!contains_top_level_key_fallback(*source, key)) continue;
      projection.present = true;
      projection.invalid =
          !extract_bool_key_fallback(*source, key, projection.value);
      return projection;
    }
  }
  return projection;
}

inline BoolFieldProjection project_bool_field(
    const std::string& source,
    const std::initializer_list<const char*> keys) {
  return project_bool_field({&source}, keys);
}

}  // namespace inferlab_artifact
