#include "release_evidence_pipeline_evidence_query_closure_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_repair_metrics.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_release_pipeline {
namespace {

using evidence_query_closure_view_detail::write_bool_field;
using evidence_query_closure_view_detail::write_count_field;
using evidence_query_closure_view_detail::write_json_field;
void write_cross_artifact_fields(
    std::ostringstream& out, const std::string& pad,
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const std::string& prefix, const std::string& status_key) {
  const auto& cross = projection.cross_artifact;
  const auto& families = cross.families;
  const auto& derived = cross.derived;
  const auto& paths = derived.repair_paths;
  const auto* first = paths.empty() ? nullptr : &paths.front();
  const auto* product = derived.product_replay_family_present
                            ? &derived.product_replay_family
                            : nullptr;
  const auto& product_sources = derived.product_replay_source_artifacts;

  write_json_field(
      out, pad, status_key,
      inferlab_plan::json_string(cross.status));
  write_count_field(out, pad, prefix + "family_count", families.size());
  write_count_field(out, pad, prefix + "active_family_count",
                    derived.active_family_count);
  write_count_field(out, pad, prefix + "total_blocker_count",
                    derived.total_blocker_count);
  write_count_field(out, pad, prefix + "current_repo_action_count",
                    derived.current_repo_action_count);
  write_count_field(
      out, pad, prefix + "target_or_validation_action_count",
      derived.target_or_validation_action_count);
  write_count_field(out, pad, prefix + "source_location_gap_count",
                    derived.source_location_gap_count);
  write_json_field(
      out, pad, prefix + "primary_family",
      inferlab_plan::json_string(derived.primary_family));
  write_json_field(
      out, pad, prefix + "primary_blocker_reason",
      inferlab_plan::json_string(derived.primary_blocker_reason));
  write_json_field(
      out, pad, prefix + "primary_environment",
      inferlab_plan::json_string(derived.primary_environment));
  write_bool_field(out, pad, prefix + "release_focus_present",
                   cross.release_focus_present);
  write_json_field(
      out, pad, prefix + "release_focus_decision",
      inferlab_plan::json_string(cross.release_focus_decision));
  write_json_field(
      out, pad, prefix + "release_focus_repair_lane",
      inferlab_plan::json_string(cross.release_focus_repair_lane));
  write_count_field(out, pad, prefix + "release_focus_affected_claim_count",
                    derived.affected_claims.size());
  write_count_field(out, pad, prefix + "release_focus_repair_path_count",
                    paths.size());
  write_json_field(
      out, pad, prefix + "release_focus_first_repair_family",
      inferlab_plan::json_string(first == nullptr ? "" : first->family));
  write_json_field(
      out, pad, prefix + "release_focus_first_repair_environment",
      inferlab_plan::json_string(first == nullptr
                                     ? ""
                                     : first->primary_environment));
  write_json_field(
      out, pad, prefix + "release_focus_first_repair_action",
      inferlab_plan::json_string(first == nullptr ? "" : first->next_action));
  write_json_field(
      out, pad, prefix + "release_focus_primary_action",
      inferlab_plan::json_string(derived.primary_action));
  write_bool_field(out, pad, prefix + "release_focus_current_repo_actionable",
                   derived.current_repo_actionable);
  write_bool_field(
      out, pad, prefix + "release_focus_target_or_validation_required",
      derived.target_or_validation_required);
  write_bool_field(
      out, pad, prefix + "release_focus_source_location_metadata_required",
      derived.source_location_metadata_required);
  write_bool_field(out, pad, prefix + "product_replay_family_present",
                   derived.product_replay_family_present);
  write_json_field(
      out, pad, prefix + "product_replay_family_status",
      inferlab_plan::json_string(derived.product_replay_family_status));
  write_count_field(out, pad, prefix + "product_replay_family_blocker_count",
                    product == nullptr ? 0U : product->blocker_count);
  write_count_field(
      out, pad,
      prefix + "product_replay_family_semantic_linkage_blocker_count",
      product == nullptr ? 0U : product->semantic_linkage_blocker_count);
  write_count_field(
      out, pad,
      prefix + "product_replay_family_replay_or_execute_missing_count",
      product == nullptr ? 0U : product->replay_or_execute_missing_count);
  write_count_field(
      out, pad,
      prefix + "product_replay_family_target_or_validation_action_count",
      product == nullptr ? 0U : product->target_or_validation_action_count);
  write_count_field(
      out, pad,
      prefix + "product_replay_family_target_product_replay_action_count",
      product == nullptr ? 0U : product->target_product_replay_action_count);
  write_count_field(
      out, pad,
      prefix + "product_replay_family_target_gpu_execute_action_count",
      product == nullptr ? 0U : product->target_gpu_execute_action_count);
  write_count_field(out, pad,
                    prefix + "product_replay_family_source_artifact_count",
                    product_sources.size());
  write_json_field(
      out, pad, prefix + "product_replay_family_first_source_artifact",
      inferlab_plan::json_string(product_sources.empty()
                                     ? ""
                                     : product_sources.front()));
  write_json_field(
      out, pad, prefix + "product_replay_family_next_action",
      inferlab_plan::json_string(product == nullptr ? ""
                                                    : product->next_action));
}

void write_product_replay_fields(
    std::ostringstream& out, const std::string& pad,
    const inferlab_evidence::ProductReplaySemanticLinkageSignals& signals,
    const std::string& source_prefix, const std::string& metric_prefix) {
  write_json_field(out, pad, metric_prefix + "status",
                   inferlab_plan::json_string(signals.status));
  write_bool_field(out, pad, source_prefix + "semantic_events_present",
                   signals.semantic_events_present);
  write_bool_field(out, pad, source_prefix + "alignment_present",
                   signals.alignment_present);
  write_count_field(out, pad, metric_prefix + "read_issue_count",
                    signals.read_issue_count);
  write_count_field(out, pad, metric_prefix + "available_event_count",
                    signals.available_event_count);
  write_count_field(out, pad, metric_prefix + "pending_event_count",
                    signals.pending_event_count);
  write_count_field(out, pad, metric_prefix + "sample_event_count",
                    signals.sample_event_count);
  write_bool_field(out, pad, metric_prefix + "sample_replay_present",
                   signals.sample_replay_present);
  write_count_field(out, pad, metric_prefix + "replay_supported_event_count",
                    signals.replay_supported_event_count);
  write_count_field(out, pad, metric_prefix + "alignment_rule_count",
                    signals.alignment_rule_count);
  write_count_field(out, pad, metric_prefix + "aligned_rule_count",
                    signals.aligned_rule_count);
  write_count_field(out, pad, metric_prefix + "pending_rule_count",
                    signals.pending_rule_count);
  write_count_field(out, pad, metric_prefix + "correlation_count",
                    signals.correlation_count);
  write_count_field(out, pad, metric_prefix + "ready_correlation_count",
                    signals.ready_correlation_count);
  write_count_field(out, pad, metric_prefix + "blocked_correlation_count",
                    signals.blocked_correlation_count);
  write_count_field(out, pad, metric_prefix + "blocker_count",
                    signals.blocker_count);
  write_count_field(out, pad, metric_prefix + "target_or_validation_action_count",
                    signals.target_or_validation_action_count);
  write_json_field(out, pad, metric_prefix + "first_blocker",
                   inferlab_plan::json_string(signals.first_blocker));
  write_json_field(out, pad, metric_prefix + "next_action",
                   inferlab_plan::json_string(signals.next_action));
}

}  // namespace

