#include "edge_cloud_routing_view.hpp"

#include "edge_cloud_routing_session_view.hpp"

#include "plan_json_utils.hpp"

#include <optional>
#include <sstream>

namespace inferlab::edge_cloud_routing {
namespace {

const char* bool_json(const bool value) { return value ? "true" : "false"; }

std::string optional_size_json(
    const std::optional<std::size_t>& value) {
  return value.has_value() ? std::to_string(*value) : "null";
}

std::string optional_double_json(const std::optional<double>& value) {
  return value.has_value() ? inferlab_plan::fixed6(*value) : "null";
}

std::string network_quality_assessment_json(
    const EdgeCloudNetworkQualityAssessment& assessment,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"eligible\": " << bool_json(assessment.eligible)
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(assessment.status) << ",\n";
  out << nested << "\"observation_age_ms\": "
      << (assessment.observation_age_ms.has_value()
              ? std::to_string(*assessment.observation_age_ms)
              : "null")
      << ",\n";
  out << nested << "\"blockers\": "
      << inferlab_plan::json_string_array(assessment.blockers, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string budget_assessment_json(
    const EdgeCloudBudgetAssessment& assessment, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"local_eligible\": "
      << bool_json(assessment.local_eligible) << ",\n";
  out << nested << "\"cloud_eligible\": "
      << bool_json(assessment.cloud_eligible) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(assessment.status) << ",\n";
  out << nested << "\"observation_age_ms\": "
      << (assessment.observation_age_ms.has_value()
              ? std::to_string(*assessment.observation_age_ms)
              : "null")
      << ",\n";
  out << nested << "\"local_blockers\": "
      << inferlab_plan::json_string_array(assessment.local_blockers,
                                          indent + 2)
      << ",\n";
  out << nested << "\"cloud_blockers\": "
      << inferlab_plan::json_string_array(assessment.cloud_blockers,
                                          indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string budget_policy_input_json(
    const EdgeCloudBudgetPolicyInput& input, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"battery_gate_enabled\": "
      << bool_json(input.battery_gate_enabled) << ",\n";
  out << nested << "\"battery_observation_available\": "
      << bool_json(input.battery_observation_available) << ",\n";
  out << nested << "\"battery_percent\": "
      << inferlab_plan::fixed6(input.battery_percent) << ",\n";
  out << nested << "\"minimum_local_battery_percent\": "
      << inferlab_plan::fixed6(input.minimum_local_battery_percent)
      << ",\n";
  out << nested << "\"local_energy_gate_enabled\": "
      << bool_json(input.local_energy_gate_enabled) << ",\n";
  out << nested << "\"local_energy_estimate_available\": "
      << bool_json(input.local_energy_estimate_available) << ",\n";
  out << nested << "\"estimated_local_energy_uj\": "
      << input.estimated_local_energy_uj << ",\n";
  out << nested << "\"maximum_local_energy_uj\": "
      << input.maximum_local_energy_uj << ",\n";
  out << nested << "\"cloud_cost_gate_enabled\": "
      << bool_json(input.cloud_cost_gate_enabled) << ",\n";
  out << nested << "\"cloud_cost_estimate_available\": "
      << bool_json(input.cloud_cost_estimate_available) << ",\n";
  out << nested << "\"estimated_cloud_request_cost_usd\": "
      << inferlab_plan::fixed6(input.estimated_cloud_request_cost_usd)
      << ",\n";
  out << nested << "\"maximum_cloud_request_cost_usd\": "
      << inferlab_plan::fixed6(input.maximum_cloud_request_cost_usd)
      << ",\n";
  out << nested << "\"observation_age_ms\": "
      << input.observation_age_ms << ",\n";
  out << nested << "\"maximum_observation_age_ms\": "
      << input.maximum_observation_age_ms << ",\n";
  out << nested << "\"provenance\": "
      << inferlab_plan::json_string(input.provenance) << "\n";
  out << pad << "}";
  return out.str();
}

std::string attempt_json(const EdgeCloudAttempt& attempt, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"attempt_id\": "
      << inferlab_plan::json_string(attempt.attempt_id) << ",\n";
  out << nested << "\"placement\": "
      << inferlab_plan::json_string(
             edge_cloud_placement_name(attempt.placement))
      << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(
             edge_runtime_status_name(attempt.status))
      << ",\n";
  out << nested << "\"error_category\": "
      << inferlab_plan::json_string(attempt.error_category) << ",\n";
  out << nested << "\"finish_reason\": "
      << (attempt.finish_reason.empty()
              ? "null"
              : inferlab_plan::json_string(attempt.finish_reason))
      << ",\n";
  out << nested << "\"delivered_stream_chunks\": "
      << attempt.delivered_chunks << ",\n";
  out << nested << "\"reported_prompt_tokens\": "
      << optional_size_json(attempt.reported_prompt_tokens) << ",\n";
  out << nested << "\"reported_completion_tokens\": "
      << optional_size_json(attempt.reported_completion_tokens) << ",\n";
  out << nested << "\"duration_ms\": "
      << optional_double_json(attempt.duration_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string attempts_json(const std::vector<EdgeCloudAttempt>& attempts,
                          const int indent) {
  if (attempts.empty()) return "[]";
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < attempts.size(); ++index) {
    out << attempt_json(attempts[index], indent + 2);
    out << (index + 1U == attempts.size() ? "\n" : ",\n");
  }
  out << inferlab_plan::json_indent(indent) << "]";
  return out.str();
}

std::string execution_json(const Artifact& artifact, const int indent) {
  if (!artifact.execution_result_present) return "null";
  const auto& result = artifact.execution;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(edge_runtime_status_name(result.status))
      << ",\n";
  out << nested << "\"error_category\": "
      << inferlab_plan::json_string(result.error_category) << ",\n";
  out << nested << "\"final_placement\": "
      << inferlab_plan::json_string(
             edge_cloud_placement_name(result.final_placement))
      << ",\n";
  out << nested << "\"finish_reason\": "
      << (result.finish_reason.empty()
              ? "null"
              : inferlab_plan::json_string(result.finish_reason))
      << ",\n";
  out << nested << "\"delivered_stream_chunks\": "
      << result.delivered_chunks << ",\n";
  out << nested << "\"reported_prompt_tokens\": "
      << optional_size_json(result.reported_prompt_tokens) << ",\n";
  out << nested << "\"reported_completion_tokens\": "
      << optional_size_json(result.reported_completion_tokens) << ",\n";
  out << nested << "\"fallback_attempted\": "
      << bool_json(result.fallback_attempted) << ",\n";
  out << nested << "\"fallback_performed\": "
      << bool_json(result.fallback_performed) << ",\n";
  out << nested << "\"fallback_network_quality\": "
      << (result.fallback_network_quality_present
              ? network_quality_assessment_json(
                    result.fallback_network_quality, indent + 2)
              : "null")
      << ",\n";
  out << nested << "\"fallback_budget\": "
      << (result.fallback_budget_present
              ? budget_assessment_json(result.fallback_budget, indent + 2)
              : "null")
      << ",\n";
  out << nested << "\"fallback_session_recovery\": "
      << (result.fallback_session_recovery_present
              ? session_recovery_assessment_json(
                    result.fallback_session_recovery, indent + 2)
              : "null")
      << ",\n";
  out << nested << "\"attempts\": "
      << attempts_json(result.attempts, indent + 2) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Artifact& artifact) {
  validate_artifact(artifact);
  const std::string status =
      artifact.summary.decision == "blocked" ? "blocked" : "ready";
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_edge_cloud_routing\",\n";
  out << "  \"status\": " << inferlab_plan::json_string(status) << ",\n";
  out << "  \"mode\": "
      << inferlab_plan::json_string(mode_name(artifact.mode)) << ",\n";
  out << "  \"executed\": " << bool_json(artifact.executed) << ",\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++17 request-before-execution edge/cloud placement, privacy/deadline/network/budget gating, durable-transcript pre-generation session recovery admission, local zero-output retry eligibility, optional libcurl OpenAI-compatible SSE transport and redacted execution evidence; recovery restarts only from a caller-reconstructed prompt after a completed turn or an incomplete zero-output request, never resumes partial output or transfers KV/hidden state, and configured observations/estimates are not active probes, device-exclusive measurements or billing records\",\n";
  out << "  \"generated_at\": "
      << inferlab_plan::json_string(inferlab_plan::current_utc_timestamp())
      << ",\n";
  out << "  \"config\": {\n";
  out << "    \"path_label\": "
      << inferlab_plan::json_string(artifact.config.path_label) << ",\n";
  out << "    \"path_redacted\": true,\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(artifact.config.status) << ",\n";
  out << "    \"schema_version\": " << artifact.config.schema_version
      << ",\n";
  out << "    \"target_platform\": "
      << inferlab_plan::json_string(artifact.config.target_platform) << ",\n";
  out << "    \"target_platform_match_status\": "
      << inferlab_plan::json_string(
             artifact.config.target_platform_match_status)
      << ",\n";
  out << "    \"request_fingerprint\": "
      << (artifact.config.request_fingerprint.empty()
              ? "null"
              : inferlab_plan::json_string(
                    artifact.config.request_fingerprint))
      << ",\n";
  out << "    \"session_identity_emitted\": false,\n";
  out << "    \"session_recovery_requested\": "
      << bool_json(artifact.config.session_recovery_requested) << ",\n";
  out << "    \"prompt_fingerprint\": "
      << (artifact.config.prompt_fingerprint.empty()
              ? "null"
              : inferlab_plan::json_string(artifact.config.prompt_fingerprint))
      << ",\n";
  out << "    \"prompt_fingerprint_redacted_by_policy\": "
      << bool_json(artifact.config.prompt_fingerprint_redacted_by_policy)
      << ",\n";
  out << "    \"fingerprint_algorithm\": \"fnv1a64_noncryptographic\",\n";
  out << "    \"prompt_bytes\": " << artifact.config.prompt_bytes << ",\n";
  out << "    \"prompt_text_emitted\": false,\n";
  out << "    \"local_model_path_label\": "
      << inferlab_plan::json_string(artifact.config.local_model_path_label)
      << ",\n";
  out << "    \"local_model_path_status\": "
      << inferlab_plan::json_string(
             artifact.config.local_model_path_status)
      << ",\n";
  out << "    \"endpoint_emitted\": false,\n";
  out << "    \"endpoint_security\": "
      << inferlab_plan::json_string(artifact.config.endpoint_security)
      << ",\n";
  out << "    \"api_key_environment_variable\": "
      << inferlab_plan::json_string(
             artifact.config.api_key_environment_variable)
      << ",\n";
  out << "    \"api_key_value_emitted\": false,\n";
  out << "    \"product_id\": "
      << inferlab_plan::json_string(artifact.config.product_id) << ",\n";
  out << "    \"cloud_model_id\": "
      << inferlab_plan::json_string(artifact.config.cloud_model_id) << ",\n";
  out << "    \"data_classification\": "
      << inferlab_plan::json_string(artifact.config.data_classification)
      << "\n";
  out << "  },\n";
  out << "  \"dependency_status\": {\n";
  out << "    \"local_runtime\": "
      << inferlab_plan::json_string(artifact.dependencies.local_runtime_id)
      << ",\n";
  out << "    \"local_dependency_linked\": "
      << bool_json(artifact.dependencies.local_dependency_linked) << ",\n";
  out << "    \"local_streaming_supported\": "
      << bool_json(artifact.dependencies.local_streaming_supported) << ",\n";
  out << "    \"local_model_format_supported\": "
      << bool_json(artifact.dependencies.local_model_format_supported)
      << ",\n";
  out << "    \"cloud_transport\": "
      << inferlab_plan::json_string(artifact.dependencies.cloud_transport_id)
      << ",\n";
  out << "    \"cloud_transport_version\": "
      << inferlab_plan::json_string(
             artifact.dependencies.cloud_transport_version)
      << ",\n";
  out << "    \"cloud_dependency_linked\": "
      << bool_json(artifact.dependencies.cloud_dependency_linked) << ",\n";
  out << "    \"structured_json_parser_linked\": "
      << bool_json(artifact.dependencies.structured_json_parser_linked)
      << ",\n";
  out << "    \"cloud_streaming_supported\": "
      << bool_json(artifact.dependencies.cloud_streaming_supported) << ",\n";
  out << "    \"cloud_cancel_supported\": "
      << bool_json(artifact.dependencies.cloud_cancel_supported) << ",\n";
  out << "    \"api_key_environment_configured\": "
      << bool_json(
             artifact.dependencies.api_key_environment_configured)
      << "\n";
  out << "  },\n";
  out << "  \"policy_input\": {\n";
  out << "    \"local_runtime_ready\": "
      << bool_json(artifact.policy.local_runtime_ready) << ",\n";
  out << "    \"local_resource_fit\": "
      << bool_json(artifact.policy.local_resource_fit) << ",\n";
  out << "    \"cloud_transport_ready\": "
      << bool_json(artifact.policy.cloud_transport_ready) << ",\n";
  out << "    \"network_available\": "
      << bool_json(artifact.policy.network_available) << ",\n";
  out << "    \"remote_processing_allowed\": "
      << bool_json(artifact.policy.remote_processing_allowed) << ",\n";
  out << "    \"prefer_local\": "
      << bool_json(artifact.policy.prefer_local) << ",\n";
  out << "    \"remaining_deadline_ms\": "
      << artifact.policy.remaining_deadline_ms << ",\n";
  out << "    \"minimum_cloud_deadline_ms\": "
      << artifact.policy.minimum_cloud_deadline_ms << ",\n";
  out << "    \"network_available_provenance\": \"configuration_declaration_not_network_probe\",\n";
  out << "    \"network_quality_input\": {\n";
  out << "      \"policy_enabled\": "
      << bool_json(artifact.policy.network_quality.policy_enabled) << ",\n";
  out << "      \"observation_available\": "
      << bool_json(artifact.policy.network_quality.observation_available)
      << ",\n";
  out << "      \"round_trip_time_ms\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.round_trip_time_ms)
      << ",\n";
  out << "      \"jitter_ms\": "
      << inferlab_plan::fixed6(artifact.policy.network_quality.jitter_ms)
      << ",\n";
  out << "      \"packet_loss_ratio\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.packet_loss_ratio)
      << ",\n";
  out << "      \"downlink_mbps\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.downlink_mbps)
      << ",\n";
  out << "      \"observation_age_ms\": "
      << artifact.policy.network_quality.observation_age_ms << ",\n";
  out << "      \"maximum_round_trip_time_ms\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.maximum_round_trip_time_ms)
      << ",\n";
  out << "      \"maximum_jitter_ms\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.maximum_jitter_ms)
      << ",\n";
  out << "      \"maximum_packet_loss_ratio\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.maximum_packet_loss_ratio)
      << ",\n";
  out << "      \"minimum_downlink_mbps\": "
      << inferlab_plan::fixed6(
             artifact.policy.network_quality.minimum_downlink_mbps)
      << ",\n";
  out << "      \"maximum_observation_age_ms\": "
      << artifact.policy.network_quality.maximum_observation_age_ms
      << ",\n";
  out << "      \"provenance\": "
      << inferlab_plan::json_string(
             artifact.policy.network_quality.provenance)
      << "\n";
  out << "    },\n";
  out << "    \"budget_input\": "
      << budget_policy_input_json(artifact.policy.budget, 4) << ",\n";
  out << "    \"session_recovery_input\": "
      << session_recovery_input_json(artifact.session_recovery_input, 4)
      << "\n";
  out << "  },\n";
  out << "  \"placement_decision\": {\n";
  out << "    \"placement\": "
      << inferlab_plan::json_string(
             edge_cloud_placement_name(artifact.placement.placement))
      << ",\n";
  out << "    \"reason\": "
      << inferlab_plan::json_string(artifact.placement.reason) << ",\n";
  out << "    \"remote_eligible\": "
      << bool_json(artifact.placement.remote_eligible) << ",\n";
  out << "    \"fallback_eligible\": "
      << bool_json(artifact.placement.fallback_eligible) << ",\n";
  out << "    \"network_quality\": "
      << network_quality_assessment_json(
             artifact.placement.network_quality, 4)
      << ",\n";
  out << "    \"budget\": "
      << budget_assessment_json(artifact.placement.budget, 4) << ",\n";
  out << "    \"session_recovery\": "
      << session_recovery_assessment_json(
             artifact.placement.session_recovery, 4)
      << "\n";
  out << "  },\n";
  out << "  \"local_load\": {\n";
  out << "    \"attempted\": " << bool_json(artifact.local_load.attempted)
      << ",\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(artifact.local_load.status)
      << ",\n";
  out << "    \"error_category\": "
      << inferlab_plan::json_string(artifact.local_load.error_category)
      << "\n";
  out << "  },\n";
  out << "  \"execution\": " << execution_json(artifact, 2) << ",\n";
  out << "  \"stream_accounting\": {\n";
  out << "    \"observed_stream_bytes\": "
      << artifact.observed_stream_bytes << ",\n";
  out << "    \"stream_text_emitted\": false,\n";
  out << "    \"semantic\": \"SSE deltas are text chunks; delivered chunk counts are never presented as tokenizer token counts, while reported token counts come only from runtime or server usage metadata\"\n";
  out << "  },\n";
  out << "  \"security_controls\": {\n";
  out << "    \"device_only_remote_forbidden\": true,\n";
  out << "    \"tls_peer_verification_required\": true,\n";
  out << "    \"redirects_disabled\": true,\n";
  out << "    \"loopback_http_requires_explicit_opt_in\": true,\n";
  out << "    \"fallback_requires_zero_delivered_output\": true,\n";
  out << "    \"cancelled_or_deadline_exhausted_fallback_forbidden\": true,\n";
  out << "    \"budget_estimates_are_caller_supplied_not_measurements_or_bills\": true,\n";
  out << "    \"partial_output_session_resume_forbidden\": true,\n";
  out << "    \"session_recovery_prompt_fingerprint_suppressed\": true\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"evidence_level\": "
      << inferlab_plan::json_string(artifact.summary.evidence_level) << ",\n";
  out << "    \"decision\": "
      << inferlab_plan::json_string(artifact.summary.decision) << ",\n";
  out << "    \"local_decision_count\": "
      << artifact.summary.local_decision_count << ",\n";
  out << "    \"cloud_decision_count\": "
      << artifact.summary.cloud_decision_count << ",\n";
  out << "    \"local_then_cloud_decision_count\": "
      << artifact.summary.local_then_cloud_decision_count << ",\n";
  out << "    \"reject_decision_count\": "
      << artifact.summary.reject_decision_count << ",\n";
  out << "    \"attempt_count\": " << artifact.summary.attempt_count
      << ",\n";
  out << "    \"local_attempt_count\": "
      << artifact.summary.local_attempt_count << ",\n";
  out << "    \"cloud_attempt_count\": "
      << artifact.summary.cloud_attempt_count << ",\n";
  out << "    \"successful_attempt_count\": "
      << artifact.summary.successful_attempt_count << ",\n";
  out << "    \"failed_attempt_count\": "
      << artifact.summary.failed_attempt_count << ",\n";
  out << "    \"cancelled_attempt_count\": "
      << artifact.summary.cancelled_attempt_count << ",\n";
  out << "    \"delivered_chunk_count\": "
      << artifact.summary.delivered_chunk_count << ",\n";
  out << "    \"pre_output_fallback_count\": "
      << artifact.summary.pre_output_fallback_count << ",\n";
  out << "    \"privacy_blocker_count\": "
      << artifact.summary.privacy_blocker_count << ",\n";
  out << "    \"network_blocker_count\": "
      << artifact.summary.network_blocker_count << ",\n";
  out << "    \"resource_blocker_count\": "
      << artifact.summary.resource_blocker_count << ",\n";
  out << "    \"dependency_blocker_count\": "
      << artifact.summary.dependency_blocker_count << ",\n";
  out << "    \"network_quality_issue_count\": "
      << artifact.summary.network_quality_issue_count << ",\n";
  out << "    \"budget_issue_count\": "
      << artifact.summary.budget_issue_count << ",\n";
  out << "    \"local_budget_ineligible_count\": "
      << artifact.summary.local_budget_ineligible_count << ",\n";
  out << "    \"cloud_budget_ineligible_count\": "
      << artifact.summary.cloud_budget_ineligible_count << ",\n";
  out << "    \"session_recovery_issue_count\": "
      << artifact.summary.session_recovery_issue_count << ",\n";
  out << "    \"session_recovery_ready_count\": "
      << artifact.summary.session_recovery_ready_count << ",\n";
  out << "    \"session_recovery_blocked_count\": "
      << artifact.summary.session_recovery_blocked_count << ",\n";
  out << "    \"fallback_attempted\": "
      << bool_json(artifact.summary.fallback_attempted) << ",\n";
  out << "    \"fallback_performed\": "
      << bool_json(artifact.summary.fallback_performed) << ",\n";
  out << "    \"blocker_count\": " << artifact.summary.blocker_count
      << "\n";
  out << "  },\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(artifact.blockers, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             artifact.recommended_next_commands, 2)
      << ",\n";
  out << "  \"not_run\": "
      << inferlab_plan::json_string_array(
             artifact.mode == Mode::kPlan
                 ? std::vector<std::string>{
                       "local generation not run in plan mode",
                       "cloud endpoint not called in plan mode",
                       "no performance or product execute claim"}
                 : (!artifact.executed
                        ? std::vector<std::string>{
                              "no local or cloud generation attempt was admitted",
                              "no performance or product execute claim"}
                        : std::vector<std::string>{}),
             2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::edge_cloud_routing
