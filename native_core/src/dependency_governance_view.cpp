#include "dependency_governance_view.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "high_value_artifact_catalog.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::dependency_governance {
namespace {

const inferlab_high_value::ManagedArtifact& artifact_contract(
    const ArtifactView view) {
  switch (view) {
    case ArtifactView::policy_gate:
      return inferlab_high_value::require_managed_artifact(
          "third_party_dependency_gate");
    case ArtifactView::sbom_gate:
      return inferlab_high_value::require_managed_artifact(
          "dependency_sbom_gate");
  }
  throw std::logic_error("unknown dependency governance artifact view");
}

template <typename Renderer>
std::string object_array_json(const std::size_t count,
                              const int indent,
                              Renderer&& renderer) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int item_indent = internal::checked_add_int(
      indent, 2, "dependency governance array JSON indent");
  std::ostringstream out;
  out << "[";
  if (count != 0U) out << "\n";
  for (std::size_t index = 0; index < count; ++index) {
    out << renderer(index, item_indent)
        << (index + 1U == count ? "\n" : ",\n");
  }
  if (count != 0U) out << pad;
  out << "]";
  return out.str();
}

std::string dependency_json(const DependencyRecord& record,
                            const int indent) {
  const auto& dependency = record.dependency;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency record JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"component_type\": "
      << inferlab_plan::json_string(dependency_kind_name(record.kind))
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(dependency.name) << ",\n";
  out << nested << "\"pinned_version\": "
      << inferlab_plan::json_string(dependency.pinned_version) << ",\n";
  out << nested << "\"version\": "
      << inferlab_plan::json_string(dependency.pinned_version) << ",\n";
  out << nested << "\"version_record_scope\": "
      << inferlab_plan::json_string(
             record.kind == DependencyKind::product_runtime
                 ? "target_product_environment_version_must_be_recorded_before_execute_claims"
                 : "configured_or_discovered_build_dependency_version_must_be_recorded_on_target_machine")
      << ",\n";
  out << nested << "\"source\": "
      << inferlab_plan::json_string(dependency.source) << ",\n";
  out << nested << "\"license\": "
      << inferlab_plan::json_string(dependency.license) << ",\n";
  out << nested << "\"cmake_option\": "
      << inferlab_plan::json_string(dependency.cmake_option) << ",\n";
  out << nested << "\"cmake_option_or_profile\": "
      << inferlab_plan::json_string(dependency.cmake_option) << ",\n";
  out << nested << "\"required\": "
      << inferlab_plan::json_bool(dependency.required) << ",\n";
  out << nested << "\"required_for_current_build\": "
      << inferlab_plan::json_bool(dependency.required) << ",\n";
  out << nested << "\"optional\": "
      << inferlab_plan::json_bool(dependency.optional) << ",\n";
  out << nested << "\"used_by\": "
      << inferlab_plan::json_string_array(dependency.used_by, nested_indent)
      << ",\n";
  out << nested << "\"engineering_value\": "
      << inferlab_plan::json_string(dependency.engineering_value) << ",\n";
  out << nested << "\"risk_level\": "
      << inferlab_plan::json_string(dependency_risk_name(record.risk))
      << ",\n";
  out << nested << "\"fallback\": "
      << inferlab_plan::json_string(dependency.fallback) << ",\n";
  out << nested << "\"claim_impact\": "
      << inferlab_plan::json_string(dependency.claim_impact) << ",\n";
  out << nested << "\"current_server_action\": "
      << inferlab_plan::json_string(dependency.current_server_action)
      << ",\n";
  out << nested << "\"metadata_complete\": "
      << inferlab_plan::json_bool(record.metadata_complete) << ",\n";
  out << nested << "\"metadata_gaps\": "
      << inferlab_plan::json_string_array(record.metadata_gaps, nested_indent)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string dependencies_json(const Plan& plan, const int indent) {
  return object_array_json(
      plan.records.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return dependency_json(plan.records[index], item_indent);
      });
}

