#include "mature_product_profile_contract.hpp"

#include "advanced_serving_internal.hpp"

#include <stdexcept>

namespace inferlab::mature_product_profiles {

const std::vector<ProductProfileFieldSpec>&
product_profile_required_artifact_field_specs() {
  static const std::vector<ProductProfileFieldSpec> fields = {
      {"schema_version", ProductProfileFieldKind::string},
      {"schema", ProductProfileFieldKind::object},
      {"product", ProductProfileFieldKind::string},
      {"product_name", ProductProfileFieldKind::string},
      {"category", ProductProfileFieldKind::string},
      {"mode", ProductProfileFieldKind::string},
      {"status", ProductProfileFieldKind::string},
      {"executed", ProductProfileFieldKind::boolean},
      {"replayed", ProductProfileFieldKind::boolean},
      {"generated_at", ProductProfileFieldKind::string},
      {"official_sources", ProductProfileFieldKind::array},
      {"inferlab_adapter", ProductProfileFieldKind::object},
      {"environment_preflight", ProductProfileFieldKind::object},
      {"version_capture", ProductProfileFieldKind::object},
      {"config_snapshot", ProductProfileFieldKind::object},
      {"planned_commands", ProductProfileFieldKind::object},
      {"raw_metrics", ProductProfileFieldKind::object},
      {"key_features", ProductProfileFieldKind::array},
      {"not_implemented_by_inferlab", ProductProfileFieldKind::array},
      {"local_optimization_scope", ProductProfileFieldKind::array},
      {"claim_scope", ProductProfileFieldKind::object},
  };
  static const bool validated = [] {
    std::vector<std::string> names;
    names.reserve(fields.size());
    for (const auto& field : fields) names.push_back(field.name);
    advanced_serving_internal::require_unique_nonempty_strings(
        names, "product profile required artifact field specs");
    return true;
  }();
  (void)validated;
  return fields;
}

const std::vector<std::string>& product_profile_required_artifact_fields() {
  static const std::vector<std::string> fields = [] {
    std::vector<std::string> names;
    const auto& specs = product_profile_required_artifact_field_specs();
    names.reserve(specs.size());
    for (const auto& field : specs) names.push_back(field.name);
    return names;
  }();
  static const bool validated = [] {
    if (fields.size() !=
        product_profile_required_artifact_field_specs().size()) {
      throw std::logic_error(
          "product profile field-name projection size mismatch");
    }
    advanced_serving_internal::require_unique_nonempty_strings(
        fields, "product profile required artifact field names");
    return true;
  }();
  (void)validated;
  return fields;
}

}  // namespace inferlab::mature_product_profiles
