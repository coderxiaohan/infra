#pragma once

#include "native_trace_context.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::scheduler_deep_dive {

enum class ArtifactRole {
  batching,
  root_cause,
};

enum class FairnessPolicy {
  queue_tenant_reciprocal,
  quota_active_linear,
};

enum class AdmissionDecision {
  no_active_work,
  admit_and_batch,
  admit,
  admit_with_smaller_chunks,
  shed_or_defer_prefill,
  defer_or_reject,
};

enum class RootCause {
  not_applicable,
  tenant_quota_pressure,
  kv_budget_pressure,
  prefill_chunk_decode_stall,
  queue_depth_admission_delay,
  slo_p99_tail_risk,
  within_native_policy_budget,
};

enum class RecommendedAction {
  no_active_work,
  ready_to_compare_with_product_scheduler_replay,
  reduce_prefill_chunk_or_split_prefill_decode_workers,
  attach_real_ttft_tpot_p99_traces_after_migration,
  apply_tenant_quota_or_fairness_guard_before_admission,
  reduce_kv_pressure_or_route_to_warm_kv_tier,
  defer_low_priority_prefill_or_scale_decode_capacity,
  enable_backpressure_or_replay_product_trace_for_tail_validation,
};

const char* artifact_role_name(ArtifactRole role);
const char* fairness_policy_name(FairnessPolicy policy);
const char* admission_decision_name(AdmissionDecision decision);
const char* root_cause_name(RootCause root_cause);
const char* recommended_action_name(RecommendedAction action);

struct AdmissionScoreWeights {
  double inverse_slo_risk = 0.0;
  double fairness = 0.0;
  double inverse_chunk_pressure = 0.0;
  double inverse_kv_pressure = 0.0;
  double decode_stall_policy = 0.0;
};

struct DeepDiveScoreWeights {
  double admission = 0.0;
  double inverse_slo_risk = 0.0;
  double fairness = 0.0;
  double inverse_decode_stall = 0.0;
  double healthy_root_cause = 0.0;
};

struct Assumptions {
  ArtifactRole role = ArtifactRole::batching;
  std::string profile_name;
  FairnessPolicy fairness_policy =
      FairnessPolicy::queue_tenant_reciprocal;
  double canonical_prefix_cache_hit_rate = 0.0;
  double canonical_chunk_launch_overhead_ms = 0.0;
  double canonical_kv_bytes_per_token = 0.0;
  double canonical_kv_budget_gb = 0.0;
  double queue_base_factor = 0.0;
  double queue_kv_pressure_weight = 0.0;
  double queue_quota_pressure_weight = 0.0;
  double tpot_decode_starvation_weight = 0.0;
  double p99_tenant_tick_weight = 0.0;
  double p99_quota_penalty_ms = 0.0;
  double fairness_queue_depth_weight = 0.0;
  double fairness_tenant_weight = 0.0;
  double fairness_quota_weight = 0.0;
  double fairness_active_request_weight = 0.0;
  double fairness_active_request_penalty_cap = 0.0;
  double ttft_risk_weight = 0.0;
  double p99_risk_weight = 0.0;
  AdmissionScoreWeights admission_weights;
  DeepDiveScoreWeights deep_dive_weights;
  double chunk_pressure_tick_window = 0.0;
  double decode_stall_policy_threshold_ms = 0.0;
  double high_stall_policy_credit = 0.0;
  double unhealthy_root_cause_credit = 0.0;
  double decode_stall_normalization_ms = 0.0;
  double admission_threshold = 0.0;
  double defer_risk_threshold = 0.0;
  double defer_quota_threshold = 0.0;
  double ready_score_threshold = 0.0;
  double recommendation_stall_threshold_ms = 0.0;
  double root_quota_threshold = 0.0;
  double root_kv_threshold = 0.0;
  double root_decode_stall_threshold_ms = 0.0;
  double root_queue_ttft_share_threshold = 0.0;
  double root_slo_risk_threshold = 0.0;
  int max_visible_prefill_chunks = 0;
};

struct Scenario {
  std::string scenario_id;
  std::string tenant_id;
  std::string alignment;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int chunk_tokens = 0;
  int queue_depth = 0;
  int tenant_concurrency = 0;
  int max_batch_tokens = 0;
  double arrival_ms = 0.0;
  double prefill_ms_per_token = 0.0;
  double decode_ms_per_token = 0.0;
  double scheduler_tick_ms = 0.0;
  double kv_pressure_ratio = 0.0;
  double quota_pressure_ratio = 0.0;
  double slo_ttft_ms = 0.0;
  double slo_p99_ms = 0.0;
};

struct TimelineEvent {
  std::string name;
  std::string lane;
  double start_ms = 0.0;
  double duration_ms = 0.0;
};

struct CanonicalChunkedPrefillProjection {
  int effective_prompt_tokens = 0;
  int prefill_chunk_count = 0;
  int prefill_wave_count = 0;
  int decode_maximal_batch_size = 0;
  double first_chunk_ttft_ms = 0.0;
  double total_prefill_ms = 0.0;
  double scheduler_gap_ms = 0.0;
  double decode_stall_ms = 0.0;
  double decode_starvation_risk = 0.0;
  double mechanism_fairness_estimate = 0.0;
  double mechanism_p99_risk = 0.0;
  std::string chunk_policy;
  std::string recommendation;
};

struct Record {
  Scenario scenario;
  CanonicalChunkedPrefillProjection canonical_chunked_prefill;
  std::vector<TimelineEvent> timeline;
  bool active_work = false;
  bool ranking_applicable = false;
  double queue_wait_ms = 0.0;
  double prefill_chunk_ms = 0.0;
  double decode_stall_ms = 0.0;
  double ttft_ms = 0.0;
  double tpot_ms = 0.0;
  double p99_latency_ms = 0.0;
  double ttft_risk = 0.0;
  double p99_tail_risk = 0.0;
  double slo_violation_risk = 0.0;
  double fairness_score = 0.0;
  double inverse_chunk_pressure = 0.0;
  double inverse_kv_pressure = 0.0;
  double decode_stall_policy_score = 0.0;
  double admission_score = 0.0;
  double scheduler_deep_dive_score = 0.0;
  AdmissionDecision admission_decision =
      AdmissionDecision::admit_with_smaller_chunks;
  RootCause root_cause = RootCause::not_applicable;
  RecommendedAction recommended_action =
      RecommendedAction::attach_real_ttft_tpot_p99_traces_after_migration;
};

struct Plan {
  ArtifactRole role = ArtifactRole::batching;
  Assumptions assumptions;
  std::vector<Record> records;
  inferlab_native_trace::Projection trace_context;
  std::size_t active_record_count = 0;
  std::size_t comparable_record_count = 0;
  std::size_t best_record_index = 0;
  std::size_t riskiest_record_index = 0;
};

Assumptions default_assumptions(ArtifactRole role);
std::vector<Scenario> default_scenarios(ArtifactRole role);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios,
                inferlab_native_trace::Source trace_source);

}  // namespace inferlab::scheduler_deep_dive