std::string target_map_record_json(const DependencyRecord& record,
                                   const int indent) {
  const auto& dependency = record.dependency;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency target-map JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency\": "
      << inferlab_plan::json_string(dependency.name) << ",\n";
  out << nested << "\"control\": "
      << inferlab_plan::json_string(dependency.cmake_option) << ",\n";
  out << nested << "\"cmake_option_or_profile\": "
      << inferlab_plan::json_string(dependency.cmake_option) << ",\n";
  out << nested << "\"used_by\": "
      << inferlab_plan::json_string_array(dependency.used_by, nested_indent)
      << ",\n";
  out << nested << "\"targets_or_profiles\": "
      << inferlab_plan::json_string_array(dependency.used_by, nested_indent)
      << ",\n";
  out << nested << "\"required_for_current_build\": "
      << inferlab_plan::json_bool(dependency.required) << "\n";
  out << pad << "}";
  return out.str();
}

std::string dependency_to_target_map_json(const Plan& plan,
                                          const int indent) {
  return object_array_json(
      plan.records.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return target_map_record_json(plan.records[index], item_indent);
      });
}

std::string claim_map_record_json(const DependencyRecord& record,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency claim-map JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency\": "
      << inferlab_plan::json_string(record.dependency.name) << ",\n";
  out << nested << "\"claim_impact\": "
      << inferlab_plan::json_string(record.dependency.claim_impact) << ",\n";
  out << nested << "\"performance_claim_requires_results_raw\": true,\n";
  out << nested << "\"dry_run_claim_allowed\": "
      << inferlab_plan::json_bool(record.dry_run_claim_allowed) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "dependency metadata does not create runtime, correctness, product-readiness, or performance evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string dependency_to_claim_map_json(const Plan& plan,
                                         const int indent) {
  return object_array_json(
      plan.records.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return claim_map_record_json(plan.records[index], item_indent);
      });
}

std::string license_review_record_json(const DependencyRecord& record,
                                       const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency license-review JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency\": "
      << inferlab_plan::json_string(record.dependency.name) << ",\n";
  out << nested << "\"license\": "
      << inferlab_plan::json_string(record.dependency.license) << ",\n";
  out << nested << "\"review_status\": "
      << inferlab_plan::json_string(
             license_review_status_name(record.license_review_status))
      << ",\n";
  out << nested << "\"target_validation_status\": "
      << inferlab_plan::json_string(
             "recorded_pending_target_machine_and_production_license_review")
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "license recording does not imply dependency installation, production approval, or benchmark validation")
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string license_review_json(const Plan& plan, const int indent) {
  return object_array_json(
      plan.records.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return license_review_record_json(plan.records[index], item_indent);
      });
}

std::string fallback_record_json(const DependencyRecord& record,
                                 const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency fallback JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency\": "
      << inferlab_plan::json_string(record.dependency.name) << ",\n";
  out << nested << "\"optional\": "
      << inferlab_plan::json_bool(record.dependency.optional) << ",\n";
  out << nested << "\"fallback\": "
      << inferlab_plan::json_string(record.dependency.fallback) << ",\n";
  out << nested << "\"fallback_ready\": "
      << inferlab_plan::json_bool(record.fallback_ready) << ",\n";
  out << nested << "\"fallback_blocks_core_claim\": "
      << inferlab_plan::json_bool(record.fallback_blocks_core_claim) << ",\n";
  out << nested << "\"current_server_action\": "
      << inferlab_plan::json_string(record.dependency.current_server_action)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string fallback_matrix_json(const Plan& plan, const int indent) {
  return object_array_json(
      plan.records.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return fallback_record_json(plan.records[index], item_indent);
      });
}

std::string license_group_json(const LicenseGroup& group,
                               const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency license-group JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"license\": "
      << inferlab_plan::json_string(group.license) << ",\n";
  out << nested << "\"component_count\": " << group.component_count
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string license_summary_json(const Plan& plan, const int indent) {
  return object_array_json(
      plan.license_groups.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        return license_group_json(plan.license_groups[index], item_indent);
      });
}

