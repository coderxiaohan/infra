#include "artifact_semantic_index.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_artifact {
namespace {

std::string escape_json_pointer_token(const std::string& value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    if (ch == '~') {
      escaped += "~0";
    } else if (ch == '/') {
      escaped += "~1";
    } else {
      escaped.push_back(ch);
    }
  }
  return escaped;
}

SemanticValueKind value_kind_at(const std::string& document,
                                const std::size_t start) {
  if (start >= document.size()) {
    throw std::logic_error("semantic index value start is out of range");
  }
  switch (document[start]) {
    case '"':
      return SemanticValueKind::String;
    case '{':
      return SemanticValueKind::Object;
    case '[':
      return SemanticValueKind::Array;
    case 't':
    case 'f':
      return SemanticValueKind::Boolean;
    case 'n':
      return SemanticValueKind::Null;
    default:
      return SemanticValueKind::Number;
  }
}

bool value_is_nonempty(const std::string& document,
                       const std::size_t start,
                       const std::size_t end,
                       const SemanticValueKind kind) {
  switch (kind) {
    case SemanticValueKind::String: {
      std::size_t close = std::string::npos;
      std::string decoded;
      return parse_json_string_literal(document, start, close, decoded) &&
             close + 1U == end && !decoded.empty();
    }
    case SemanticValueKind::Object:
      return skip_json_space(document, start + 1U) < end - 1U;
    case SemanticValueKind::Array:
      return skip_json_space(document, start + 1U) < end - 1U;
    case SemanticValueKind::Null:
      return false;
    case SemanticValueKind::Boolean:
    case SemanticValueKind::Number:
      return true;
    case SemanticValueKind::AnyNonNull:
      break;
  }
  throw std::logic_error("AnyNonNull is not an observed JSON value kind");
}

void update_maximum_depth(ArtifactSemanticIndex& index,
                          const std::size_t depth) {
  index.maximum_depth = std::max(index.maximum_depth, depth);
}

void collect_value(const std::string& document,
                   const std::size_t start,
                   const std::size_t end,
                   const std::string& pointer,
                   const std::size_t depth,
                   const std::size_t maximum_field_occurrences,
                   const std::size_t maximum_depth,
                   ArtifactSemanticIndex& index);

void collect_object(const std::string& document,
                    const std::size_t start,
                    const std::size_t end,
                    const std::string& pointer,
                    const std::size_t depth,
                    const std::size_t maximum_field_occurrences,
                    const std::size_t maximum_depth,
                    ArtifactSemanticIndex& index) {
  index.object_count = inferlab::internal::checked_add_size(
      index.object_count, 1U, "semantic index object count");
  update_maximum_depth(index, depth);
  std::size_t pos = skip_json_space(document, start + 1U);
  if (pos >= end) {
    throw std::logic_error("semantic index object traversal exceeded value");
  }
  if (document[pos] == '}') return;

  while (pos < end) {
    std::size_t key_close = std::string::npos;
    std::string key;
    if (!parse_json_string_literal(document, pos, key_close, key)) {
      throw std::logic_error("semantic index failed to decode object key");
    }
    pos = skip_json_space(document, key_close + 1U);
    if (pos >= end || document[pos] != ':') {
      throw std::logic_error("semantic index object key has no value");
    }
    const std::size_t value_start = skip_json_space(document, pos + 1U);
    const std::size_t value_end = find_json_value_end(document, value_start);
    if (value_end == std::string::npos || value_end > end) {
      throw std::logic_error("semantic index failed to bound object value");
    }
    if (index.fields.size() >= maximum_field_occurrences) {
      throw std::length_error(
          "semantic index field occurrence limit exceeded");
    }
    const SemanticValueKind kind = value_kind_at(document, value_start);
    const std::string child_pointer =
        pointer + "/" + escape_json_pointer_token(key);
    index.fields.push_back({key, child_pointer, kind,
                            value_is_nonempty(document, value_start,
                                              value_end, kind),
                            value_start, value_end});
    collect_value(document, value_start, value_end, child_pointer,
                  depth + 1U, maximum_field_occurrences, maximum_depth,
                  index);

    pos = skip_json_space(document, value_end);
    if (pos >= end) {
      throw std::logic_error("semantic index object is not terminated");
    }
    if (document[pos] == '}') return;
    if (document[pos] != ',') {
      throw std::logic_error("semantic index object separator is invalid");
    }
    pos = skip_json_space(document, pos + 1U);
  }
  throw std::logic_error("semantic index object traversal did not terminate");
}

