#pragma once

#include "artifact_semantic_index.hpp"
#include "checked_index.hpp"
#include "deep_dive_trace_record_pointer.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_deep_dive_trace {

inline constexpr std::size_t kMaximumJoinedRecordsPerSource = 8192U;
inline constexpr std::size_t kMaximumProjectedScalarValues = 65536U;
inline constexpr std::size_t kMaximumProjectedStringBytes = 4096U;
inline constexpr char kProductTraceRecordPrefix[] =
    "/request_trace_join/records/";
inline constexpr char kNativeTraceRecordPrefix[] =
    "/native_trace_context/records/";
inline constexpr const char* kProductRecordPrefix =
    kProductTraceRecordPrefix;
inline constexpr const char* kNativeRecordPrefix =
    kNativeTraceRecordPrefix;

inline void append_unique(std::vector<std::string>& values,
                          const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

inline bool bounded_unsigned(const double value,
                             const std::size_t maximum,
                             std::size_t& result) {
  if (!std::isfinite(value) || value < 0.0 ||
      std::floor(value) != value ||
      value > static_cast<double>(maximum)) {
    result = 0U;
    return false;
  }
  result = static_cast<std::size_t>(value);
  return true;
}

inline bool required_scalar(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key,
    const inferlab_artifact::SemanticValueKind kind,
    const std::string& pointer,
    inferlab_artifact::SemanticScalarValue& value,
    std::vector<std::string>& issues) {
  const auto projection = inferlab_artifact::project_semantic_scalars(
      document, index, key, kind, true, pointer, 1U,
      kMaximumProjectedStringBytes);
  if (!projection.complete || projection.values.size() != 1U ||
      projection.values.front().json_pointer != pointer) {
    append_unique(issues, "missing_or_invalid_scalar:" + pointer);
    return false;
  }
  value = projection.values.front();
  return true;
}

inline bool required_string(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, const std::string& pointer,
    std::string& result, std::vector<std::string>& issues) {
  inferlab_artifact::SemanticScalarValue value;
  if (!required_scalar(document, index, key,
                       inferlab_artifact::SemanticValueKind::String,
                       pointer, value, issues)) {
    return false;
  }
  result = value.string_value;
  return true;
}

inline bool required_bool(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, const std::string& pointer,
    bool& result, std::vector<std::string>& issues) {
  inferlab_artifact::SemanticScalarValue value;
  if (!required_scalar(document, index, key,
                       inferlab_artifact::SemanticValueKind::Boolean,
                       pointer, value, issues)) {
    return false;
  }
  result = value.boolean_value;
  return true;
}

inline bool required_unsigned(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, const std::string& pointer,
    const std::size_t maximum, std::size_t& result,
    std::vector<std::string>& issues) {
  inferlab_artifact::SemanticScalarValue value;
  if (!required_scalar(document, index, key,
                       inferlab_artifact::SemanticValueKind::Number,
                       pointer, value, issues) ||
      !bounded_unsigned(value.number_value, maximum, result)) {
    append_unique(issues, "unsigned_scalar_out_of_range:" + pointer);
    return false;
  }
  return true;
}

template <typename Value, typename Decode>
std::map<std::size_t, Value> collect_record_field(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& prefix, const std::string& key,
    const inferlab_artifact::SemanticValueKind kind, Decode decode,
    std::vector<std::string>& issues) {
  std::size_t relevant_occurrence_count = 0U;
  for (const auto& occurrence : index.fields) {
    if (occurrence.key != key ||
        !trace_record_pointer_has_prefix(occurrence.json_pointer, prefix)) {
      continue;
    }
    std::size_t record_index = 0U;
    if (!parse_trace_record_pointer(
            occurrence.json_pointer, prefix, key,
            kMaximumJoinedRecordsPerSource, record_index)) {
      append_unique(issues, "invalid_record_pointer:" + key);
      continue;
    }
    relevant_occurrence_count = inferlab::internal::checked_add_size(
        relevant_occurrence_count, 1U,
        "trace scalar relevant field occurrence count");
  }

  const auto projection = inferlab_artifact::project_semantic_scalars(
      document, index, key, kind, false, "",
      kMaximumProjectedScalarValues, kMaximumProjectedStringBytes);
  std::map<std::size_t, Value> result;
  std::size_t decoded_relevant_count = 0U;
  for (const auto& scalar : projection.values) {
    std::size_t record_index = 0U;
    if (!parse_trace_record_pointer(
            scalar.json_pointer, prefix, key,
            kMaximumJoinedRecordsPerSource, record_index)) {
      continue;
    }
    Value decoded{};
    if (!decode(scalar, decoded)) {
      append_unique(issues, "invalid_record_value:" + key);
      continue;
    }
    if (!result.emplace(record_index, std::move(decoded)).second) {
      append_unique(issues, "duplicate_record_field:" + key);
      continue;
    }
    decoded_relevant_count = inferlab::internal::checked_add_size(
        decoded_relevant_count, 1U,
        "trace scalar decoded relevant field count");
  }
  if (decoded_relevant_count != relevant_occurrence_count) {
    append_unique(issues, "record_field_projection_incomplete:" + key);
  }
  return result;
}

inline std::map<std::size_t, std::size_t> collect_unsigned_field(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& prefix, const std::string& key,
    const std::size_t maximum, std::vector<std::string>& issues) {
  return collect_record_field<std::size_t>(
      document, index, prefix, key,
      inferlab_artifact::SemanticValueKind::Number,
      [maximum](const inferlab_artifact::SemanticScalarValue& scalar,
                std::size_t& value) {
        return bounded_unsigned(scalar.number_value, maximum, value);
      },
      issues);
}

inline std::map<std::size_t, std::string> collect_string_field(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& prefix, const std::string& key,
    std::vector<std::string>& issues) {
  return collect_record_field<std::string>(
      document, index, prefix, key,
      inferlab_artifact::SemanticValueKind::String,
      [](const inferlab_artifact::SemanticScalarValue& scalar,
         std::string& value) {
        value = scalar.string_value;
        return true;
      },
      issues);
}

inline std::map<std::size_t, bool> collect_bool_field(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& prefix, const std::string& key,
    std::vector<std::string>& issues) {
  return collect_record_field<bool>(
      document, index, prefix, key,
      inferlab_artifact::SemanticValueKind::Boolean,
      [](const inferlab_artifact::SemanticScalarValue& scalar,
         bool& value) {
        value = scalar.boolean_value;
        return true;
      },
      issues);
}

template <typename Value>
const Value* find_row_value(const std::map<std::size_t, Value>& values,
                            const std::size_t index,
                            const std::string& field,
                            std::vector<std::string>& issues) {
  const auto found = values.find(index);
  if (found == values.end()) {
    append_unique(issues, "missing_record_field:" + field);
    return nullptr;
  }
  return &found->second;
}

}  // namespace inferlab_deep_dive_trace
