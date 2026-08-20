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

std::string optional_string_json(const bool present,
                                 const std::string& value) {
  return present ? inferlab_plan::json_string(value) : "null";
}

}  // namespace

std::string evidence_query_selector_resolution_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline selector closure field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& resolution = projection.selector_resolution;
  const auto& export_signals = projection.selector_repair_card_export;
  const auto& action_plan = projection.selector_action_plan;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad, "selector_resolution_present",
                   resolution.present);
  write_json_field(
      out, field_pad, "resolution_status",
      optional_string_json(resolution.resolution_status_present,
                           resolution.resolution_status));
  write_json_field(out, field_pad, "selector_kind",
                   optional_string_json(resolution.kind_present,
                                        resolution.kind));
  write_count_field(out, field_pad, "selector_match_count",
                    resolution.match_count);
  write_bool_field(out, field_pad, "selector_ambiguous",
                   resolution.ambiguous);
  write_count_field(out, field_pad, "selector_repair_card_count",
                    export_signals.card_count);
  write_count_field(out, field_pad, "selector_repair_card_emitted_count",
                    export_signals.emitted_count);
  write_bool_field(out, field_pad, "selector_repair_card_truncated",
                   export_signals.truncated);
  write_json_field(
      out, field_pad, "selector_repair_card_export_status",
      optional_string_json(export_signals.export_status_present,
                           export_signals.export_status));
  write_count_field(out, field_pad, "selector_repair_card_embedded_limit",
                    export_signals.embedded_limit);
  write_count_field(out, field_pad, "selector_repair_card_full_export_count",
                    export_signals.full_export_count);
  write_bool_field(out, field_pad,
                   "selector_repair_card_external_export_configured",
                   export_signals.external_export_configured);
  write_bool_field(out, field_pad,
                   "selector_repair_card_external_export_written",
                   export_signals.external_export_written);
  write_json_field(
      out, field_pad, "selector_repair_card_external_export_status",
      optional_string_json(export_signals.external_export_status_present,
                           export_signals.external_export_status));
  write_count_field(out, field_pad, "selector_repair_card_current_repo_count",
                    export_signals.current_repo_count);
  write_count_field(out, field_pad,
                    "selector_repair_card_current_server_count",
                    export_signals.current_server_count);
  write_count_field(out, field_pad,
                    "selector_repair_card_target_machine_count",
                    export_signals.target_machine_count);
  write_count_field(out, field_pad,
                    "selector_repair_card_target_product_count",
                    export_signals.target_product_count);
  write_count_field(out, field_pad,
                    "selector_repair_card_source_location_gap_card_count",
                    export_signals.source_location_gap_card_count);
  write_json_field(
      out, field_pad, "selector_repair_card_export_review_status",
      optional_string_json(export_signals.export_review_status_present,
                           export_signals.export_review_status));
  write_json_field(
      out, field_pad, "selector_repair_card_export_blocker_reason",
      optional_string_json(export_signals.export_blocker_reason_present,
                           export_signals.export_blocker_reason));
  write_bool_field(out, field_pad,
                   "selector_repair_card_external_file_required",
                   export_signals.external_file_required);
  write_count_field(out, field_pad, "selector_source_location_count",
                    resolution.source_location_count);
  write_count_field(out, field_pad, "selector_source_location_gap_count",
                    resolution.source_location_gap_count);
  write_count_field(out, field_pad, "selector_current_repo_action_count",
                    resolution.current_repo_action_count);
  write_count_field(out, field_pad, "selector_current_server_action_count",
                    resolution.current_server_action_count);
  write_count_field(out, field_pad, "selector_target_machine_action_count",
                    resolution.target_machine_action_count);
  write_count_field(out, field_pad, "selector_target_product_action_count",
                    resolution.target_product_action_count);
  write_bool_field(out, field_pad, "selector_action_plan_present",
                   action_plan.present);
  write_json_field(out, field_pad, "selector_action_plan_decision",
                   optional_string_json(action_plan.decision_present,
                                        action_plan.decision));
  write_count_field(out, field_pad, "selector_action_plan_step_count",
                    action_plan.step_count);
  write_count_field(out, field_pad,
                    "selector_action_plan_current_repo_step_count",
                    action_plan.current_repo_step_count);
  write_count_field(
      out, field_pad,
      "selector_action_plan_target_or_validation_step_count",
      action_plan.target_or_validation_step_count);
  write_count_field(out, field_pad,
                    "selector_action_plan_source_location_gap_count",
                    action_plan.source_location_gap_count);
  write_count_field(out, field_pad,
                    "selector_action_plan_readiness_blocker_count",
                    action_plan.readiness_blocker_count);
  write_json_field(out, field_pad, "selector_action_plan_readiness_state",
                   optional_string_json(action_plan.readiness_state_present,
                                        action_plan.readiness_state));
  write_bool_field(out, field_pad, "release_gate_consumed", true);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "selector-resolution closure forwards Evidence Query selector "
          "routing metadata only; it does not execute query operations, "
          "refresh artifacts, or create benchmark/replay/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_snapshot_diff_review_closure_json(
    const ReleasePipelineSnapshotDiffReleaseLinkProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline snapshot review field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& snapshot = projection.snapshot_diff;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.direct_artifact_present);
  write_bool_field(out, field_pad, "snapshot_diff_query_present",
                   snapshot.query_present);
  write_bool_field(out, field_pad, "deep_semantic_compared",
                   snapshot.deep_semantic_compared);
  write_bool_field(out, field_pad, "deep_semantic_truncated",
                   snapshot.deep_semantic_truncated);
  write_count_field(out, field_pad, "added_deep_path_count",
                    snapshot.added_deep_path_count);
  write_count_field(out, field_pad, "removed_deep_path_count",
                    snapshot.removed_deep_path_count);
  write_count_field(out, field_pad, "changed_deep_value_count",
                    snapshot.changed_deep_value_count);
  write_json_field(
      out, field_pad, "deep_semantic_decision",
      snapshot.deep_semantic_decision.empty()
          ? "null"
          : inferlab_plan::json_string(snapshot.deep_semantic_decision));
  write_count_field(out, field_pad, "repair_path_count",
                    projection.repair_path_count);
  write_count_field(out, field_pad, "source_location_gap_count",
                    projection.source_location_gap_count);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "snapshot-diff review summarizes bounded Evidence Query JSON "
          "Pointer path differences only; it does not execute artifact "
          "producers, refresh snapshots, compare benchmark performance, or "
          "create replay/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
