#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_release_pipeline {
namespace {

using evidence_query_closure_view_detail::write_bool_field;
using evidence_query_closure_view_detail::write_count_field;
using evidence_query_closure_view_detail::write_json_field;

std::size_t unsigned_or_zero(
    const inferlab_artifact::UnsignedFieldProjection& field) {
  return field.present && !field.invalid ? field.value : 0U;
}

bool bool_or_false(const inferlab_artifact::BoolFieldProjection& field) {
  return field.present && !field.invalid && field.value;
}

std::string string_or_null(
    const inferlab_artifact::StringFieldProjection& field) {
  return field.present && !field.invalid
             ? inferlab_plan::json_string(field.value)
             : "null";
}

}  // namespace

std::string evidence_query_operation_catalog_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline operation catalog field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& catalog = projection.operation_catalog;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad, "direct_artifact_readable",
                   projection.source_readable);
  write_json_field(out, field_pad, "direct_artifact_read_status",
                   inferlab_plan::json_string(projection.source_read_status));
  write_bool_field(out, field_pad, "summary_usable",
                   projection.summary_usable);
  write_bool_field(out, field_pad, "operation_catalog_present",
                   catalog.object_present);
  write_count_field(out, field_pad, "contract_issue_count",
                    catalog.contract_issue_count);
  write_count_field(out, field_pad, "supported_operation_count",
                    unsigned_or_zero(catalog.supported_count));
  write_count_field(out, field_pad, "read_only_operation_count",
                    unsigned_or_zero(catalog.read_only_count));
  write_count_field(out, field_pad, "snapshot_required_operation_count",
                    unsigned_or_zero(catalog.snapshot_required_count));
  write_count_field(out, field_pad, "selector_recommended_operation_count",
                    unsigned_or_zero(catalog.selector_recommended_count));
  write_count_field(out, field_pad, "target_required_operation_count",
                    unsigned_or_zero(catalog.target_required_count));
  write_bool_field(out, field_pad, "read_only_complete",
                   catalog.read_only_complete);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "operation catalog closure documents Evidence Query CLI semantics "
          "only; it does not execute query operations, refresh artifacts, or "
          "create benchmark/replay/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_selected_operation_result_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline selected operation field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& selected = projection.selected_operation;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad, "selected_operation_result_present",
                   selected.object_present);
  write_count_field(out, field_pad, "contract_issue_count",
                    selected.contract_issue_count);
  write_bool_field(out, field_pad, "selected_result_present",
                   bool_or_false(selected.result_present));
  write_bool_field(out, field_pad, "selected_result_input_ready",
                   bool_or_false(selected.input_ready));
  write_count_field(out, field_pad, "selected_result_payload_count",
                    unsigned_or_zero(selected.payload_count));
  write_json_field(out, field_pad, "selected_result_kind",
                   string_or_null(selected.result_kind));
  write_count_field(out, field_pad, "show_artifact_candidate_count",
                    unsigned_or_zero(
                        selected.show_artifact_candidate_count));
  write_bool_field(out, field_pad, "show_artifact_candidate_truncated",
                   bool_or_false(
                       selected.show_artifact_candidate_truncated));
  write_bool_field(out, field_pad, "show_artifact_exact_match",
                   bool_or_false(selected.show_artifact_exact_match));
  write_bool_field(out, field_pad, "show_artifact_selector_ambiguous",
                   bool_or_false(
                       selected.show_artifact_selector_ambiguous));
  write_json_field(out, field_pad, "show_artifact_selection_strategy",
                   string_or_null(
                       selected.show_artifact_selection_strategy));
  write_bool_field(out, field_pad, "payload_selector_focus_present",
                   bool_or_false(
                       selected.payload_selector_focus_present));
  write_json_field(out, field_pad, "payload_selector_focus_status",
                   string_or_null(
                       selected.payload_selector_focus_status));
  write_count_field(out, field_pad, "payload_selector_focus_match_count",
                    unsigned_or_zero(
                        selected.payload_selector_focus_match_count));
  write_bool_field(out, field_pad, "payload_selector_focus_ambiguous",
                   bool_or_false(
                       selected.payload_selector_focus_ambiguous));
  write_count_field(
      out, field_pad, "payload_selector_focus_source_location_gap_count",
      unsigned_or_zero(
          selected.payload_selector_focus_source_location_gap_count));
  write_bool_field(out, field_pad, "release_gate_consumed", true);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "selected operation result closure forwards Evidence Query UX "
          "payload state only; it does not execute query operations, refresh "
          "artifacts, or create benchmark/replay/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_operation_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const auto& catalog = projection.operation_catalog;
  const auto& selected = projection.selected_operation;
  const auto& resolution = projection.selector_resolution;
  const auto& export_signals = projection.selector_repair_card_export;
  const auto& action_plan = projection.selector_action_plan;
  std::ostringstream out;
  write_count_field(
      out, pad, "evidence_query_closure_source_contract_issue_count",
      projection.source_contract_issue_count);
  write_count_field(
      out, pad, "evidence_query_operation_catalog_contract_issue_count",
      catalog.contract_issue_count);
  write_count_field(
      out, pad, "evidence_query_selected_result_contract_issue_count",
      selected.contract_issue_count);
  write_bool_field(out, pad, "evidence_query_operation_catalog_present",
                   catalog.object_present);
  write_count_field(out, pad,
                    "evidence_query_operation_catalog_supported_count",
                    unsigned_or_zero(catalog.supported_count));
  write_count_field(out, pad,
                    "evidence_query_operation_catalog_read_only_count",
                    unsigned_or_zero(catalog.read_only_count));
  write_count_field(
      out, pad, "evidence_query_operation_catalog_snapshot_required_count",
      unsigned_or_zero(catalog.snapshot_required_count));
  write_count_field(
      out, pad, "evidence_query_operation_catalog_selector_recommended_count",
      unsigned_or_zero(catalog.selector_recommended_count));
  write_count_field(
      out, pad, "evidence_query_operation_catalog_target_required_count",
      unsigned_or_zero(catalog.target_required_count));
  write_bool_field(out, pad, "evidence_query_selected_result_present",
                   bool_or_false(selected.result_present));
  write_bool_field(out, pad, "evidence_query_selected_result_input_ready",
                   bool_or_false(selected.input_ready));
  write_count_field(out, pad,
                    "evidence_query_selected_result_payload_count",
                    unsigned_or_zero(selected.payload_count));
  write_json_field(out, pad, "evidence_query_selected_result_kind",
                   string_or_null(selected.result_kind));
  write_count_field(out, pad,
                    "evidence_query_show_artifact_candidate_count",
                    unsigned_or_zero(
                        selected.show_artifact_candidate_count));
  write_bool_field(out, pad,
                   "evidence_query_show_artifact_candidate_truncated",
                   bool_or_false(
                       selected.show_artifact_candidate_truncated));
  write_bool_field(out, pad, "evidence_query_show_artifact_exact_match",
                   bool_or_false(selected.show_artifact_exact_match));
  write_bool_field(out, pad,
                   "evidence_query_show_artifact_selector_ambiguous",
                   bool_or_false(
                       selected.show_artifact_selector_ambiguous));
  write_json_field(out, pad,
                   "evidence_query_show_artifact_selection_strategy",
                   string_or_null(
                       selected.show_artifact_selection_strategy));
  write_bool_field(out, pad,
                   "evidence_query_payload_selector_focus_present",
                   bool_or_false(
                       selected.payload_selector_focus_present));
  write_json_field(out, pad,
                   "evidence_query_payload_selector_focus_status",
                   string_or_null(
                       selected.payload_selector_focus_status));
  write_count_field(out, pad,
                    "evidence_query_payload_selector_focus_match_count",
                    unsigned_or_zero(
                        selected.payload_selector_focus_match_count));
  write_bool_field(out, pad,
                   "evidence_query_payload_selector_focus_ambiguous",
                   bool_or_false(
                       selected.payload_selector_focus_ambiguous));
  write_count_field(
      out, pad,
      "evidence_query_payload_selector_focus_source_location_gap_count",
      unsigned_or_zero(
          selected.payload_selector_focus_source_location_gap_count));
  write_bool_field(out, pad, "evidence_query_selector_resolution_present",
                   resolution.present);
  write_json_field(
      out, pad, "evidence_query_selector_resolution_status",
      resolution.resolution_status_present
          ? inferlab_plan::json_string(resolution.resolution_status)
          : "null");
  write_json_field(out, pad, "evidence_query_selector_kind",
                   resolution.kind_present
                       ? inferlab_plan::json_string(resolution.kind)
                       : "null");
  write_count_field(out, pad, "evidence_query_selector_match_count",
                    resolution.match_count);
  write_bool_field(out, pad, "evidence_query_selector_ambiguous",
                   resolution.ambiguous);
  write_count_field(out, pad, "evidence_query_selector_repair_card_count",
                    export_signals.card_count);
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_emitted_count",
                    export_signals.emitted_count);
  write_bool_field(out, pad,
                   "evidence_query_selector_repair_card_truncated",
                   export_signals.truncated);
  write_json_field(
      out, pad, "evidence_query_selector_repair_card_export_status",
      export_signals.export_status_present
          ? inferlab_plan::json_string(export_signals.export_status)
          : "null");
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_embedded_limit",
                    export_signals.embedded_limit);
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_full_export_count",
                    export_signals.full_export_count);
  write_bool_field(
      out, pad,
      "evidence_query_selector_repair_card_external_export_configured",
      export_signals.external_export_configured);
  write_bool_field(
      out, pad,
      "evidence_query_selector_repair_card_external_export_written",
      export_signals.external_export_written);
  write_json_field(
      out, pad,
      "evidence_query_selector_repair_card_external_export_status",
      export_signals.external_export_status_present
          ? inferlab_plan::json_string(export_signals.external_export_status)
          : "null");
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_current_repo_count",
                    export_signals.current_repo_count);
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_current_server_count",
                    export_signals.current_server_count);
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_target_machine_count",
                    export_signals.target_machine_count);
  write_count_field(out, pad,
                    "evidence_query_selector_repair_card_target_product_count",
                    export_signals.target_product_count);
  write_count_field(
      out, pad,
      "evidence_query_selector_repair_card_source_location_gap_card_count",
      export_signals.source_location_gap_card_count);
  write_json_field(
      out, pad,
      "evidence_query_selector_repair_card_export_review_status",
      export_signals.export_review_status_present
          ? inferlab_plan::json_string(export_signals.export_review_status)
          : "null");
  write_json_field(
      out, pad,
      "evidence_query_selector_repair_card_export_blocker_reason",
      export_signals.export_blocker_reason_present
          ? inferlab_plan::json_string(export_signals.export_blocker_reason)
          : "null");
  write_bool_field(
      out, pad,
      "evidence_query_selector_repair_card_external_file_required",
      export_signals.external_file_required);
  write_count_field(out, pad,
                    "evidence_query_selector_source_location_count",
                    resolution.source_location_count);
  write_count_field(out, pad,
                    "evidence_query_selector_source_location_gap_count",
                    resolution.source_location_gap_count);
  write_count_field(out, pad,
                    "evidence_query_selector_current_repo_action_count",
                    resolution.current_repo_action_count);
  write_count_field(out, pad,
                    "evidence_query_selector_current_server_action_count",
                    resolution.current_server_action_count);
  write_count_field(out, pad,
                    "evidence_query_selector_target_machine_action_count",
                    resolution.target_machine_action_count);
  write_count_field(out, pad,
                    "evidence_query_selector_target_product_action_count",
                    resolution.target_product_action_count);
  write_json_field(
      out, pad, "evidence_query_selector_action_plan_decision",
      action_plan.decision_present
          ? inferlab_plan::json_string(action_plan.decision)
          : "null");
  write_count_field(out, pad,
                    "evidence_query_selector_action_plan_step_count",
                    action_plan.step_count);
  write_count_field(
      out, pad,
      "evidence_query_selector_action_plan_current_repo_step_count",
      action_plan.current_repo_step_count);
  write_count_field(
      out, pad,
      "evidence_query_selector_action_plan_target_or_validation_step_count",
      action_plan.target_or_validation_step_count);
  write_count_field(
      out, pad,
      "evidence_query_selector_action_plan_source_location_gap_count",
      action_plan.source_location_gap_count);
  write_count_field(
      out, pad,
      "evidence_query_selector_action_plan_readiness_blocker_count",
      action_plan.readiness_blocker_count);
  write_json_field(
      out, pad, "evidence_query_selector_action_plan_readiness_state",
      action_plan.readiness_state_present
          ? inferlab_plan::json_string(action_plan.readiness_state)
          : "null");
  return out.str();
}

}  // namespace inferlab_release_pipeline
