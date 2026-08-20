#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_release_pipeline {
namespace {

using evidence_query_closure_view_detail::write_bool_field;
using evidence_query_closure_view_detail::write_count_field;
using evidence_query_closure_view_detail::write_json_field;

struct QueryUxViewContext {
  const ReleasePipelineEvidenceQueryClosureProjection& projection;
  const inferlab_evidence::EvidenceQueryNavigationSignals& navigation;
  const EvidenceQueryQueryUxObjectPresenceProjection& objects;
  std::string pad;
  std::string nested;
};

QueryUxViewContext query_ux_view_context(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent, const char* indent_label) {
  return {projection,
          projection.query_ux.navigation,
          projection.query_ux.objects,
          inferlab_plan::json_indent(indent),
          inferlab_plan::json_indent(inferlab::internal::checked_add_int(
              indent, 2, indent_label))};
}

void write_query_ux_source_header(std::ostringstream& out,
                                  const QueryUxViewContext& context,
                                  const std::string& presence_key,
                                  const bool object_present) {
  write_json_field(out, context.nested, "source",
                   inferlab_plan::json_string(
                       context.projection.artifact.artifact));
  write_bool_field(out, context.nested, "direct_artifact_present",
                   context.projection.source_present);
  write_bool_field(out, context.nested, presence_key, object_present);
}

}  // namespace

std::string evidence_query_explain_tree_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto context = query_ux_view_context(
      projection, indent,
      "release pipeline Evidence Query explain-tree closure indent");
  const auto& signals = context.navigation.explain_tree_coverage;
  std::ostringstream out;
  out << context.pad << "{\n";
  write_query_ux_source_header(out, context, "explain_tree_present",
                               context.objects.explain_tree);
  write_count_field(out, context.nested, "node_count", signals.node_count);
  write_count_field(out, context.nested, "source_exact_node_count",
                    signals.source_exact_node_count);
  write_count_field(out, context.nested, "source_gap_node_count",
                    signals.source_gap_node_count);
  write_count_field(out, context.nested, "current_repo_source_gap_count",
                    signals.current_repo_source_gap_count);
  write_count_field(out, context.nested,
                    "target_or_validation_source_gap_count",
                    signals.target_or_validation_source_gap_count);
  write_count_field(out, context.nested, "source_exactness_percent",
                    signals.source_exactness_percent);
  write_bool_field(out, context.nested, "release_gate_consumed", true);
  write_json_field(
      out, context.nested, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query explain-tree source "
          "coverage as read-only blocker explanation metadata only; it does "
          "not execute repairs or create benchmark/replay/execute evidence"),
      false);
  out << context.pad << "}";
  return out.str();
}

std::string evidence_query_blocker_drilldown_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto context = query_ux_view_context(
      projection, indent,
      "release pipeline Evidence Query blocker-drilldown closure indent");
  const auto& signals = context.navigation.blocker_drilldown;
  std::ostringstream out;
  out << context.pad << "{\n";
  write_query_ux_source_header(out, context, "blocker_drilldown_present",
                               context.objects.blocker_drilldown);
  write_count_field(out, context.nested, "record_count", signals.record_count);
  write_count_field(out, context.nested, "category_count",
                    signals.category_count);
  write_count_field(out, context.nested, "source_location_gap_count",
                    signals.source_location_gap_count);
  write_count_field(out, context.nested, "affected_claim_count",
                    signals.affected_claim_count);
  write_count_field(out, context.nested, "exact_wiring_location_count",
                    signals.exact_wiring_location_count);
  write_count_field(out, context.nested, "current_repo_action_count",
                    signals.current_repo_action_count);
  write_count_field(out, context.nested, "current_server_action_count",
                    signals.current_server_action_count);
  write_count_field(out, context.nested, "target_machine_action_count",
                    signals.target_machine_action_count);
  write_count_field(out, context.nested, "target_product_action_count",
                    signals.target_product_action_count);
  write_bool_field(out, context.nested, "release_gate_consumed", true);
  write_json_field(
      out, context.nested, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query blocker drilldown as "
          "read-only operator triage metadata only; it does not execute "
          "repairs or create benchmark/replay/execute evidence"),
      false);
  out << context.pad << "}";
  return out.str();
}

