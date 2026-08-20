#include "release_evidence_pipeline_decision_view.hpp"

#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_release_pipeline {

std::string release_pipeline_decision_summary_fields_json(
    const ReleasePipelineDecisionProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "\"release_decision\": "
      << inferlab_plan::json_string(projection.decision_name) << ",\n";
  out << pad << "\"release_decision_reason\": "
      << inferlab_plan::json_string(projection.decision_reason) << ",\n";
  out << pad << "\"stage_count\": " << projection.stage_count << ",\n";
  out << pad << "\"current_decision_stage_count\": "
      << projection.current_decision_stage_count << ",\n";
  out << pad << "\"post_generation_stage_count\": "
      << projection.post_generation_stage_count << ",\n";
  out << pad << "\"catalog_managed_stage_count\": "
      << projection.catalog_managed_stage_count << ",\n";
  out << pad << "\"fallback_retained_stage_count\": "
      << projection.fallback_retained_stage_count << ",\n";
  out << pad << "\"stage_identity_drift_status\": "
      << inferlab_plan::json_string(projection.stage_identity_drift_status)
      << ",\n";
  out << pad << "\"required_stage_count\": "
      << projection.required_stage_count << ",\n";
  out << pad << "\"ready_required_stage_count\": "
      << projection.ready_required_stage_count << ",\n";
  out << pad << "\"blocking_stage_count\": "
      << projection.blocking_stage_count << ",\n";
  out << pad << "\"replayed_input_count\": "
      << projection.replayed_input_count << ",\n";
  out << pad << "\"executed_input_count\": "
      << projection.executed_input_count << ",\n";
  out << pad << "\"execute_verified_input_count\": "
      << projection.execute_verified_input_count << ",\n";
  out << pad << "\"replay_evidence_status\": "
      << inferlab_plan::json_string(projection.replay_evidence_status)
      << ",\n";
  out << pad << "\"replay_evidence_ready\": "
      << inferlab_plan::json_bool(projection.replay_evidence_ready) << ",\n";
  out << pad << "\"replay_evidence_blocker_count\": "
      << projection.replay_evidence_blocker_count << ",\n";
  out << pad << "\"replay_evidence_read_issue_count\": "
      << projection.replay_evidence_read_issue_count << ",\n";
  out << pad << "\"replay_evidence_available_event_count\": "
      << projection.replay_evidence_available_event_count << ",\n";
  out << pad << "\"replay_evidence_replay_supported_event_count\": "
      << projection.replay_evidence_replay_supported_event_count << ",\n";
  out << pad << "\"replay_evidence_alignment_rule_count\": "
      << projection.replay_evidence_alignment_rule_count << ",\n";
  out << pad << "\"replay_evidence_fully_aligned_rule_count\": "
      << projection.replay_evidence_fully_aligned_rule_count << ",\n";
  out << pad << "\"replay_evidence_alignment_decision\": "
      << inferlab_plan::json_string(
             projection.replay_evidence_alignment_decision)
      << ",\n";
  out << pad << "\"release_alignment_dimension_count\": "
      << projection.release_alignment_dimension_count << ",\n";
  out << pad << "\"release_alignment_ready_dimension_count\": "
      << projection.release_alignment_ready_dimension_count << ",\n";
  out << pad << "\"release_alignment_blocking_dimension_count\": "
      << projection.release_alignment_blocking_dimension_count << ",\n";
  out << pad << "\"release_alignment_advisory_dimension_count\": "
      << projection.release_alignment_advisory_dimension_count << ",\n";
  out << pad << "\"release_alignment_issue_count\": "
      << projection.release_alignment_issue_count << ",\n";
  out << pad << "\"repository_dag_acyclic\": "
      << inferlab_plan::json_bool(projection.repository_dag_acyclic)
      << ",\n";
  out << pad << "\"repository_dag_scope_complete\": "
      << inferlab_plan::json_bool(projection.repository_dag_scope_complete)
      << ",\n";
  out << pad << "\"repository_dag_issue_count\": "
      << projection.repository_dag_issue_count << ",\n";
  out << pad << "\"source_contract_gate_applied\": "
      << inferlab_plan::json_bool(projection.source_contract_gate_applied)
      << ",\n";
  out << pad << "\"source_contract_gate_blocks_release\": "
      << inferlab_plan::json_bool(
             projection.source_contract_gate_blocks_release)
      << ",\n";
  out << pad << "\"source_contract_issue_count\": "
      << projection.source_contract_issue_count << ",\n";
  out << pad << "\"source_contract_blocking_source_count\": "
      << projection.source_contract_blocking_source_count << ",\n";
  out << pad << "\"release_blocking_signal_count\": "
      << projection.release_blocking_signal_count << ",\n";
  out << pad << "\"evidence_query_source_contract_issue_count\": "
      << projection.evidence_query_source_contract_issue_count << ",\n";
  out << pad << "\"blocker_repair_link_contract_issue_count\": "
      << projection.blocker_link_contract_issue_count << ",\n";
  out << pad << "\"post_generation_observation_issue_count\": "
      << projection.post_generation_observation_issue_count << ",\n";
  return out.str();
}