void collect_array(const std::string& document,
                   const std::size_t start,
                   const std::size_t end,
                   const std::string& pointer,
                   const std::size_t depth,
                   const std::size_t maximum_field_occurrences,
                   const std::size_t maximum_depth,
                   ArtifactSemanticIndex& index) {
  index.array_count = inferlab::internal::checked_add_size(
      index.array_count, 1U, "semantic index array count");
  update_maximum_depth(index, depth);
  std::size_t pos = skip_json_space(document, start + 1U);
  if (pos >= end) {
    throw std::logic_error("semantic index array traversal exceeded value");
  }
  if (document[pos] == ']') return;

  std::size_t element_index = 0U;
  while (pos < end) {
    const std::size_t value_start = pos;
    const std::size_t value_end = find_json_value_end(document, value_start);
    if (value_end == std::string::npos || value_end > end) {
      throw std::logic_error("semantic index failed to bound array value");
    }
    const std::string child_pointer =
        pointer + "/" + std::to_string(element_index);
    collect_value(document, value_start, value_end, child_pointer,
                  depth + 1U, maximum_field_occurrences, maximum_depth,
                  index);
    element_index = inferlab::internal::checked_add_size(
        element_index, 1U, "semantic index array element count");

    pos = skip_json_space(document, value_end);
    if (pos >= end) {
      throw std::logic_error("semantic index array is not terminated");
    }
    if (document[pos] == ']') return;
    if (document[pos] != ',') {
      throw std::logic_error("semantic index array separator is invalid");
    }
    pos = skip_json_space(document, pos + 1U);
  }
  throw std::logic_error("semantic index array traversal did not terminate");
}

void collect_value(const std::string& document,
                   const std::size_t start,
                   const std::size_t end,
                   const std::string& pointer,
                   const std::size_t depth,
                   const std::size_t maximum_field_occurrences,
                   const std::size_t maximum_depth,
                   ArtifactSemanticIndex& index) {
  if (depth > maximum_depth) {
    throw std::length_error("semantic index maximum depth exceeded");
  }
  update_maximum_depth(index, depth);
  const SemanticValueKind kind = value_kind_at(document, start);
  if (kind == SemanticValueKind::Object) {
    collect_object(document, start, end, pointer, depth,
                   maximum_field_occurrences, maximum_depth, index);
    return;
  }
  if (kind == SemanticValueKind::Array) {
    collect_array(document, start, end, pointer, depth,
                  maximum_field_occurrences, maximum_depth, index);
    return;
  }
  index.scalar_count = inferlab::internal::checked_add_size(
      index.scalar_count, 1U, "semantic index scalar count");
}

bool kind_matches(const SemanticValueKind observed,
                  const SemanticValueKind expected) {
  return expected == SemanticValueKind::AnyNonNull
             ? observed != SemanticValueKind::Null
             : observed == expected;
}

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

bool pointer_matches(const SemanticFieldOccurrence& occurrence,
                     const std::string& exact_json_pointer) {
  return exact_json_pointer.empty() ||
         occurrence.json_pointer == exact_json_pointer;
}

bool scalar_kind(const SemanticValueKind kind) {
  return kind == SemanticValueKind::String ||
         kind == SemanticValueKind::Number ||
         kind == SemanticValueKind::Boolean;
}

void append_issue(std::vector<std::string>& issues,
                  const std::string& issue) {
  if (std::find(issues.begin(), issues.end(), issue) == issues.end()) {
    issues.push_back(issue);
  }
}

