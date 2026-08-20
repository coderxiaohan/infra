#include "evidence_query_operation_contract_view.hpp"

#include "evidence_domain_utils.hpp"
#include "evidence_query_operation_policy.hpp"
#include "evidence_query_selection.hpp"
#include "evidence_query_snapshot_diff.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_evidence_query {

std::string operation_descriptor_json(
    const QueryOperation& operation,
    const SnapshotDiffSignals& snapshot_diff,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const bool selector_recommended = operation_selector_recommended(operation);
  const bool target_required = operation_target_required(operation);
  const bool snapshot_required =
      operation_snapshot_inputs_required(operation);
  const bool snapshot_inputs_configured =
      snapshot_diff.before_configured && snapshot_diff.after_configured;

  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"operation\": "
      << inferlab_plan::json_string(operation.operation) << ",\n";
  out << nested << "\"human_label\": "
      << inferlab_plan::json_string(operation.human_label) << ",\n";
  out << nested << "\"normalized_query\": "
      << inferlab_plan::json_string(operation.normalized_query) << ",\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(operation.selector) << ",\n";
  out << nested << "\"selector_recommended\": "
      << (selector_recommended ? "true" : "false") << ",\n";
  out << nested << "\"selector_present\": "
      << (!operation.selector.empty() ? "true" : "false") << ",\n";
  out << nested << "\"target\": "
      << inferlab_plan::json_string(operation.target) << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_readiness_target_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"target_required\": "
      << (target_required ? "true" : "false") << ",\n";
  out << nested << "\"target_present\": "
      << (!operation.target.empty() ? "true" : "false") << ",\n";
  out << nested << "\"readiness_target\": "
      << (inferlab_evidence::is_readiness_target(operation.target)
              ? inferlab_plan::json_string(operation.target)
              : "null")
      << ",\n";
  out << nested << "\"snapshot_inputs_required\": "
      << (snapshot_required ? "true" : "false") << ",\n";
  out << nested << "\"snapshot_inputs_configured\": "
      << (snapshot_inputs_configured ? "true" : "false") << ",\n";
  out << nested << "\"before_snapshot_configured\": "
      << (snapshot_diff.before_configured ? "true" : "false") << ",\n";
  out << nested << "\"after_snapshot_configured\": "
      << (snapshot_diff.after_configured ? "true" : "false") << ",\n";
  out << nested << "\"current_repo_action\": "
      << inferlab_plan::json_string(operation_current_repo_action(operation))
      << ",\n";
  out << nested << "\"target_machine_action\": "
      << inferlab_plan::json_string(operation_target_machine_action(operation))
      << ",\n";
  out << nested << "\"executes_commands\": false,\n";
  out << nested
      << "\"claim_boundary\": \"operation descriptor documents read-only Evidence Query UX semantics only; it does not execute repairs, refresh artifacts, compare snapshot contents, run validation, or create benchmark/replay/execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

std::string operation_catalog_json(const int indent) {
  const auto& specs = evidence_query_operation_catalog();
  const auto summary = evidence_query_operation_catalog_summary();
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"supported_operation_count\": "
      << summary.supported_operation_count << ",\n";
  out << nested << "\"read_only_operation_count\": "
      << summary.read_only_operation_count << ",\n";
  out << nested << "\"snapshot_required_operation_count\": "
      << summary.snapshot_required_operation_count << ",\n";
  out << nested << "\"selector_recommended_operation_count\": "
      << summary.selector_recommended_operation_count << ",\n";
  out << nested << "\"target_required_operation_count\": "
      << summary.target_required_operation_count << ",\n";
  out << nested << "\"operations\": [";
  bool first = true;
  for (const auto& spec : specs) {
    if (!first) out << ",";
    first = false;
    QueryOperation operation;
    operation.operation = spec.operation;
    if (spec.supported_targets.size() == 1U) {
      operation.target = spec.supported_targets.front();
    }
    out << "\n" << nested << "  {\n";
    out << nested << "    \"operation\": "
        << inferlab_plan::json_string(spec.operation) << ",\n";
    out << nested << "    \"positional_form\": "
        << inferlab_plan::json_string(spec.positional_form) << ",\n";
    out << nested << "    \"ux_form\": "
        << inferlab_plan::json_string(spec.ux_form) << ",\n";
    out << nested << "    \"selector_hint\": "
        << inferlab_plan::json_string(spec.selector_hint) << ",\n";
    out << nested << "    \"selector_recommended\": "
        << (operation_selector_recommended(operation) ? "true" : "false")
        << ",\n";
    out << nested << "    \"target_hint\": "
        << inferlab_plan::json_string(spec.target_hint) << ",\n";
    out << nested << "    \"target_required\": "
        << (operation_target_required(operation) ? "true" : "false")
        << ",\n";
    out << nested << "    \"snapshot_inputs_required\": "
        << (operation_snapshot_inputs_required(operation) ? "true" : "false")
        << ",\n";
    out << nested << "    \"supported_targets\": "
        << inferlab_plan::json_string_array(spec.supported_targets,
                                            indent + 4)
        << ",\n";
    out << nested << "    \"read_only\": true,\n";
    out << nested << "    \"executes_commands\": false,\n";
    out << nested << "    \"current_repo_action\": "
        << inferlab_plan::json_string(spec.catalog_current_repo_action)
        << ",\n";
    out << nested << "    \"target_machine_action\": "
        << inferlab_plan::json_string(spec.catalog_target_machine_action)
        << ",\n";
    out << nested
        << "    \"claim_boundary\": \"operation catalog documents Evidence Query UX semantics only; it does not execute repairs, refresh artifacts, compare snapshot contents, or create benchmark/replay/execute evidence\"\n";
    out << nested << "  }";
  }
  if (!specs.empty()) out << "\n" << nested;
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"operation catalog is a static read-only CLI contract for downstream reports and release gates; it does not execute any operation\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
