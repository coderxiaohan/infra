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

void write_consumer_field_coverage(
    std::ostringstream& out, const std::string& pad,
    const inferlab_evidence::ConsumerFieldCoverageSignals& signals,
    const std::string& prefix) {
  write_count_field(out, pad, prefix + "surface_count",
                    signals.consumer_field_coverage_surface_count);
  write_count_field(
      out, pad, prefix + "catalog_managed_surface_count",
      signals.consumer_field_coverage_catalog_managed_surface_count);
  write_count_field(out, pad, prefix + "fallback_surface_count",
                    signals.consumer_field_coverage_fallback_surface_count);
  write_count_field(out, pad, prefix + "required_field_count",
                    signals.consumer_field_coverage_required_field_count);
  write_count_field(out, pad, prefix + "covered_field_count",
                    signals.consumer_field_coverage_covered_field_count);
  write_count_field(out, pad, prefix + "field_gap_count",
                    signals.consumer_field_coverage_field_gap_count);
  write_count_field(
      out, pad, prefix + "parity_refresh_required_count",
      signals.consumer_field_coverage_parity_refresh_required_count);
  write_count_field(
      out, pad, prefix + "current_repo_action_count",
      signals.consumer_field_coverage_current_repo_action_count);
  write_count_field(
      out, pad, prefix + "target_machine_action_count",
      signals.consumer_field_coverage_target_machine_action_count);
  write_count_field(out, pad, prefix + "repair_card_count",
                    signals.consumer_field_coverage_repair_card_count);
  write_count_field(
      out, pad, prefix + "current_repo_repair_card_count",
      signals.consumer_field_coverage_current_repo_repair_card_count);
  write_count_field(
      out, pad, prefix + "target_machine_repair_card_count",
      signals.consumer_field_coverage_target_machine_repair_card_count);
}

void write_fallback_removal(
    std::ostringstream& out, const std::string& pad,
    const inferlab_evidence::CatalogFallbackRemovalSignals& signals,
    const std::string& prefix, const bool include_derived_counts) {
  const std::string shortest = prefix + "fallback_removal_shortest_path_";
  write_bool_field(out, pad, shortest + "present",
                   signals.fallback_removal_shortest_path_present);
  write_json_field(
      out, pad, shortest + "state",
      inferlab_plan::json_string(
          signals.fallback_removal_shortest_path_state));
  write_count_field(out, pad, shortest + "step_count",
                    signals.fallback_removal_shortest_path_step_count);
  write_count_field(
      out, pad, shortest + "current_repo_step_count",
      signals.fallback_removal_shortest_path_current_repo_step_count);
  write_count_field(
      out, pad, shortest + "target_machine_step_count",
      signals.fallback_removal_shortest_path_target_machine_step_count);
  write_count_field(
      out, pad, shortest + "source_location_gap_count",
      signals.fallback_removal_shortest_path_source_location_gap_count);
  write_json_field(
      out, pad, shortest + "primary_environment",
      inferlab_plan::json_string(
          signals.fallback_removal_shortest_path_primary_environment));
  write_json_field(
      out, pad, shortest + "first_surface",
      inferlab_plan::json_string(
          signals.fallback_removal_shortest_path_first_surface));
  write_bool_field(out, pad, shortest + "blocks_removal",
                   signals.fallback_removal_shortest_path_blocks_removal);

  const std::string parity = prefix + "fallback_removal_parity_gate_";
  write_bool_field(out, pad, parity + "present",
                   signals.fallback_removal_parity_gate_present);
  write_json_field(
      out, pad, parity + "decision",
      inferlab_plan::json_string(
          signals.fallback_removal_parity_gate_decision));
  write_bool_field(out, pad, parity + "removal_allowed",
                   signals.fallback_removal_parity_gate_removal_allowed);
  write_count_field(out, pad, parity + "blocker_count",
                    signals.fallback_removal_parity_gate_blocker_count);
  if (include_derived_counts) {
    write_count_field(
        out, pad, parity + "blocking_surface_count",
        signals.fallback_removal_parity_gate_blocking_surface_count);
  }
  write_count_field(
      out, pad, parity + "fallback_surface_count",
      signals.fallback_removal_parity_gate_fallback_surface_count);
  write_count_field(
      out, pad, parity + "parity_refresh_required_count",
      signals.fallback_removal_parity_gate_parity_refresh_required_count);
  write_count_field(
      out, pad, parity + "current_repo_action_count",
      signals.fallback_removal_parity_gate_current_repo_action_count);
  write_count_field(
      out, pad, parity + "target_machine_action_count",
      signals.fallback_removal_parity_gate_target_machine_action_count);
  write_count_field(
      out, pad, parity + "source_location_gap_count",
      signals.fallback_removal_parity_gate_source_location_gap_count);
  if (include_derived_counts) {
    write_count_field(
        out, pad,
        parity + "blocking_surface_source_location_gap_count",
        signals
            .fallback_removal_parity_gate_blocking_surface_source_location_gap_count);
  }
  write_json_field(
      out, pad, parity + "first_surface",
      inferlab_plan::json_string(
          signals.fallback_removal_parity_gate_first_surface));
  write_bool_field(out, pad, parity + "blocks_removal",
                   signals.fallback_removal_parity_gate_blocks_removal);
}

}  // namespace

std::string evidence_query_catalog_flow_filter_closure_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const int field_indent = inferlab::internal::checked_add_int(
      indent, 2, "release pipeline catalog-flow field indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string field_pad = inferlab_plan::json_indent(field_indent);
  const auto& catalog_flow = projection.catalog_flow;
  std::ostringstream out;
  out << "{\n";
  write_json_field(out, field_pad, "source",
                   inferlab_plan::json_string(projection.artifact.artifact));
  write_bool_field(out, field_pad, "direct_artifact_present",
                   projection.source_present);
  write_bool_field(out, field_pad, "filter_query_present",
                   catalog_flow.object_present);
  write_count_field(out, field_pad, "contract_issue_count",
                    catalog_flow.contract_issue_count);
  write_consumer_field_coverage(
      out, field_pad, catalog_flow.consumer_field_coverage,
      "catalog_flow_consumer_field_coverage_");
  write_fallback_removal(out, field_pad, catalog_flow.fallback_removal,
                         "catalog_flow_", false);
  write_json_field(
      out, field_pad, "claim_boundary",
      inferlab_plan::json_string(
          "Evidence Query catalog-flow field coverage is release-review "
          "metadata only; it does not refresh catalog consumers, remove "
          "fallback lists, run benchmarks, replay products, or prove execute "
          "readiness"),
      false);
  out << pad << "}";
  return out.str();
}

std::string evidence_query_catalog_flow_summary_fields_json(
    const ReleasePipelineEvidenceQueryClosureProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const auto& catalog_flow = projection.catalog_flow;
  std::ostringstream out;
  write_bool_field(out, pad, "evidence_query_catalog_flow_filter_present",
                   catalog_flow.object_present);
  write_count_field(
      out, pad, "evidence_query_catalog_flow_contract_issue_count",
      catalog_flow.contract_issue_count);
  write_consumer_field_coverage(
      out, pad, catalog_flow.consumer_field_coverage,
      "evidence_query_catalog_flow_consumer_field_coverage_");
  write_fallback_removal(out, pad, catalog_flow.fallback_removal,
                         "evidence_query_catalog_flow_", true);
  return out.str();
}

}  // namespace inferlab_release_pipeline