bool decode_scalar(const std::string& document,
                   const SemanticFieldOccurrence& occurrence,
                   const std::size_t maximum_string_bytes,
                   SemanticScalarValue& value) {
  if (occurrence.value_start >= occurrence.value_end ||
      occurrence.value_end > document.size()) {
    return false;
  }
  value.json_pointer = occurrence.json_pointer;
  value.kind = occurrence.value_kind;
  if (occurrence.value_kind == SemanticValueKind::String) {
    const std::size_t encoded_size =
        occurrence.value_end - occurrence.value_start;
    if (encoded_size < 2U || encoded_size - 2U > maximum_string_bytes) {
      return false;
    }
    std::size_t close = std::string::npos;
    if (!parse_json_string_literal(document, occurrence.value_start, close,
                                   value.string_value) ||
        close + 1U != occurrence.value_end ||
        value.string_value.size() > maximum_string_bytes) {
      return false;
    }
    return true;
  }
  if (occurrence.value_kind == SemanticValueKind::Boolean) {
    const std::string text = document.substr(
        occurrence.value_start,
        occurrence.value_end - occurrence.value_start);
    if (text == "true") {
      value.boolean_value = true;
      return true;
    }
    if (text == "false") {
      value.boolean_value = false;
      return true;
    }
    return false;
  }
  if (occurrence.value_kind == SemanticValueKind::Number) {
    if (occurrence.value_end - occurrence.value_start > 128U) return false;
    const std::string text = document.substr(
        occurrence.value_start,
        occurrence.value_end - occurrence.value_start);
    try {
      std::size_t consumed = 0U;
      value.number_value = std::stod(text, &consumed);
      return consumed == text.size() && std::isfinite(value.number_value);
    } catch (const std::invalid_argument&) {
      return false;
    } catch (const std::out_of_range&) {
      return false;
    }
  }
  return false;
}

}  // namespace

const char* semantic_value_kind_name(const SemanticValueKind kind) {
  switch (kind) {
    case SemanticValueKind::AnyNonNull:
      return "any_non_null";
    case SemanticValueKind::String:
      return "string";
    case SemanticValueKind::Number:
      return "number";
    case SemanticValueKind::Boolean:
      return "boolean";
    case SemanticValueKind::Object:
      return "object";
    case SemanticValueKind::Array:
      return "array";
    case SemanticValueKind::Null:
      return "null";
  }
  throw std::invalid_argument("unknown semantic JSON value kind");
}

ArtifactSemanticIndex build_artifact_semantic_index(
    const std::string& document,
    const std::size_t maximum_field_occurrences,
    const std::size_t maximum_depth) {
  if (maximum_field_occurrences == 0U || maximum_depth == 0U ||
      maximum_depth > kFallbackJsonMaxDepth) {
    throw std::invalid_argument("semantic index limits are invalid");
  }
  ArtifactSemanticIndex index;
  if (document.empty()) {
    index.parse_status = "empty_document";
    return index;
  }
  if (!json_document_is_valid_object(document)) {
    index.parse_status = "invalid_json_object";
    return index;
  }

  const std::size_t start = skip_json_space(document, 0U);
  const std::size_t end = find_json_value_end(document, start);
  if (end == std::string::npos || skip_json_space(document, end) !=
                                      document.size()) {
    throw std::logic_error(
        "semantic index root bounds disagree with strict JSON validation");
  }
  try {
    collect_value(document, start, end, "", 0U,
                  maximum_field_occurrences, maximum_depth, index);
  } catch (const std::length_error&) {
    index.parse_status = "resource_limit_exceeded";
    index.object_count = 0U;
    index.array_count = 0U;
    index.scalar_count = 0U;
    index.maximum_depth = 0U;
    index.fields.clear();
    return index;
  }
  index.parse_ok = true;
  index.parse_status = "indexed";
  return index;
}

