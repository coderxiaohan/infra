#include "artifact_profile_section.hpp"

#include "artifact_metadata_reader.hpp"

#include <cmath>
#include <limits>
#include <string>

namespace inferlab_artifact_profile {

bool exact_number(const std::string& document,
                  const inferlab_artifact::ArtifactSemanticIndex& index,
                  const std::string& key, double& value) {
  const auto projected = inferlab_artifact::project_semantic_scalars(
      document, index, key, inferlab_artifact::SemanticValueKind::Number,
      true, "/" + key, 1U);
  if (!projected.complete || projected.values.size() != 1U) return false;
  value = projected.values.front().number_value;
  return std::isfinite(value);
}

bool exact_string(const std::string& document,
                  const inferlab_artifact::ArtifactSemanticIndex& index,
                  const std::string& key, std::string& value) {
  const auto projected = inferlab_artifact::project_semantic_scalars(
      document, index, key, inferlab_artifact::SemanticValueKind::String,
      true, "/" + key, 1U);
  if (!projected.complete || projected.values.size() != 1U) return false;
  value = projected.values.front().string_value;
  return !value.empty();
}

bool exact_bool(const std::string& document,
                const inferlab_artifact::ArtifactSemanticIndex& index,
                const std::string& key, bool& value) {
  const auto projected = inferlab_artifact::project_semantic_scalars(
      document, index, key, inferlab_artifact::SemanticValueKind::Boolean,
      false, "/" + key, 1U);
  if (!projected.complete || projected.values.size() != 1U) return false;
  value = projected.values.front().boolean_value;
  return true;
}

bool exact_non_negative_int(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, int& value) {
  double number = 0.0;
  if (!exact_number(document, index, key, number) || number < 0.0 ||
      std::floor(number) != number ||
      number > static_cast<double>(std::numeric_limits<int>::max())) {
    return false;
  }
  value = static_cast<int>(number);
  return true;
}

bool exact_size(const std::string& document,
                const inferlab_artifact::ArtifactSemanticIndex& index,
                const std::string& key, std::size_t& value) {
  double number = 0.0;
  if (!exact_number(document, index, key, number) || number < 0.0 ||
      std::floor(number) != number ||
      number >= static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    return false;
  }
  value = static_cast<std::size_t>(number);
  return true;
}

bool empty_issues_array(const std::string& section) {
  if (!inferlab_artifact::contains_top_level_key_fallback(section,
                                                           "issues")) {
    return false;
  }
  const std::string issues =
      inferlab_artifact::json_array_section_or_empty(section, "issues");
  return !issues.empty() &&
         inferlab_artifact::top_level_json_values(issues).empty();
}

}  // namespace inferlab_artifact_profile