std::string release_pipeline_replay_evidence_dimension_json(
    const ReleasePipelineDecisionProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(projection.replay_evidence_status)
      << ",\n";
  out << nested << "\"replay_evidence_ready\": "
      << inferlab_plan::json_bool(projection.replay_evidence_ready) << ",\n";
  out << nested << "\"blocks_replay_ready\": "
      << inferlab_plan::json_bool(
             projection.replay_dimension_blocks_replay_ready)
      << ",\n";
  out << nested << "\"semantic_events_present\": "
      << inferlab_plan::json_bool(
             projection.replay_semantic_events_present)
      << ",\n";
  out << nested << "\"alignment_present\": "
      << inferlab_plan::json_bool(projection.replay_alignment_present)
      << ",\n";
  out << nested << "\"sample_replay_present\": "
      << inferlab_plan::json_bool(projection.replay_sample_present)
      << ",\n";
  out << nested << "\"available_event_count\": "
      << projection.replay_evidence_available_event_count << ",\n";
  out << nested << "\"replay_supported_event_count\": "
      << projection.replay_evidence_replay_supported_event_count << ",\n";
  out << nested << "\"alignment_rule_count\": "
      << projection.replay_evidence_alignment_rule_count << ",\n";
  out << nested << "\"fully_aligned_rule_count\": "
      << projection.replay_evidence_fully_aligned_rule_count << ",\n";
  out << nested << "\"alignment_decision\": "
      << inferlab_plan::json_string(
             projection.replay_evidence_alignment_decision)
      << ",\n";
  out << nested << "\"read_issue_count\": "
      << projection.replay_evidence_read_issue_count << ",\n";
  out << nested << "\"blocker_count\": "
      << projection.replay_evidence_blocker_count << ",\n";
  out << nested << "\"next_action\": "
      << inferlab_plan::json_string(projection.replay_evidence_next_action)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"existing replay semantic-event and alignment evidence only; this dimension does not import logs, run replay workloads, execute products, or prove performance\"\n";
  out << pad << "}";
  return out.str();
}

std::string release_pipeline_source_contract_gate_json(
    const ReleasePipelineDecisionProjection& projection,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const std::string item = inferlab_plan::json_indent(indent + 4);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"gate_applied\": "
      << inferlab_plan::json_bool(projection.source_contract_gate_applied)
      << ",\n";
  out << nested << "\"blocks_release\": "
      << inferlab_plan::json_bool(
             projection.source_contract_gate_blocks_release)
      << ",\n";
  out << nested << "\"issue_count\": "
      << projection.source_contract_issue_count << ",\n";
  out << nested << "\"blocking_source_count\": "
      << projection.source_contract_blocking_source_count << ",\n";
  out << nested << "\"sources\": [";
  for (std::size_t index = 0; index < projection.contract_sources.size();
       ++index) {
    const auto& source = projection.contract_sources[index];
    out << "\n" << item << "{\n";
    out << item << "  \"id\": " << inferlab_plan::json_string(source.id)
        << ",\n";
    out << item << "  \"canonical_owner\": "
        << inferlab_plan::json_string(source.canonical_owner) << ",\n";
    out << item << "  \"issue_count\": " << source.issue_count << ",\n";
    out << item << "  \"blocks_release\": "
        << inferlab_plan::json_bool(source.blocks_release) << "\n";
    out << item << "}"
        << (index + 1U == projection.contract_sources.size() ? "" : ",");
  }
  if (!projection.contract_sources.empty()) out << "\n" << nested;
  out << "],\n";
  out << nested << "\"post_generation_observations\": [";
  for (std::size_t index = 0;
       index < projection.post_generation_observations.size(); ++index) {
    const auto& source = projection.post_generation_observations[index];
    out << "\n" << item << "{\n";
    out << item << "  \"id\": " << inferlab_plan::json_string(source.id)
        << ",\n";
    out << item << "  \"canonical_owner\": "
        << inferlab_plan::json_string(source.canonical_owner) << ",\n";
    out << item << "  \"issue_count\": " << source.issue_count << ",\n";
    out << item << "  \"blocks_release\": false,\n";
    out << item
        << "  \"dependency_phase\": \"post_generation_observation\"\n";
    out << item << "}"
        << (index + 1U == projection.post_generation_observations.size()
                ? ""
                : ",");
  }
  if (!projection.post_generation_observations.empty()) {
    out << "\n" << nested;
  }
  out << "],\n";
  out << nested
      << "\"claim_boundary\": \"contract diagnostics only; this gate does not execute benchmarks, product replay, CUDA kernels, or target-machine validation\"\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_pipeline