std::string evidence_query_cross_artifact_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline cross-artifact field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad,
                   "cross_artifact_blocker_explanation_present",
                   projection.cross_artifact.object_present);
  write_count_field(out, field_pad, "contract_issue_count",
                    projection.cross_artifact.contract_issue_count);
  write_cross_artifact_fields(out, field_pad, projection, "", "status");
  write_bool_field(out, field_pad, "release_gate_consumed", true);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query cross-artifact blocker "
          "explanation as read-only evidence-correlation metadata only; it "
          "does not execute repairs, refresh artifacts, replay products, or "
          "create benchmark/execute evidence"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_product_replay_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const std::vector<NonGpuReviewRepairPath>& repair_paths,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline Product Replay field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& product = projection.product_replay;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad, "linkage_present", product.object_present);
  write_count_field(out, field_pad, "contract_issue_count",
                    product.contract_issue_count);
  write_product_replay_fields(out, field_pad, product.signals, "", "");
  write_bool_field(out, field_pad, "release_gate_consumed", true);
  write_count_field(
      out, field_pad, "release_repair_path_count",
      count_non_gpu_repair_category_from_source(
          repair_paths, "product_replay_semantic_linkage",
          projection.artifact.artifact));
  write_count_field(
      out, field_pad, "release_repair_path_source_location_gap_count",
      count_non_gpu_repair_category_source_location_gaps_from_source(
          repair_paths, "product_replay_semantic_linkage",
          projection.artifact.artifact));
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "release pipeline consumes Evidence Query product replay semantic "
          "linkage as read-only replay explanation metadata only; it does "
          "not replay products, execute products, refresh artifacts, or "
          "create benchmark evidence"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_cross_artifact_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  write_cross_artifact_fields(
      out, pad, projection, "evidence_query_cross_artifact_",
      "evidence_query_cross_artifact_blocker_explanation_status");
  return out.str();
}

std::string evidence_query_product_replay_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const auto& product = projection.product_replay;
  std::ostringstream out;
  write_bool_field(
      out, pad, "evidence_query_product_replay_semantic_linkage_present",
      product.object_present);
  write_product_replay_fields(
      out, pad, product.signals, "evidence_query_product_replay_",
      "evidence_query_product_replay_semantic_linkage_");
  return out.str();
}

}  // namespace inferlab_release_pipeline
