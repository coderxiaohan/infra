#pragma once

#include "evidence_query_artifact_projection.hpp"
#include "evidence_query_repair_policy.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>
#include <vector>

namespace inferlab_evidence_query {

inline std::string record_result_json(
    const inferlab_artifact::WarehouseArtifactRecord& record,
    const std::string& query,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::string& category = blocker_category(record);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(record.artifact) << ",\n";
  out << nested << "\"query\": " << inferlab_plan::json_string(query)
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(result_status(record, query)) << ",\n";
  out << nested << "\"blocker_category\": "
      << inferlab_plan::json_string(category) << ",\n";
  out << nested << "\"repair_environment\": "
      << inferlab_plan::json_string(repair_environment(record)) << ",\n";
  out << nested << "\"repair_action\": "
      << inferlab_plan::json_string(repair_action(category)) << ",\n";
  out << nested << "\"evidence_confidence\": "
      << inferlab_plan::json_string(evidence_confidence(record)) << ",\n";
  out << nested << "\"present\": " << (record.present ? "true" : "false")
      << ",\n";
  out << nested << "\"matrix_present\": "
      << (record.matrix_present ? "true" : "false") << ",\n";
  out << nested << "\"summary_present\": "
      << (record.summary_present ? "true" : "false") << ",\n";
  out << nested << "\"prometheus_present\": "
      << (record.prometheus_present ? "true" : "false") << ",\n";
  out << nested << "\"report_present\": "
      << (record.report_present ? "true" : "false") << ",\n";
  out << nested << "\"final_report_present\": "
      << (record.final_report_present ? "true" : "false") << ",\n";
  out << nested << "\"contract_present\": "
      << (record.contract_present ? "true" : "false") << ",\n";
  out << nested << "\"freshness_present\": "
      << (record.freshness_present ? "true" : "false") << ",\n";
  out << nested << "\"metadata\": "
      << inferlab_artifact::metadata_json(record.metadata, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string results_json(const QueryArtifactProjection& projection,
                                const std::string& query,
                                const int indent) {
  const std::string pad(indent, ' ');
  bool first = true;
  std::ostringstream out;
  out << "[";
  for (const auto* record : projection.matched_records) {
    if (!first) out << ",";
    first = false;
    out << "\n" << record_result_json(*record, query, indent + 2);
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string artifact_list_json(
    const QueryArtifactProjection& projection,
    const std::string& query,
    const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  bool first = true;
  std::ostringstream out;
  out << "[";
  for (const auto* record : projection.matched_records) {
    if (!first) out << ",";
    first = false;
    out << "\n" << nested << "{\n";
    out << nested << "  \"artifact\": "
        << inferlab_plan::json_string(record->artifact) << ",\n";
    out << nested << "  \"status\": "
        << inferlab_plan::json_string(result_status(*record, query))
        << ",\n";
    out << nested << "  \"blocker_category\": "
        << inferlab_plan::json_string(blocker_category(*record)) << ",\n";
    out << nested << "  \"evidence_confidence\": "
        << inferlab_plan::json_string(evidence_confidence(*record))
        << ",\n";
    out << nested << "  \"present\": "
        << (record->present ? "true" : "false") << ",\n";
    out << nested << "  \"summary_present\": "
        << (record->summary_present ? "true" : "false") << ",\n";
    out << nested << "  \"final_report_present\": "
        << (record->final_report_present ? "true" : "false") << "\n";
    out << nested << "}";
  }
  if (!first) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string show_artifact_json(
    const ShowArtifactDetailProjection& detail,
    const int indent) {
  const auto& selection = detail.selection;
  const auto* selected = selection.selected;
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  const std::vector<std::string> empty;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"selector\": "
      << inferlab_plan::json_string(selection.selector) << ",\n";
  out << nested << "\"matched\": "
      << (selected != nullptr ? "true" : "false") << ",\n";
  out << nested << "\"candidate_count\": " << selection.candidate_count
      << ",\n";
  out << nested << "\"candidate_artifacts\": "
      << inferlab_plan::json_string_array(selection.candidate_artifacts,
                                          indent + 2)
      << ",\n";
  out << nested << "\"candidate_list_truncated\": "
      << (selection.candidate_list_truncated ? "true" : "false") << ",\n";
  out << nested << "\"exact_match\": "
      << (selection.exact_match ? "true" : "false") << ",\n";
  out << nested << "\"selector_ambiguous\": "
      << (selection.selector_ambiguous ? "true" : "false") << ",\n";
  out << nested << "\"selection_strategy\": "
      << inferlab_plan::json_string(selection.selection_strategy) << ",\n";
  out << nested << "\"selected_artifact\": "
      << inferlab_plan::json_string(selected == nullptr ? ""
                                                        : selected->artifact);
  if (selected != nullptr) {
    const auto& exact_locations =
        detail.source_exact ? detail.wiring_locations : empty;
    const auto& fallback_locations =
        detail.source_exact ? empty : detail.wiring_locations;
    out << ",\n";
    out << nested << "\"artifact\": "
        << inferlab_plan::json_string(selected->artifact) << ",\n";
    out << nested << "\"status\": "
        << inferlab_plan::json_string(detail.status) << ",\n";
    out << nested << "\"blocker_category\": "
        << inferlab_plan::json_string(detail.category) << ",\n";
    out << nested << "\"repair_environment\": "
        << inferlab_plan::json_string(detail.repair_environment) << ",\n";
    out << nested << "\"repair_action\": "
        << inferlab_plan::json_string(repair_action(detail.category))
        << ",\n";
    out << nested << "\"affected_claims\": "
        << inferlab_plan::json_string_array(detail.affected_claims,
                                            indent + 2)
        << ",\n";
    out << nested << "\"evidence_confidence\": "
        << inferlab_plan::json_string(detail.confidence) << ",\n";
    out << nested << "\"source_resolution_status\": "
        << inferlab_plan::json_string(
               detail.category == "none"
                   ? "not_blocked"
                   : (detail.source_exact
                          ? "exact_source_location_available"
                          : "source_location_mapping_missing"))
        << ",\n";
    out << nested << "\"source_exact\": "
        << (detail.source_exact ? "true" : "false") << ",\n";
    out << nested << "\"source_location_count\": "
        << exact_locations.size() << ",\n";
    out << nested << "\"source_location_gap_count\": "
        << detail.source_location_gap_count << ",\n";
    out << nested << "\"wiring_locations\": "
        << inferlab_plan::json_string_array(detail.wiring_locations,
                                            indent + 2)
        << ",\n";
    out << nested << "\"exact_wiring_locations\": "
        << inferlab_plan::json_string_array(exact_locations, indent + 2)
        << ",\n";
    out << nested << "\"fallback_investigation_locations\": "
        << inferlab_plan::json_string_array(fallback_locations, indent + 2)
        << ",\n";
    out << nested << "\"current_repo_action_required\": "
        << (detail.repair_environment_kind ==
                    inferlab_evidence::RepairEnvironment::kCurrentRepoFixable
                ? "true"
                : "false")
        << ",\n";
    out << nested << "\"current_server_validation_required\": "
        << (detail.repair_environment_kind ==
                    inferlab_evidence::RepairEnvironment::
                        kCurrentServerWhenValidationEnabled
                ? "true"
                : "false")
        << ",\n";
    out << nested << "\"target_machine_action_required\": "
        << (detail.repair_environment_kind ==
                    inferlab_evidence::RepairEnvironment::
                        kTargetGpuOrExecuteMachine
                ? "true"
                : "false")
        << ",\n";
    out << nested << "\"target_product_action_required\": "
        << (detail.repair_environment_kind ==
                    inferlab_evidence::RepairEnvironment::
                        kTargetProductOrReplayEvidence
                ? "true"
                : "false")
        << ",\n";
    out << nested << "\"next_actions\": "
        << inferlab_plan::json_string_array(
               {selection.selector_ambiguous
                    ? "rerun show artifact with an exact artifact id from candidate_artifacts before treating this as a focused view"
                    : repair_action(detail.category),
                selection.exact_match
                    ? "exact artifact id matched; inspect selected artifact detail"
                    : "selector is broad; use exact artifact id when candidate_count is greater than one",
                repair_action(detail.category),
                detail.source_exact
                    ? "inspect exact_wiring_locations before changing code or report wiring"
                    : (detail.category == "none"
                           ? "no blocker-specific source repair is required"
                           : "add exact source/config/report wiring for this blocker category before treating the repair card as handoff-ready"),
                "refresh the owning artifact producer only when validation is enabled"},
               indent + 2)
        << ",\n";
    out << nested
        << "\"claim_boundary\": \"show_artifact detail is read-only triage metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\""
        << ",\n";
    out << nested << "\"metadata\": "
        << inferlab_artifact::metadata_json(selected->metadata, indent + 2);
  } else {
    out << ",\n";
    out << nested << "\"next_actions\": "
        << inferlab_plan::json_string_array(
               {"use list artifacts to inspect available artifact ids",
                "rerun show artifact with an exact artifact id or narrower fragment",
                "refresh evidence warehouse only when validation is enabled"},
               indent + 2)
        << ",\n";
    out << nested
        << "\"claim_boundary\": \"show_artifact detail is read-only triage metadata; it does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence\"";
  }
  out << "\n" << pad << "}";
  return out.str();
}

}  // namespace inferlab_evidence_query
