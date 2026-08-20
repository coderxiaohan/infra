#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_artifact {

enum class SemanticValueKind {
  AnyNonNull,
  String,
  Number,
  Boolean,
  Object,
  Array,
  Null,
};

struct SemanticFieldOccurrence {
  std::string key;
  std::string json_pointer;
  SemanticValueKind value_kind = SemanticValueKind::Null;
  bool nonempty = false;
  std::size_t value_start = 0U;
  std::size_t value_end = 0U;
};

struct ArtifactSemanticIndex {
  bool parse_ok = false;
  std::string parse_status = "not_evaluated";
  std::size_t object_count = 0U;
  std::size_t array_count = 0U;
  std::size_t scalar_count = 0U;
  std::size_t maximum_depth = 0U;
  std::vector<SemanticFieldOccurrence> fields;
};

struct SemanticSignalProjection {
  std::string key;
  std::string json_pointer_selector;
  SemanticValueKind expected_kind = SemanticValueKind::AnyNonNull;
  bool nonempty_required = true;
  bool document_parse_ok = false;
  bool present = false;
  bool kind_matches = false;
  bool nonempty = false;
  bool valid = false;
  std::size_t occurrence_count = 0U;
  std::size_t matching_occurrence_count = 0U;
  std::size_t invalid_occurrence_count = 0U;
  std::vector<std::string> observed_kinds;
  std::vector<std::string> json_pointers;
};

struct SemanticScalarValue {
  std::string json_pointer;
  SemanticValueKind kind = SemanticValueKind::Null;
  std::string string_value;
  double number_value = 0.0;
  bool boolean_value = false;
};

struct SemanticScalarProjection {
  bool evaluated = false;
  bool document_parse_ok = false;
  bool complete = false;
  bool present = false;
  bool scalar_kind_matches = false;
  std::size_t selected_occurrence_count = 0U;
  std::size_t decoded_value_count = 0U;
  std::vector<SemanticScalarValue> values;
  std::vector<std::string> issues;
};

const char* semantic_value_kind_name(SemanticValueKind kind);

ArtifactSemanticIndex build_artifact_semantic_index(
    const std::string& document,
    std::size_t maximum_field_occurrences = 65536U,
    std::size_t maximum_depth = 256U);

SemanticSignalProjection project_semantic_signal(
    const ArtifactSemanticIndex& index,
    const std::string& key,
    SemanticValueKind expected_kind,
    bool require_nonempty = true,
    std::size_t maximum_reported_paths = 32U,
    const std::string& exact_json_pointer = "");

SemanticScalarProjection project_semantic_scalars(
    const std::string& document,
    const ArtifactSemanticIndex& index,
    const std::string& key,
    SemanticValueKind expected_kind,
    bool require_nonempty = true,
    const std::string& exact_json_pointer = "",
    std::size_t maximum_values = 64U,
    std::size_t maximum_string_bytes = 4096U);

}  // namespace inferlab_artifact
