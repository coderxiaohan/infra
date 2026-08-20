#include "release_evidence_pipeline_evidence_query_closure_projection.hpp"

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>

namespace inferlab_release_pipeline {
namespace {

template <typename Field>
bool required_field_invalid(const Field& field) {
  return !field.present || field.invalid;
}

template <typename Field>
void count_required_field_issue(const Field& field, std::size_t& issue_count,
                                const char* label) {
  if (!required_field_invalid(field)) return;
  issue_count = inferlab::internal::checked_add_size(issue_count, 1U, label);
}

void count_required_field_issues(
    const std::initializer_list<bool> invalid_fields,
    std::size_t& issue_count, const char* label) {
  for (const bool invalid : invalid_fields) {
    if (!invalid) continue;
    issue_count =
        inferlab::internal::checked_add_size(issue_count, 1U, label);
  }
}

bool object_section_present(const std::string& document, const char* key) {
  return !inferlab_artifact::json_object_section_or_empty(document, key)
              .empty();
}

EvidenceQueryOperationCatalogClosureProjection project_operation_catalog(
    const std::string& document, const std::string& summary,
    const bool summary_usable, const bool source_present) {
  EvidenceQueryOperationCatalogClosureProjection projection;
  projection.object_present =
      summary_usable && object_section_present(document, "operation_catalog");
  if (!summary_usable) {
    projection.contract_issue_count = source_present ? 1U : 0U;
    return projection;
  }

  projection.supported_count = inferlab_artifact::project_unsigned_field(
      summary, {"operation_catalog_supported_count"});
  projection.read_only_count = inferlab_artifact::project_unsigned_field(
      summary, {"operation_catalog_read_only_count"});
  projection.snapshot_required_count =
      inferlab_artifact::project_unsigned_field(
          summary, {"operation_catalog_snapshot_required_count"});
  projection.selector_recommended_count =
      inferlab_artifact::project_unsigned_field(
          summary, {"operation_catalog_selector_recommended_count"});
  projection.target_required_count =
      inferlab_artifact::project_unsigned_field(
          summary, {"operation_catalog_target_required_count"});

  if (source_present && !projection.object_present) {
    projection.contract_issue_count = 1U;
  }
  count_required_field_issue(
      projection.supported_count, projection.contract_issue_count,
      "release pipeline operation catalog supported-count issue");
  count_required_field_issue(
      projection.read_only_count, projection.contract_issue_count,
      "release pipeline operation catalog read-only-count issue");
  count_required_field_issue(
      projection.snapshot_required_count, projection.contract_issue_count,
      "release pipeline operation catalog snapshot-required issue");
  count_required_field_issue(
      projection.selector_recommended_count, projection.contract_issue_count,
      "release pipeline operation catalog selector-recommended issue");
  count_required_field_issue(
      projection.target_required_count, projection.contract_issue_count,
      "release pipeline operation catalog target-required issue");

  projection.read_only_complete =
      !required_field_invalid(projection.supported_count) &&
      !required_field_invalid(projection.read_only_count) &&
      projection.supported_count.value > 0U &&
      projection.supported_count.value == projection.read_only_count.value;
  return projection;
}

EvidenceQuerySelectedOperationClosureProjection project_selected_operation(
    const std::string& document, const std::string& summary,
    const bool summary_usable, const bool source_present) {
  EvidenceQuerySelectedOperationClosureProjection projection;
  projection.object_present =
      summary_usable &&
      object_section_present(document, "selected_operation_result");
  if (!summary_usable) {
    projection.contract_issue_count = source_present ? 1U : 0U;
    return projection;
  }

  projection.result_present = inferlab_artifact::project_bool_field(
      summary, {"query_operation_selected_result_present"});
  projection.input_ready = inferlab_artifact::project_bool_field(
      summary, {"query_operation_selected_result_input_ready"});
  projection.payload_count = inferlab_artifact::project_unsigned_field(
      summary, {"query_operation_selected_result_payload_count"});
  projection.result_kind = inferlab_artifact::project_string_field(
      summary, {"query_operation_selected_result_kind"});
  projection.show_artifact_candidate_count =
      inferlab_artifact::project_unsigned_field(
          summary, {"query_operation_show_artifact_candidate_count"});
  projection.show_artifact_candidate_truncated =
      inferlab_artifact::project_bool_field(
          summary, {"query_operation_show_artifact_candidate_truncated"});
  projection.show_artifact_exact_match =
      inferlab_artifact::project_bool_field(
          summary, {"query_operation_show_artifact_exact_match"});
  projection.show_artifact_selector_ambiguous =
      inferlab_artifact::project_bool_field(
          summary, {"query_operation_show_artifact_selector_ambiguous"});
  projection.show_artifact_selection_strategy =
      inferlab_artifact::project_string_field(
          summary, {"query_operation_show_artifact_selection_strategy"});
  projection.payload_selector_focus_present =
      inferlab_artifact::project_bool_field(
          summary, {"query_operation_payload_selector_focus_present"});
  projection.payload_selector_focus_status =
      inferlab_artifact::project_string_field(
          summary, {"query_operation_payload_selector_focus_status"});
  projection.payload_selector_focus_match_count =
      inferlab_artifact::project_unsigned_field(
          summary, {"query_operation_payload_selector_focus_match_count"});
  projection.payload_selector_focus_ambiguous =
      inferlab_artifact::project_bool_field(
          summary, {"query_operation_payload_selector_focus_ambiguous"});
  projection.payload_selector_focus_source_location_gap_count =
      inferlab_artifact::project_unsigned_field(
          summary,
          {"query_operation_payload_selector_focus_source_location_gap_count"});

  if (source_present && !projection.object_present) {
    projection.contract_issue_count = 1U;
  }
  count_required_field_issues(
      {required_field_invalid(projection.result_present),
       required_field_invalid(projection.input_ready),
       required_field_invalid(projection.payload_count),
       required_field_invalid(projection.result_kind),
       required_field_invalid(projection.show_artifact_candidate_count),
       required_field_invalid(projection.show_artifact_candidate_truncated),
       required_field_invalid(projection.show_artifact_exact_match),
       required_field_invalid(projection.show_artifact_selector_ambiguous),
       required_field_invalid(projection.show_artifact_selection_strategy),
       required_field_invalid(projection.payload_selector_focus_present),
       required_field_invalid(projection.payload_selector_focus_status),
       required_field_invalid(projection.payload_selector_focus_match_count),
       required_field_invalid(projection.payload_selector_focus_ambiguous),
       required_field_invalid(
           projection.payload_selector_focus_source_location_gap_count)},
      projection.contract_issue_count,
      "release pipeline selected-operation field issue");
  return projection;
}

}  // namespace

ReleasePipelineEvidenceQueryClosureProjection::
    ReleasePipelineEvidenceQueryClosureProjection(
        const ReleasePipelineSourceContext& sources)
    : artifact(inferlab_high_value::require_managed_artifact(
          "evidence_query_result")),
      snapshot_diff(sources.snapshot_diff),
      selector_resolution(sources.navigation.selector.resolution),
      selector_repair_card_export(
          sources.navigation.selector.repair_card_export),
      selector_action_plan(sources.navigation.selector.action_plan),
      source_present(sources.evidence_query->metadata.present),
      source_readable(sources.evidence_query->read_result.readable),
      source_read_status(sources.evidence_query->read_result.status),
      summary_usable(sources.evidence_query_summary_projection.usable),
      query_ux(sources.evidence_query_document, summary_usable,
               source_present, sources.navigation),
      catalog_flow(project_evidence_query_catalog_flow_closure(
          sources.evidence_query_document, summary_usable, source_present)),
      cross_artifact(project_evidence_query_cross_artifact_closure(
          sources.evidence_query_document, summary_usable, source_present)),
      product_replay(project_evidence_query_product_replay_closure(
          sources.evidence_query_document, summary_usable, source_present)),
      operation_catalog(project_operation_catalog(
          sources.evidence_query_document, sources.evidence_query_summary,
          summary_usable, source_present)),
      selected_operation(project_selected_operation(
          sources.evidence_query_document, sources.evidence_query_summary,
          summary_usable, source_present)) {
  source_contract_issue_count = inferlab::internal::checked_sum_size(
      {query_ux.contract_issue_count,
       catalog_flow.contract_issue_count,
       cross_artifact.contract_issue_count,
       product_replay.contract_issue_count,
       operation_catalog.contract_issue_count,
       selected_operation.contract_issue_count},
      "release pipeline evidence query closure contract issue count");
}

}  // namespace inferlab_release_pipeline
