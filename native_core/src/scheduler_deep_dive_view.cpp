#include "scheduler_deep_dive_view.hpp"

#include "advanced_serving_internal.hpp"
#include "native_trace_context_view.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::scheduler_deep_dive {
namespace {

using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_unique_nonempty_strings;

void validate_artifact_contract(const ArtifactContract& contract) {
  (void)artifact_role_name(contract.role);
  require_nonempty(contract.artifact_type,
                   "scheduler deep-dive artifact type");
  require_nonempty(contract.target_name,
                   "scheduler deep-dive target name");
  require_nonempty(contract.artifact_path,
                   "scheduler deep-dive artifact path");
  require_nonempty(contract.refresh_command,
                   "scheduler deep-dive refresh command");
  require_nonempty(contract.deep_dive_type,
                   "scheduler deep-dive type");
  require_nonempty(contract.claim_scope,
                   "scheduler deep-dive claim scope");
  require_unique_nonempty_strings(
      contract.research_basis,
      "scheduler deep-dive research basis");
  require_unique_nonempty_strings(
      contract.product_alignment,
      "scheduler deep-dive product alignment");
  require_unique_nonempty_strings(
      contract.evidence_bridge_inputs,
      "scheduler deep-dive evidence inputs");
}

const ArtifactContract& batching_contract() {
  static const ArtifactContract contract = {
      ArtifactRole::batching,
      "native_scheduler_batching_deep_dive",
      "inferlab_scheduler_batching_deep_dive",
      "results/raw/native_scheduler_batching_deep_dive.json",
      "./build/inferlab_scheduler_batching_deep_dive --repo-root . --output results/raw/native_scheduler_batching_deep_dive.json",
      "scheduler_batching",
      "native scheduler deep-dive plan; not a product benchmark",
      {"continuous batching", "chunked prefill", "SLO admission",
       "tenant quota", "prefill/decode disaggregation"},
      {"vLLM", "SGLang", "Dynamo", "KServe/llm-d", "Ray Serve"},
      {"results/raw/scheduler_trace.json",
       "results/raw/scheduler_pressure_sweep.json",
       "results/raw/native_chunked_prefill_scheduler_plan.json",
       "results/raw/native_slo_capacity_plan.json",
       "results/raw/product_vllm_capability_profile.json",
       "results/raw/product_sglang_capability_profile.json"}};
  static const bool validated = [] {
    validate_artifact_contract(contract);
    return true;
  }();
  (void)validated;
  return contract;
}

const ArtifactContract& root_cause_contract() {
  static const ArtifactContract contract = {
      ArtifactRole::root_cause,
      "native_scheduler_root_cause_deep_dive",
      "inferlab_scheduler_root_cause_deep_dive",
      "results/raw/native_scheduler_root_cause_deep_dive.json",
      "./build/inferlab_scheduler_root_cause_deep_dive --repo-root . --output results/raw/native_scheduler_root_cause_deep_dive.json",
      "scheduler_root_cause",
      "native scheduler root-cause mechanism slice; not measured product scheduler evidence",
      {"continuous batching", "chunked prefill", "tenant quota",
       "admission control", "SLO root cause", "KV pressure"},
      {"vLLM", "SGLang", "Dynamo", "KServe/llm-d", "Ray Serve",
       "LMCache", "Mooncake"},
      {"results/raw/native_scheduler_batching_deep_dive.json",
       "results/raw/native_kv_tier_decision_deep_dive.json",
       "results/raw/native_pd_split_crossover_deep_dive.json",
       "results/raw/scheduler_trace.json",
       "results/raw/product_vllm_capability_profile.json",
       "results/raw/product_sglang_capability_profile.json",
       "results/raw/product_kserve_llmd_profile.json"}};
  static const bool validated = [] {
    validate_artifact_contract(contract);
    return true;
  }();
  (void)validated;
  return contract;
}

std::string admission_weights_json(const AdmissionScoreWeights& weights,
                                   const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"decode_stall_policy\": "
      << inferlab_plan::fixed6(weights.decode_stall_policy) << ",\n";
  out << nested << "\"fairness\": "
      << inferlab_plan::fixed6(weights.fairness) << ",\n";
  out << nested << "\"inverse_chunk_pressure\": "
      << inferlab_plan::fixed6(weights.inverse_chunk_pressure) << ",\n";
  out << nested << "\"inverse_kv_pressure\": "
      << inferlab_plan::fixed6(weights.inverse_kv_pressure) << ",\n";
  out << nested << "\"inverse_slo_risk\": "
      << inferlab_plan::fixed6(weights.inverse_slo_risk) << "\n";
  out << pad << "}";
  return out.str();
}

std::string deep_dive_weights_json(const DeepDiveScoreWeights& weights,
                                   const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"admission\": "
      << inferlab_plan::fixed6(weights.admission) << ",\n";
  out << nested << "\"fairness\": "
      << inferlab_plan::fixed6(weights.fairness) << ",\n";
  out << nested << "\"healthy_root_cause\": "
      << inferlab_plan::fixed6(weights.healthy_root_cause) << ",\n";
  out << nested << "\"inverse_decode_stall\": "
      << inferlab_plan::fixed6(weights.inverse_decode_stall) << ",\n";
  out << nested << "\"inverse_slo_risk\": "
      << inferlab_plan::fixed6(weights.inverse_slo_risk) << "\n";
  out << pad << "}";
  return out.str();
}

std::string assumptions_json(const Assumptions& assumptions,
                             const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"admission_score_weights\": "
      << admission_weights_json(assumptions.admission_weights,
                                indent + 2U)
      << ",\n";
  out << nested << "\"admission_threshold\": "
      << inferlab_plan::fixed6(assumptions.admission_threshold) << ",\n";
  out << nested << "\"canonical_chunk_launch_overhead_ms\": "
      << inferlab_plan::fixed6(
             assumptions.canonical_chunk_launch_overhead_ms)
      << ",\n";
  out << nested << "\"canonical_kv_budget_gb\": "
      << inferlab_plan::fixed6(assumptions.canonical_kv_budget_gb)
      << ",\n";
  out << nested << "\"canonical_kv_bytes_per_token\": "
      << inferlab_plan::fixed6(
             assumptions.canonical_kv_bytes_per_token)
      << ",\n";
  out << nested << "\"canonical_prefix_cache_hit_rate\": "
      << inferlab_plan::fixed6(
             assumptions.canonical_prefix_cache_hit_rate)
      << ",\n";
  out << nested << "\"chunk_pressure_tick_window\": "
      << inferlab_plan::fixed6(
             assumptions.chunk_pressure_tick_window)
      << ",\n";
  out << nested << "\"deep_dive_score_weights\": "
      << deep_dive_weights_json(assumptions.deep_dive_weights,
                                indent + 2U)
      << ",\n";
  out << nested << "\"decode_stall_normalization_ms\": "
      << inferlab_plan::fixed6(
             assumptions.decode_stall_normalization_ms)
      << ",\n";
  out << nested << "\"decode_stall_policy_threshold_ms\": "
      << inferlab_plan::fixed6(
             assumptions.decode_stall_policy_threshold_ms)
      << ",\n";
  out << nested << "\"defer_quota_threshold\": "
      << inferlab_plan::fixed6(assumptions.defer_quota_threshold)
      << ",\n";
  out << nested << "\"defer_risk_threshold\": "
      << inferlab_plan::fixed6(assumptions.defer_risk_threshold)
      << ",\n";
  out << nested << "\"fairness_policy\": "
      << inferlab_plan::json_string(
             fairness_policy_name(assumptions.fairness_policy))
      << ",\n";
  out << nested << "\"fairness_weights\": {\n";
  out << nested << "  \"active_request_penalty_cap\": "
      << inferlab_plan::fixed6(
             assumptions.fairness_active_request_penalty_cap)
      << ",\n";
  out << nested << "  \"active_request_weight\": "
      << inferlab_plan::fixed6(
             assumptions.fairness_active_request_weight)
      << ",\n";
  out << nested << "  \"queue_depth_weight\": "
      << inferlab_plan::fixed6(
             assumptions.fairness_queue_depth_weight)
      << ",\n";
  out << nested << "  \"quota_weight\": "
      << inferlab_plan::fixed6(assumptions.fairness_quota_weight)
      << ",\n";
  out << nested << "  \"tenant_weight\": "
      << inferlab_plan::fixed6(assumptions.fairness_tenant_weight)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"high_stall_policy_credit\": "
      << inferlab_plan::fixed6(assumptions.high_stall_policy_credit)
      << ",\n";
  out << nested << "\"max_visible_prefill_chunks\": "
      << assumptions.max_visible_prefill_chunks << ",\n";
  out << nested << "\"p99_quota_penalty_ms\": "
      << inferlab_plan::fixed6(assumptions.p99_quota_penalty_ms)
      << ",\n";
  out << nested << "\"p99_risk_weight\": "
      << inferlab_plan::fixed6(assumptions.p99_risk_weight) << ",\n";
  out << nested << "\"p99_tenant_tick_weight\": "
      << inferlab_plan::fixed6(assumptions.p99_tenant_tick_weight)
      << ",\n";
  out << nested << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << ",\n";
  out << nested << "\"queue_base_factor\": "
      << inferlab_plan::fixed6(assumptions.queue_base_factor) << ",\n";
  out << nested << "\"queue_kv_pressure_weight\": "
      << inferlab_plan::fixed6(
             assumptions.queue_kv_pressure_weight)
      << ",\n";
  out << nested << "\"queue_quota_pressure_weight\": "
      << inferlab_plan::fixed6(
             assumptions.queue_quota_pressure_weight)
      << ",\n";
  out << nested << "\"ready_score_threshold\": "
      << inferlab_plan::fixed6(assumptions.ready_score_threshold)
      << ",\n";
  out << nested << "\"recommendation_stall_threshold_ms\": "
      << inferlab_plan::fixed6(
             assumptions.recommendation_stall_threshold_ms)
      << ",\n";
  out << nested << "\"role\": "
      << inferlab_plan::json_string(artifact_role_name(assumptions.role))
      << ",\n";
  out << nested << "\"root_cause_thresholds\": {\n";
  out << nested << "  \"decode_stall_ms\": "
      << inferlab_plan::fixed6(
             assumptions.root_decode_stall_threshold_ms)
      << ",\n";
  out << nested << "  \"kv_pressure_ratio\": "
      << inferlab_plan::fixed6(assumptions.root_kv_threshold) << ",\n";
  out << nested << "  \"queue_ttft_share\": "
      << inferlab_plan::fixed6(
             assumptions.root_queue_ttft_share_threshold)
      << ",\n";
  out << nested << "  \"quota_pressure_ratio\": "
      << inferlab_plan::fixed6(assumptions.root_quota_threshold)
      << ",\n";
  out << nested << "  \"slo_violation_risk\": "
      << inferlab_plan::fixed6(assumptions.root_slo_risk_threshold)
      << "\n";
  out << nested << "},\n";
  out << nested << "\"tpot_decode_starvation_weight\": "
      << inferlab_plan::fixed6(
             assumptions.tpot_decode_starvation_weight)
      << ",\n";
  out << nested << "\"ttft_risk_weight\": "
      << inferlab_plan::fixed6(assumptions.ttft_risk_weight) << ",\n";
  out << nested << "\"unhealthy_root_cause_credit\": "
      << inferlab_plan::fixed6(
             assumptions.unhealthy_root_cause_credit)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string timeline_json(const std::vector<TimelineEvent>& events,
                          const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  const std::string fields(indent + 4U, ' ');
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < events.size(); ++index) {
    const auto& event = events[index];
    out << nested << "{\n";
    out << fields << "\"duration_ms\": "
        << inferlab_plan::fixed6(event.duration_ms) << ",\n";
    out << fields << "\"lane\": "
        << inferlab_plan::json_string(event.lane) << ",\n";
    out << fields << "\"name\": "
        << inferlab_plan::json_string(event.name) << ",\n";
    out << fields << "\"start_ms\": "
        << inferlab_plan::fixed6(event.start_ms) << "\n";
    out << nested << "}"
        << (index + 1U == events.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string canonical_projection_json(
    const CanonicalChunkedPrefillProjection& projection,
    const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"chunk_policy\": "
      << inferlab_plan::json_string(projection.chunk_policy) << ",\n";
  out << nested << "\"decode_maximal_batch_size\": "
      << projection.decode_maximal_batch_size << ",\n";
  out << nested << "\"decode_stall_ms\": "
      << inferlab_plan::fixed6(projection.decode_stall_ms) << ",\n";
  out << nested << "\"decode_starvation_risk\": "
      << inferlab_plan::fixed6(projection.decode_starvation_risk)
      << ",\n";
  out << nested << "\"effective_prompt_tokens\": "
      << projection.effective_prompt_tokens << ",\n";
  out << nested << "\"first_chunk_ttft_ms\": "
      << inferlab_plan::fixed6(projection.first_chunk_ttft_ms)
      << ",\n";
  out << nested << "\"mechanism_fairness_estimate\": "
      << inferlab_plan::fixed6(projection.mechanism_fairness_estimate)
      << ",\n";
  out << nested << "\"mechanism_p99_risk\": "
      << inferlab_plan::fixed6(projection.mechanism_p99_risk) << ",\n";
  out << nested << "\"owner\": "
      << inferlab_plan::json_string(
             "inferlab::estimate_chunked_prefill_scheduler")
      << ",\n";
  out << nested << "\"prefill_chunk_count\": "
      << projection.prefill_chunk_count << ",\n";
  out << nested << "\"prefill_wave_count\": "
      << projection.prefill_wave_count << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(projection.recommendation) << ",\n";
  out << nested << "\"scheduler_gap_ms\": "
      << inferlab_plan::fixed6(projection.scheduler_gap_ms) << ",\n";
  out << nested << "\"total_prefill_ms\": "
      << inferlab_plan::fixed6(projection.total_prefill_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const Scenario& scenario,
                          const ArtifactRole role,
                          const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  if (role == ArtifactRole::batching) {
    out << nested << "\"backend_family\": "
        << inferlab_plan::json_string(scenario.alignment) << ",\n";
    out << nested << "\"chunk_tokens\": " << scenario.chunk_tokens
        << ",\n";
    out << nested << "\"decode_ms_per_token\": "
        << inferlab_plan::fixed6(scenario.decode_ms_per_token) << ",\n";
    out << nested << "\"max_batch_tokens\": "
        << scenario.max_batch_tokens << ",\n";
    out << nested << "\"name\": "
        << inferlab_plan::json_string(scenario.scenario_id) << ",\n";
    out << nested << "\"output_tokens\": " << scenario.output_tokens
        << ",\n";
    out << nested << "\"prefill_ms_per_token\": "
        << inferlab_plan::fixed6(scenario.prefill_ms_per_token) << ",\n";
    out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
        << ",\n";
    out << nested << "\"queue_depth\": " << scenario.queue_depth
        << ",\n";
    out << nested << "\"scheduler_tick_ms\": "
        << inferlab_plan::fixed6(scenario.scheduler_tick_ms) << ",\n";
    out << nested << "\"slo_p99_ms\": "
        << inferlab_plan::fixed6(scenario.slo_p99_ms) << ",\n";
    out << nested << "\"slo_ttft_ms\": "
        << inferlab_plan::fixed6(scenario.slo_ttft_ms) << ",\n";
    out << nested << "\"tenant_count\": "
        << scenario.tenant_concurrency << "\n";
  } else {
    out << nested << "\"arrival_ms\": "
        << inferlab_plan::fixed6(scenario.arrival_ms) << ",\n";
    out << nested << "\"batch_token_budget\": "
        << scenario.max_batch_tokens << ",\n";
    out << nested << "\"decode_ms_per_token\": "
        << inferlab_plan::fixed6(scenario.decode_ms_per_token) << ",\n";
    out << nested << "\"decode_tokens\": " << scenario.output_tokens
        << ",\n";
    out << nested << "\"kv_pressure_ratio\": "
        << inferlab_plan::fixed6(scenario.kv_pressure_ratio) << ",\n";
    out << nested << "\"prefill_chunk_tokens\": "
        << scenario.chunk_tokens << ",\n";
    out << nested << "\"prefill_ms_per_token\": "
        << inferlab_plan::fixed6(scenario.prefill_ms_per_token) << ",\n";
    out << nested << "\"product_alignment\": "
        << inferlab_plan::json_string(scenario.alignment) << ",\n";
    out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
        << ",\n";
    out << nested << "\"queue_depth\": " << scenario.queue_depth
        << ",\n";
    out << nested << "\"quota_pressure_ratio\": "
        << inferlab_plan::fixed6(scenario.quota_pressure_ratio) << ",\n";
    out << nested << "\"request_id\": "
        << inferlab_plan::json_string(scenario.scenario_id) << ",\n";
    out << nested << "\"scheduler_tick_ms\": "
        << inferlab_plan::fixed6(scenario.scheduler_tick_ms) << ",\n";
    out << nested << "\"slo_p99_ms\": "
        << inferlab_plan::fixed6(scenario.slo_p99_ms) << ",\n";
    out << nested << "\"slo_ttft_ms\": "
        << inferlab_plan::fixed6(scenario.slo_ttft_ms) << ",\n";
    out << nested << "\"tenant_active_requests\": "
        << scenario.tenant_concurrency << ",\n";
    out << nested << "\"tenant_id\": "
        << inferlab_plan::json_string(scenario.tenant_id) << "\n";
  }
  out << pad << "}";
  return out.str();
}

std::string score_components_json(const Record& record,
                                  const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"decode_stall_policy_score\": "
      << inferlab_plan::fixed6(record.decode_stall_policy_score)
      << ",\n";
  out << nested << "\"fairness_score\": "
      << inferlab_plan::fixed6(record.fairness_score) << ",\n";
  out << nested << "\"inverse_chunk_pressure\": "
      << inferlab_plan::fixed6(record.inverse_chunk_pressure) << ",\n";
  out << nested << "\"inverse_kv_pressure\": "
      << inferlab_plan::fixed6(record.inverse_kv_pressure) << ",\n";
  out << nested << "\"p99_tail_risk\": "
      << inferlab_plan::fixed6(record.p99_tail_risk) << ",\n";
  out << nested << "\"ttft_risk\": "
      << inferlab_plan::fixed6(record.ttft_risk) << "\n";
  out << pad << "}";
  return out.str();
}

std::string batching_record_json(const Record& record,
                                 const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"active_work\": "
      << inferlab_plan::json_bool(record.active_work) << ",\n";
  out << nested << "\"admission_decision\": "
      << inferlab_plan::json_string(
             admission_decision_name(record.admission_decision))
      << ",\n";
  out << nested << "\"admission_score\": "
      << inferlab_plan::fixed6(record.admission_score) << ",\n";
  out << nested << "\"canonical_chunked_prefill\": "
      << canonical_projection_json(record.canonical_chunked_prefill,
                                   indent + 2U)
      << ",\n";
  out << nested << "\"decode_maximal_batch_size\": "
      << record.canonical_chunked_prefill.decode_maximal_batch_size
      << ",\n";
  out << nested << "\"decode_stall_ms\": "
      << inferlab_plan::fixed6(record.decode_stall_ms) << ",\n";
  out << nested << "\"jain_fairness_estimate\": "
      << inferlab_plan::fixed6(record.fairness_score) << ",\n";
  out << nested << "\"p99_latency_ms\": "
      << inferlab_plan::fixed6(record.p99_latency_ms) << ",\n";
  out << nested << "\"prefill_chunk_count\": "
      << record.canonical_chunked_prefill.prefill_chunk_count << ",\n";
  out << nested << "\"prefill_chunk_timeline\": "
      << timeline_json(record.timeline, indent + 2U) << ",\n";
  out << nested << "\"queue_wait_ms\": "
      << inferlab_plan::fixed6(record.queue_wait_ms) << ",\n";
  out << nested << "\"ranking_applicable\": "
      << inferlab_plan::json_bool(record.ranking_applicable) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommended_action_name(record.recommended_action))
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(record.scenario, ArtifactRole::batching,
                       indent + 2U)
      << ",\n";
  out << nested << "\"score_components\": "
      << score_components_json(record, indent + 2U) << ",\n";
  out << nested << "\"scheduler_deep_dive_score\": "
      << inferlab_plan::fixed6(record.scheduler_deep_dive_score)
      << ",\n";
  out << nested << "\"slo_violation_risk\": "
      << inferlab_plan::fixed6(record.slo_violation_risk) << ",\n";
  out << nested << "\"tpot_ms\": "
      << inferlab_plan::fixed6(record.tpot_ms) << ",\n";
  out << nested << "\"ttft_ms\": "
      << inferlab_plan::fixed6(record.ttft_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string root_cause_record_json(const Record& record,
                                   const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"active_work\": "
      << inferlab_plan::json_bool(record.active_work) << ",\n";
  out << nested << "\"admission_decision\": "
      << inferlab_plan::json_string(
             admission_decision_name(record.admission_decision))
      << ",\n";
  out << nested << "\"admission_score\": "
      << inferlab_plan::fixed6(record.admission_score) << ",\n";
  out << nested << "\"canonical_chunked_prefill\": "
      << canonical_projection_json(record.canonical_chunked_prefill,
                                   indent + 2U)
      << ",\n";
  out << nested << "\"decode_stall_ms\": "
      << inferlab_plan::fixed6(record.decode_stall_ms) << ",\n";
  out << nested << "\"fairness_score\": "
      << inferlab_plan::fixed6(record.fairness_score) << ",\n";
  out << nested << "\"p99_latency_ms\": "
      << inferlab_plan::fixed6(record.p99_latency_ms) << ",\n";
  out << nested << "\"p99_risk\": "
      << inferlab_plan::fixed6(record.slo_violation_risk) << ",\n";
  out << nested << "\"prefill_chunk_ms\": "
      << inferlab_plan::fixed6(record.prefill_chunk_ms) << ",\n";
  out << nested << "\"queue_wait_ms\": "
      << inferlab_plan::fixed6(record.queue_wait_ms) << ",\n";
  out << nested << "\"ranking_applicable\": "
      << inferlab_plan::json_bool(record.ranking_applicable) << ",\n";
  out << nested << "\"recommended_action\": "
      << inferlab_plan::json_string(
             recommended_action_name(record.recommended_action))
      << ",\n";
  out << nested << "\"root_cause\": "
      << inferlab_plan::json_string(root_cause_name(record.root_cause))
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(record.scenario, ArtifactRole::root_cause,
                       indent + 2U)
      << ",\n";
  out << nested << "\"score_components\": "
      << score_components_json(record, indent + 2U) << ",\n";
  out << nested << "\"scheduler_deep_dive_score\": "
      << inferlab_plan::fixed6(record.scheduler_deep_dive_score)
      << ",\n";
  out << nested << "\"tpot_ms\": "
      << inferlab_plan::fixed6(record.tpot_ms) << ",\n";
  out << nested << "\"ttft_ms\": "
      << inferlab_plan::fixed6(record.ttft_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string summary_json(const Plan& plan, const std::size_t indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2U, ' ');
  const auto& best = plan.records.at(plan.best_record_index);
  const auto& riskiest = plan.records.at(plan.riskiest_record_index);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"active_record_count\": "
      << plan.active_record_count << ",\n";
  if (plan.role == ArtifactRole::batching) {
    out << nested << "\"best_scenario\": "
        << inferlab_plan::json_string(best.scenario.scenario_id) << ",\n";
    out << nested << "\"best_scheduler_deep_dive_score\": "
        << inferlab_plan::fixed6(best.scheduler_deep_dive_score)
        << ",\n";
    out << nested << "\"comparable_record_count\": "
        << plan.comparable_record_count << ",\n";
    out << nested << "\"riskiest_scenario\": "
        << inferlab_plan::json_string(riskiest.scenario.scenario_id)
        << ",\n";
    out << nested << "\"riskiest_slo_violation_risk\": "
        << inferlab_plan::fixed6(riskiest.slo_violation_risk) << ",\n";
    out << nested << "\"scenario_count\": " << plan.records.size()
        << "\n";
  } else {
    out << nested << "\"best_request_id\": "
        << inferlab_plan::json_string(best.scenario.scenario_id) << ",\n";
    out << nested << "\"best_scheduler_deep_dive_score\": "
        << inferlab_plan::fixed6(best.scheduler_deep_dive_score)
        << ",\n";
    out << nested << "\"comparable_record_count\": "
        << plan.comparable_record_count << ",\n";
    out << nested << "\"request_count\": " << plan.records.size()
        << ",\n";
    out << nested << "\"riskiest_p99_risk\": "
        << inferlab_plan::fixed6(riskiest.slo_violation_risk) << ",\n";
    out << nested << "\"riskiest_request_id\": "
        << inferlab_plan::json_string(riskiest.scenario.scenario_id)
        << ",\n";
    out << nested << "\"riskiest_root_cause\": "
        << inferlab_plan::json_string(root_cause_name(riskiest.root_cause))
        << "\n";
  }
  out << pad << "}";
  return out.str();
}

void validate_plan_for_view(const Plan& plan,
                            const ArtifactContract& contract) {
  if (plan.role != contract.role || plan.assumptions.role != plan.role) {
    throw std::invalid_argument(
        "scheduler deep-dive plan and artifact roles disagree");
  }
  if (plan.records.empty() ||
      plan.best_record_index >= plan.records.size() ||
      plan.riskiest_record_index >= plan.records.size() ||
      plan.active_record_count > plan.records.size() ||
      plan.comparable_record_count > plan.records.size() ||
      plan.comparable_record_count == 0U) {
    throw std::invalid_argument(
        "scheduler deep-dive plan summary is invalid");
  }
  std::vector<std::string> scenario_ids;
  scenario_ids.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    scenario_ids.push_back(record.scenario.scenario_id);
  }
  const inferlab_native_trace::Domain trace_domain =
      plan.role == ArtifactRole::batching
          ? inferlab_native_trace::Domain::scheduler_batching
          : inferlab_native_trace::Domain::scheduler_root_cause;
  inferlab_native_trace::validate_trace_context_alignment(
      plan.trace_context, trace_domain, scenario_ids);
}

}  // namespace

const ArtifactContract& artifact_contract(const ArtifactRole role) {
  switch (role) {
    case ArtifactRole::batching:
      return batching_contract();
    case ArtifactRole::root_cause:
      return root_cause_contract();
  }
  throw std::invalid_argument("scheduler deep-dive artifact role is invalid");
}

std::string artifact_json(const Plan& plan) {
  const auto& contract = artifact_contract(plan.role);
  validate_plan_for_view(plan, contract);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, contract.artifact_type, contract.claim_scope,
      contract.product_alignment, contract.target_name,
      contract.artifact_path, contract.refresh_command);
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(contract.artifact_type) << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"deep_dive_type\": "
      << inferlab_plan::json_string(contract.deep_dive_type) << ",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(contract.claim_scope) << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"fixture_basis\": \"compiled_analytical_fixture\",\n";
  out << "  \"metric_evidence_level\": \"modeled_not_measured\",\n";
  out << "  \"reference_config_consumed\": false,\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"chunked_prefill_mechanism\": \"inferlab::estimate_chunked_prefill_scheduler\",\n";
  out << "    \"deep_dive_model\": \"scheduler_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"artifact_projection\": \"scheduler_deep_dive_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"claim_boundary\": {\n";
  out << "    \"allowed\": \"compiled native scheduler mechanism explanation\",\n";
  out << "    \"denied\": [\"observed_ttft_tpot_p99\", \"product_scheduler_behavior\", \"production_admission_decision\", \"benchmark_or_performance_claim\", \"release_readiness\"]\n";
  out << "  },\n";
  out << "  \"model_assumptions\": "
      << assumptions_json(plan.assumptions, 2U) << ",\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(contract.research_basis, 2)
      << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(contract.product_alignment, 2)
      << ",\n";
  out << "  \"summary\": " << summary_json(plan, 2U) << ",\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(contract.evidence_bridge_inputs,
                                          2)
      << ",\n";
  out << "  \"native_trace_context\": "
      << inferlab_native_trace::trace_context_projection_json(
             plan.trace_context, 2)
      << ",\n";
  out << "  \"decision\": \"product_replay_or_execute_evidence_required\",\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const std::string rendered =
        plan.role == ArtifactRole::batching
            ? batching_record_json(plan.records[index], 4U)
            : root_cause_record_json(plan.records[index], 4U);
    out << rendered
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({contract.refresh_command}, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::scheduler_deep_dive