SemanticSignalProjection project_semantic_signal(
    const ArtifactSemanticIndex& index,
    const std::string& key,
    const SemanticValueKind expected_kind,
    const bool require_nonempty,
    const std::size_t maximum_reported_paths,
    const std::string& exact_json_pointer) {
  if (key.empty() || expected_kind == SemanticValueKind::Null ||
      maximum_reported_paths == 0U ||
      (!exact_json_pointer.empty() && exact_json_pointer.front() != '/')) {
    throw std::invalid_argument("semantic signal projection is invalid");
  }
  SemanticSignalProjection projection;
  projection.key = key;
  projection.json_pointer_selector = exact_json_pointer;
  projection.expected_kind = expected_kind;
  projection.nonempty_required = require_nonempty;
  projection.document_parse_ok = index.parse_ok;
  if (!index.parse_ok) return projection;

  for (const auto& occurrence : index.fields) {
    if (occurrence.key != key ||
        !pointer_matches(occurrence, exact_json_pointer)) {
      continue;
    }
    projection.occurrence_count = inferlab::internal::checked_add_size(
        projection.occurrence_count, 1U,
        "semantic signal occurrence count");
    const bool type_ok = kind_matches(occurrence.value_kind, expected_kind);
    const bool occurrence_valid =
        type_ok && (!require_nonempty || occurrence.nonempty);
    if (occurrence_valid) {
      projection.matching_occurrence_count =
          inferlab::internal::checked_add_size(
              projection.matching_occurrence_count, 1U,
              "semantic signal matching occurrence count");
    } else {
      projection.invalid_occurrence_count =
          inferlab::internal::checked_add_size(
              projection.invalid_occurrence_count, 1U,
              "semantic signal invalid occurrence count");
    }
    append_unique(projection.observed_kinds,
                  semantic_value_kind_name(occurrence.value_kind));
    if (projection.json_pointers.size() < maximum_reported_paths) {
      projection.json_pointers.push_back(occurrence.json_pointer.empty()
                                             ? "/"
                                             : occurrence.json_pointer);
    }
  }
  projection.present = projection.occurrence_count > 0U;
  projection.kind_matches = projection.matching_occurrence_count > 0U &&
                            projection.invalid_occurrence_count == 0U;
  projection.nonempty = !require_nonempty ||
                        projection.matching_occurrence_count > 0U;
  projection.valid = projection.document_parse_ok && projection.present &&
                     projection.kind_matches && projection.nonempty;
  return projection;
}

SemanticScalarProjection project_semantic_scalars(
    const std::string& document,
    const ArtifactSemanticIndex& index,
    const std::string& key,
    const SemanticValueKind expected_kind,
    const bool require_nonempty,
    const std::string& exact_json_pointer,
    const std::size_t maximum_values,
    const std::size_t maximum_string_bytes) {
  if (key.empty() || !scalar_kind(expected_kind) || maximum_values == 0U ||
      maximum_string_bytes == 0U ||
      (!exact_json_pointer.empty() && exact_json_pointer.front() != '/')) {
    throw std::invalid_argument("semantic scalar projection is invalid");
  }
  SemanticScalarProjection projection;
  projection.evaluated = true;
  projection.document_parse_ok = index.parse_ok;
  if (!index.parse_ok) {
    append_issue(projection.issues, "semantic_document_not_indexed");
    return projection;
  }

  for (const auto& occurrence : index.fields) {
    if (occurrence.key != key ||
        !pointer_matches(occurrence, exact_json_pointer)) {
      continue;
    }
    projection.selected_occurrence_count =
        inferlab::internal::checked_add_size(
            projection.selected_occurrence_count, 1U,
            "semantic scalar selected occurrence count");
    if (projection.selected_occurrence_count > maximum_values) {
      append_issue(projection.issues,
                   "semantic_scalar_value_limit_exceeded");
      projection.values.clear();
      projection.decoded_value_count = 0U;
      projection.present = true;
      return projection;
    }
    if (occurrence.value_kind != expected_kind ||
        (require_nonempty && !occurrence.nonempty)) {
      append_issue(projection.issues,
                   "semantic_scalar_kind_or_nonempty_mismatch");
      continue;
    }
    SemanticScalarValue value;
    if (!decode_scalar(document, occurrence, maximum_string_bytes, value)) {
      append_issue(projection.issues,
                   "semantic_scalar_decode_failed_or_value_too_large");
      continue;
    }
    projection.values.push_back(std::move(value));
    projection.decoded_value_count = inferlab::internal::checked_add_size(
        projection.decoded_value_count, 1U,
        "semantic scalar decoded value count");
  }
  projection.present = projection.selected_occurrence_count > 0U;
  projection.scalar_kind_matches =
      projection.present && projection.issues.empty() &&
      projection.decoded_value_count == projection.selected_occurrence_count;
  projection.complete = projection.document_parse_ok &&
                        projection.scalar_kind_matches;
  if (!projection.present) {
    append_issue(projection.issues, "semantic_scalar_value_missing");
  }
  return projection;
}

}  // namespace inferlab_artifact