std::string blocker_json(const DependencyRecord& record,
                         const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency blocker JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency\": "
      << inferlab_plan::json_string(record.dependency.name) << ",\n";
  out << nested << "\"category\": \"dependency_governance_metadata_gap\",\n";
  out << nested << "\"message\": "
      << inferlab_plan::json_string(
             "dependency lacks required version, source, license, control, usage, fallback, claim, or current-server metadata")
      << ",\n";
  out << nested << "\"missing_fields\": "
      << inferlab_plan::json_string_array(record.metadata_gaps, nested_indent)
      << ",\n";
  out << nested << "\"claim_impact\": "
      << inferlab_plan::json_string(record.dependency.claim_impact) << "\n";
  out << pad << "}";
  return out.str();
}

std::string blockers_json(const Plan& plan, const int indent) {
  return object_array_json(
      plan.blocker_record_indices.size(), indent,
      [&plan](const std::size_t index, const int item_indent) {
        const std::size_t record_index = plan.blocker_record_indices[index];
        if (record_index >= plan.records.size()) {
          throw std::logic_error(
              "dependency governance blocker record index is out of range");
        }
        return blocker_json(plan.records[record_index], item_indent);
      });
}

std::string summary_json(const Plan& plan,
                         const ArtifactView view,
                         const int indent) {
  const auto& summary = plan.summary;
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency summary JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"dependency_count\": " << summary.dependency_count
      << ",\n";
  out << nested << "\"required_dependency_count\": "
      << summary.required_dependency_count << ",\n";
  out << nested << "\"optional_dependency_count\": "
      << summary.optional_dependency_count << ",\n";
  out << nested << "\"product_runtime_dependency_count\": "
      << summary.product_runtime_dependency_count << ",\n";
  out << nested << "\"high_risk_dependency_count\": "
      << summary.high_risk_dependency_count << ",\n";
  out << nested << "\"fallback_ready_count\": "
      << summary.fallback_ready_count << ",\n";
  out << nested << "\"metadata_complete_dependency_count\": "
      << summary.metadata_complete_dependency_count << ",\n";
  out << nested << "\"metadata_gap_dependency_count\": "
      << summary.blocker_count << ",\n";
  out << nested << "\"permissive_license_count\": "
      << summary.permissive_license_count << ",\n";
  out << nested << "\"manual_license_review_count\": "
      << summary.manual_license_review_count << ",\n";
  if (view == ArtifactView::policy_gate) {
    out << nested << "\"dependency_gate_decision\": ";
  } else {
    out << nested << "\"sbom_gate_decision\": ";
  }
  out << inferlab_plan::json_string(
             governance_decision_name(summary.decision))
      << ",\n";
  out << nested << "\"evidence_confidence\": \"schema_only\"\n";
  out << pad << "}";
  return out.str();
}

std::string governance_role_json(const ArtifactView view,
                                 const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency governance-role JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const bool policy = view == ArtifactView::policy_gate;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact_role\": "
      << inferlab_plan::json_string(
             policy ? "policy_summary" : "sbom_component_inventory")
      << ",\n";
  out << nested << "\"paired_artifact\": "
      << inferlab_plan::json_string(
             policy ? "native_dependency_sbom_gate.json"
                    : "native_third_party_dependency_gate.json")
      << ",\n";
  out << nested << "\"division_of_responsibility\": "
      << inferlab_plan::json_string(
             policy
                 ? "this artifact records dependency policy, risk, and fallback posture; native_dependency_sbom_gate.json records the SBOM-style component inventory"
                 : "this artifact records dependency/component inventory and license metadata; native_third_party_dependency_gate.json records policy, risk, and fallback posture")
      << ",\n";
  out << nested << "\"shared_projection_owner\": "
      << inferlab_plan::json_string(
             "dependency_governance_model_and_view")
      << ",\n";
  out << nested << "\"not_a_performance_claim\": true\n";
  out << pad << "}";
  return out.str();
}