std::string evidence_query_readiness_path_review_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto context = query_ux_view_context(
      projection, indent,
      "release pipeline Evidence Query readiness-path closure indent");
  const auto& signals = context.navigation.readiness_path;
  std::ostringstream out;
  out << context.pad << "{\n";
  write_query_ux_source_header(out, context, "readiness_path_drilldown_present",
                               context.objects.readiness_path_drilldown);
  write_json_field(out, context.nested, "target",
                   inferlab_plan::json_string(signals.target));
  write_json_field(out, context.nested, "state",
                   inferlab_plan::json_string(signals.state));
  write_count_field(out, context.nested, "blocking_repair_card_count",
                    signals.blocking_repair_card_count);
  write_count_field(out, context.nested, "path_step_count",
                    signals.step_count);
  write_count_field(out, context.nested, "current_repo_step_count",
                    signals.current_repo_step_count);
  write_count_field(out, context.nested,
                    "target_or_validation_step_count",
                    signals.target_or_validation_step_count);
  write_count_field(out, context.nested, "source_location_gap_count",
                    signals.source_location_gap_count);
  write_count_field(out, context.nested, "catalog_flow_discrepancy_count",
                    signals.catalog_flow_discrepancy_count);
  write_count_field(out, context.nested, "fallback_removal_blocked_count",
                    signals.fallback_removal_blocked_count);
  write_count_field(out, context.nested, "field_coverage_gap_count",
                    signals.field_coverage_gap_count);
  write_json_field(out, context.nested, "actionability_status",
                   inferlab_plan::json_string(signals.actionability_status));
  write_json_field(out, context.nested, "primary_environment",
                   inferlab_plan::json_string(signals.primary_environment));
  write_json_field(out, context.nested, "blocker_reason",
                   inferlab_plan::json_string(signals.blocker_reason));
  write_bool_field(out, context.nested, "current_repo_actionable",
                   signals.current_repo_actionable);
  write_bool_field(out, context.nested, "validation_or_target_required",
                   signals.validation_or_target_required);
  write_bool_field(out, context.nested, "source_location_metadata_required",
                   signals.source_location_metadata_required);
  write_bool_field(out, context.nested, "next_action_present",
                   signals.next_action_present);
  write_bool_field(out, context.nested, "release_gate_consumed", true);
  write_json_field(
      out, context.nested, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query readiness-path drilldown "
          "as read-only operator triage metadata only; it does not execute "
          "commands, refresh artifacts, or create benchmark/replay/execute "
          "evidence"),
      false);
  out << context.pad << "}";
  return out.str();
}

std::string evidence_query_source_location_coverage_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const auto context = query_ux_view_context(
      projection, indent,
      "release pipeline Evidence Query source-location closure indent");
  const auto& signals = context.navigation.source_location.category_coverage;
  std::ostringstream out;
  out << context.pad << "{\n";
  write_query_ux_source_header(
      out, context, "coverage_present",
      context.objects.source_location_category_coverage);
  write_count_field(out, context.nested, "category_count",
                    signals.category_count);
  write_count_field(out, context.nested, "active_category_count",
                    signals.active_category_count);
  write_count_field(out, context.nested, "source_exact_category_count",
                    signals.exact_category_count);
  write_count_field(out, context.nested, "source_gap_category_count",
                    signals.gap_category_count);
  write_count_field(out, context.nested,
                    "current_repo_source_gap_category_count",
                    signals.current_repo_gap_category_count);
  write_count_field(out, context.nested,
                    "target_or_validation_source_gap_category_count",
                    signals.target_or_validation_gap_category_count);
  write_count_field(out, context.nested, "source_gap_repair_card_count",
                    signals.gap_repair_card_count);
  write_count_field(out, context.nested, "source_exactness_percent",
                    signals.exactness_percent);
  write_bool_field(out, context.nested, "release_gate_consumed", true);
  write_json_field(
      out, context.nested, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query source-location category "
          "coverage as read-only mapping metadata only; it does not execute "
          "repairs or create benchmark/replay/execute evidence"),
      false);
  out << context.pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
