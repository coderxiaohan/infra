#include "artifact_schema_registry_view.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::artifact_schema_registry {
namespace {

std::string schema_json(
    const inferlab_high_value::ManagedArtifact& spec,
    const std::vector<std::string>& optional_fields,
    const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact schema registry nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"id\": "
      << inferlab_plan::json_string(spec.id) << ",\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(spec.artifact) << ",\n";
  out << nested << "\"artifact_type\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_artifact_type(spec))
      << ",\n";
  out << nested << "\"evidence_level\": "
      << inferlab_plan::json_string(spec.evidence_level) << ",\n";
  out << nested << "\"report_kind\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_report_kind(spec)) << ",\n";
  out << nested << "\"summary_group\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_summary_group(spec)) << ",\n";
  out << nested << "\"final_report_group\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_final_report_group(spec)) << ",\n";
  out << nested << "\"prometheus_category\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_prometheus_category(spec)) << ",\n";
  out << nested << "\"schema_version\": 1,\n";
  out << nested << "\"producer_target\": "
      << inferlab_plan::json_string(spec.target) << ",\n";
  out << nested << "\"report\": "
      << inferlab_plan::json_string(spec.report) << ",\n";
  out << nested << "\"required_fields\": "
      << inferlab_plan::json_string_array(
             inferlab_catalog::registry_required_fields(spec),
             nested_indent)
      << ",\n";
  out << nested << "\"summary_metrics\": "
      << inferlab_plan::json_string_array(
             inferlab_catalog::registry_required_metrics(spec),
             nested_indent)
      << ",\n";
  out << nested << "\"optional_fields\": "
      << inferlab_plan::json_string_array(
             optional_fields, nested_indent)
      << ",\n";
  out << nested << "\"allowed_modes\": "
      << inferlab_plan::json_string_array(
             inferlab_catalog::registry_allowed_modes(), nested_indent)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_claim_boundary(spec))
      << ",\n";
  out << nested << "\"freshness_policy\": "
      << inferlab_plan::json_string(
             inferlab_catalog::registry_freshness_policy(spec))
      << ",\n";
  out << nested << "\"report_wiring_required\": true,\n";
  out << nested << "\"freshness_tracking_required\": true,\n";
  out << nested
      << "\"release_decision_impact\": \"missing or stale schema evidence blocks report_ready claims\",\n";
  out << nested << "\"refresh_command\": "
      << inferlab_plan::json_string(spec.command) << "\n";
  out << pad << "}";
  return out.str();
}

std::string schemas_json(const RegistryPlan& plan, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact schema registry array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < plan.artifacts.size(); ++i) {
    out << schema_json(
        *plan.artifacts[i], plan.optional_fields, nested_indent)
        << (i + 1U == plan.artifacts.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string confidence_levels_json(const RegistryPlan& plan,
                                   const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact confidence registry nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < plan.confidence_levels.size(); ++i) {
    const auto& spec = plan.confidence_levels[i];
    out << nested << "{\n";
    out << nested << "  \"level\": "
        << inferlab_plan::json_string(
               inferlab_evidence::evidence_level_name(spec.level))
        << ",\n";
    out << nested << "  \"meaning\": "
        << inferlab_plan::json_string(spec.meaning) << "\n";
    out << nested << "}"
        << (i + 1U == plan.confidence_levels.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string claim_rules_json(const RegistryPlan& plan,
                             const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact claim-rule nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < plan.claim_boundary_rules.size(); ++i) {
    const auto& rule = plan.claim_boundary_rules[i];
    out << nested << "{\n";
    out << nested << "  \"rule\": "
        << inferlab_plan::json_string(rule.rule) << ",\n";
    out << nested << "  \"decision\": "
        << inferlab_plan::json_string(rule.decision) << "\n";
    out << nested << "}"
        << (i + 1U == plan.claim_boundary_rules.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string release_rules_json(const RegistryPlan& plan,
                               const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "artifact release-rule nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0U; i < plan.release_decision_rules.size(); ++i) {
    const auto& rule = plan.release_decision_rules[i];
    out << nested << "{\n";
    out << nested << "  \"decision\": "
        << inferlab_plan::json_string(
               inferlab_evidence::release_decision_name(rule.decision))
        << ",\n";
    out << nested << "  \"requires\": "
        << inferlab_plan::json_string(rule.requires) << "\n";
    out << nested << "}"
        << (i + 1U == plan.release_decision_rules.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace

std::string artifact_json(const RegistryPlan& plan) {
  validate_plan(plan);
  const auto& self = inferlab_high_value::require_managed_artifact(
      "artifact_schema_registry");
  const auto claim_scope =
      inferlab_catalog::registry_claim_boundary(self);
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_artifact_schema_registry\",\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, "native_artifact_schema_registry", claim_scope, {},
      self.target, "results/raw/" + self.artifact, self.command);
  out << "  \"status\": \"registry_ready\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(claim_scope) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"registered_artifact_count\": "
      << plan.artifacts.size() << ",\n";
  out << "    \"registered_schema_file_count\": "
      << plan.registry_files.size() << ",\n";
  out << "    \"selector_repair_card_export_schema_registered\": "
      << inferlab_plan::json_bool(
             plan.selector_export_schema_registered)
      << ",\n";
  out << "    \"schema_registry_decision\": \"documented\",\n";
  out << "    \"evidence_confidence\": \"schema_only\"\n";
  out << "  },\n";
  out << "  \"registry_files\": "
      << inferlab_plan::json_string_array(plan.registry_files, 2)
      << ",\n";
  out << "  \"artifact_schemas\": " << schemas_json(plan, 2) << ",\n";
  out << "  \"confidence_levels\": "
      << confidence_levels_json(plan, 2) << ",\n";
  out << "  \"claim_boundary_rules\": "
      << claim_rules_json(plan, 2) << ",\n";
  out << "  \"release_decision_rules\": "
      << release_rules_json(plan, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             plan.recommended_next_commands, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::artifact_schema_registry