std::string dependency_policy_json(const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency policy JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"version_pinning_required\": true,\n";
  out << nested << "\"version_pinning_scope\": "
      << inferlab_plan::json_string(
             "fetched dependencies use fixed revisions; system, toolkit, and product dependencies must record their resolved target-environment version")
      << ",\n";
  out << nested << "\"license_record_required\": true,\n";
  out << nested << "\"cmake_option_required\": true,\n";
  out << nested << "\"fallback_required\": true,\n";
  out << nested
      << "\"performance_claim_requires_target_artifact\": true\n";
  out << pad << "}";
  return out.str();
}

std::string sbom_contract_json(const Plan& plan, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = internal::checked_add_int(
      indent, 2, "dependency SBOM contract JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": \"inferlab_dependency_sbom_v1\",\n";
  out << nested << "\"component_count\": "
      << plan.summary.dependency_count << ",\n";
  out << nested << "\"license_policy\": "
      << inferlab_plan::json_string(
             "recorded_only_until_target_machine_and_production_license_validation")
      << ",\n";
  out << nested << "\"performance_claim_policy\": "
      << inferlab_plan::json_string(
             "dependency presence never creates performance claims without results/raw execute artifacts")
      << "\n";
  out << pad << "}";
  return out.str();
}

void validate_projection_contract(const Plan& plan) {
  if (plan.records.empty() ||
      plan.summary.dependency_count != plan.records.size() ||
      plan.dependency_names.size() != plan.records.size() ||
      plan.product_runtime_names.size() !=
          plan.summary.product_runtime_dependency_count ||
      plan.summary.blocker_count != plan.blocker_record_indices.size() ||
      plan.target_machine_validation_commands.empty()) {
    throw std::logic_error(
        "dependency governance plan is inconsistent for projection");
  }
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    if (plan.records[index].dependency.name != plan.dependency_names[index]) {
      throw std::logic_error(
          "dependency governance name projection is out of order");
    }
  }
}

}  // namespace

std::string artifact_json(const Plan& plan, const ArtifactView view) {
  validate_projection_contract(plan);
  const auto& artifact = artifact_contract(view);
  const std::string artifact_type =
      inferlab_catalog::registry_artifact_type(artifact);
  const std::string artifact_path =
      inferlab_catalog::raw_artifact_path(artifact.artifact);
  const bool policy = view == ArtifactView::policy_gate;

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_type) << ",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, artifact_type, artifact.claim_boundary, plan.product_runtime_names,
      artifact.target, artifact_path, artifact.command);
  out << "  \"status\": "
      << inferlab_plan::json_string(
             policy ? (plan.summary.decision == GovernanceDecision::documented
                           ? "dependency_policy_ready"
                           : "dependency_policy_blocked")
                    : (plan.summary.decision == GovernanceDecision::documented
                           ? "sbom_ready"
                           : "sbom_blocked"))
      << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(artifact.claim_boundary) << ",\n";
  out << "  \"governance_role\": "
      << governance_role_json(view, 2) << ",\n";
  if (policy) {
    out << "  \"dependency_policy\": " << dependency_policy_json(2)
        << ",\n";
  } else {
    out << "  \"sbom\": " << sbom_contract_json(plan, 2) << ",\n";
  }
  out << "  \"summary\": " << summary_json(plan, view, 2) << ",\n";
  out << "  \"dependencies\": " << dependencies_json(plan, 2) << ",\n";
  out << "  \"dependency_to_target_map\": "
      << dependency_to_target_map_json(plan, 2) << ",\n";
  out << "  \"dependency_to_claim_map\": "
      << dependency_to_claim_map_json(plan, 2) << ",\n";
  if (!policy) {
    out << "  \"license_summary\": " << license_summary_json(plan, 2)
        << ",\n";
  }
  out << "  \"license_review\": " << license_review_json(plan, 2)
      << ",\n";
  out << "  \"fallback_matrix\": " << fallback_matrix_json(plan, 2)
      << ",\n";
  out << "  \"target_machine_validation_commands\": "
      << inferlab_plan::json_string_array(
             plan.target_machine_validation_commands, 2)
      << ",\n";
  out << "  \"blockers\": " << blockers_json(plan, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             plan.target_machine_validation_commands, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::dependency_governance
