#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab {

struct AdvancedRequest {
  int request_id = 0;
  std::string prefix_id;
  int prompt_tokens = 0;
  int output_tokens = 0;
};

struct RoutingDecision {
  int request_id = 0;
  std::string policy;
  int worker_id = 0;
  bool cache_hit = false;
  int reused_tokens = 0;
  int recomputed_tokens = 0;
  int worker_load_after = 0;
};

struct RoutingSummary {
  std::string policy;
  int worker_count = 0;
  int request_count = 0;
  int cache_hits = 0;
  int reused_tokens = 0;
  int recomputed_tokens = 0;
  int worker_load_imbalance = 0;
  int peak_worker_load = 0;
  double prefix_hit_rate = 0.0;
  std::vector<RoutingDecision> decisions;
  std::vector<int> final_worker_loads;
};

std::vector<AdvancedRequest> default_advanced_requests();

const std::vector<std::string>& kv_aware_routing_policies();

int maximum_kv_aware_routing_workers();

RoutingSummary simulate_kv_aware_routing(
    const std::string& policy, int worker_count,
    const std::vector<AdvancedRequest>& requests);

enum class GatewayRoutingPolicy {
  round_robin,
  least_loaded,
  prefix_lora_aware,
  slo_rescue_quota,
  overload_shed,
  canary_shadow,
};

enum class GatewayRoutingDecision {
  shed_or_defer_low_priority_requests,
  route_slo_risk_to_rescue_pool,
  prefer_prefix_lora_local_worker,
  prefer_least_loaded_worker,
  route_with_weighted_policy_and_observe,
};

enum class GatewayRoutingRecommendation {
  enable_slo_rescue_pool_and_tenant_shed_guard,
  add_prefix_index_and_cache_aware_gateway_state,
  co_locate_hot_adapters_and_prefix_cache,
  keep_gateway_policy_and_collect_real_trace_artifacts,
};

struct GatewayPolicyMatrixAssumptions {
  std::string profile_name;
  double queue_slots_per_worker = 4.0;
  double adapter_slots_per_worker = 4.0;
  double tenant_count_normalizer = 256.0;
  double prefix_hit_weight = 0.75;
  double prefix_policy_bias_weight = 0.25;
  double lora_hit_weight = 0.70;
  double lora_capacity_headroom_weight = 0.12;
  double lora_policy_bias_weight = 0.18;
  double load_policy_bias_weight = 0.55;
  double load_queue_headroom_weight = 0.25;
  double load_kv_headroom_weight = 0.20;
  double tenant_overage_penalty_weight = 0.65;
  double tenant_scale_penalty_weight = 0.10;
  double slo_overrun_rescue_weight = 0.28;
  double backend_support_rescue_weight = 0.10;
  double overload_shed_divisor = 3.0;
  double non_overload_quota_drop_weight = 0.08;
  double prompt_locality_ttft_saving_ms_per_token = 0.055;
  double lora_affinity_ttft_saving_ms = 42.0;
  double kv_overpressure_ttft_penalty_ms = 120.0;
  double queue_imbalance_ttft_penalty_ms = 1.8;
  double base_control_plane_overhead_ms = 0.18;
  double tenant_lookup_overhead_ms = 0.035;
  double adapter_lookup_overhead_ms = 0.030;
  double slo_overrun_risk_weight = 0.30;
  double queue_pressure_risk_weight = 0.22;
  double kv_pressure_risk_weight = 0.18;
  double fairness_gap_risk_weight = 0.15;
  double backend_gap_risk_weight = 0.10;
  double admission_drop_risk_weight = 0.05;
  double admission_drop_decision_threshold = 0.18;
  double slo_rescue_decision_threshold = 0.70;
  double prefix_locality_decision_threshold = 0.72;
  double load_balance_decision_threshold = 0.70;
  double p99_risk_recommendation_threshold = 0.68;
  double prefix_locality_recommendation_floor = 0.45;
  double lora_affinity_recommendation_floor = 0.50;
};

struct GatewayPolicyProfile {
  GatewayRoutingPolicy policy = GatewayRoutingPolicy::round_robin;
  double cache_bias = 0.0;
  double load_bias = 0.0;
  double slo_rescue_base = 0.0;
  double tenant_fairness_bonus = 0.0;
  double extra_control_plane_overhead_ms = 0.0;
  bool overload_shedding_enabled = false;
};

struct GatewayPolicyScenario {
  std::string name;
  GatewayRoutingPolicy policy = GatewayRoutingPolicy::round_robin;
  int tenant_count = 0;
  int worker_count = 0;
  double request_qps = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double prefix_hit_rate = 0.0;
  double lora_cache_hit_rate = 0.0;
  int hot_adapter_count = 0;
  double queue_depth = 0.0;
  double kv_pressure = 0.0;
  double tenant_quota_utilization = 0.0;
  double compiled_ttft_ms = 0.0;
  double p99_ttft_slo_ms = 1.0;
  double backend_support_score = 0.0;
};

struct GatewayPolicyResult {
  GatewayPolicyScenario scenario;
  GatewayPolicyProfile policy_profile;
  bool active_workload = false;
  double queue_pressure = 0.0;
  double adapter_pressure = 0.0;
  double slo_overrun_ratio = 0.0;
  double tenant_quota_overage = 0.0;
  double kv_overage = 0.0;
  double prefix_locality_score = 0.0;
  double lora_affinity_score = 0.0;
  double load_balance_score = 0.0;
  double tenant_fairness_score = 0.0;
  double slo_rescue_score = 0.0;
  double admission_drop_rate = 0.0;
  double routed_qps = 0.0;
  double shed_qps = 0.0;
  double expected_ttft_delta_ms = 0.0;
  double control_plane_overhead_ms = 0.0;
  double p99_risk_score = 0.0;
  GatewayRoutingDecision decision =
      GatewayRoutingDecision::route_with_weighted_policy_and_observe;
  std::vector<std::string> required_metrics;
  std::vector<std::string> migration_commands;
  GatewayRoutingRecommendation recommendation =
      GatewayRoutingRecommendation::
          keep_gateway_policy_and_collect_real_trace_artifacts;
};

struct GatewayPolicyMatrixPlan {
  GatewayPolicyMatrixAssumptions assumptions;
  std::vector<std::string> routing_key_fields;
  std::string routing_key_policy_expression;
  std::vector<GatewayPolicyResult> records;
  std::size_t active_record_count = 0;
  std::size_t safest_record_index = 0;
  std::size_t best_prefix_locality_record_index = 0;
  double lowest_p99_risk_score = 0.0;
};

const char* gateway_routing_policy_name(GatewayRoutingPolicy policy);
const char* gateway_routing_decision_name(GatewayRoutingDecision decision);
const char* gateway_routing_recommendation_text(
    GatewayRoutingRecommendation recommendation);
const std::vector<GatewayPolicyProfile>& gateway_policy_profiles();
GatewayPolicyMatrixAssumptions default_gateway_policy_matrix_assumptions();
std::vector<GatewayPolicyScenario> default_gateway_policy_scenarios();
GatewayPolicyResult estimate_gateway_policy(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const GatewayPolicyScenario& scenario);
GatewayPolicyMatrixPlan build_gateway_policy_matrix_plan(
    const GatewayPolicyMatrixAssumptions& assumptions,
    const std::vector<GatewayPolicyScenario>& scenarios);

struct DisaggregatedScenario {
  int prefill_workers = 1;
  int decode_workers = 1;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double arrival_qps = 1.0;
  double bandwidth_gbps = 100.0;
};

struct DisaggregatedConstants {
  double prefill_ms_per_token = 0.08;
  double decode_ms_per_token = 1.20;
  double kv_bytes_per_token = 32768.0;
};

struct DisaggregatedResult {
  DisaggregatedScenario scenario;
  double prefill_ms = 0.0;
  double decode_ms = 0.0;
  double kv_transfer_ms = 0.0;
  double queue_penalty_ms = 0.0;
  double ttft_ms = 0.0;
  double tpot_ms = 0.0;
  double end_to_end_ms = 0.0;
  double aggregate_baseline_ms = 0.0;
  double speedup_vs_aggregate = 0.0;
};

DisaggregatedResult estimate_disaggregated_serving(
    const DisaggregatedScenario& scenario,
    const DisaggregatedConstants& constants);

struct SloScenario {
  double qps = 1.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
};

struct SloConstants {
  double ttft_slo_ms = 800.0;
  double tpot_slo_ms = 35.0;
  double gpu_hour_cost = 3.2;
  double decode_tokens_per_second_per_gpu = 800.0;
  double prefill_ms_per_token = 0.08;
};

struct SloCapacityResult {
  SloScenario scenario;
  int prefill_replicas = 1;
  int decode_replicas = 1;
  int required_replicas = 1;
  double estimated_hourly_cost = 0.0;
  double estimated_daily_cost = 0.0;
  double prefill_gpu_ms_per_request = 0.0;
  double decode_tokens_per_second_required = 0.0;
  double replica_utilization = 0.0;
  bool ttft_slo_feasible = true;
  bool tpot_slo_feasible = true;
};

SloCapacityResult estimate_slo_capacity(const SloScenario& scenario,
                                        const SloConstants& constants);

struct LongContextScenario {
  int prompt_tokens = 0;
  int output_tokens = 0;
  int chunk_size = 2048;
  double prefix_cache_hit_rate = 0.0;
  double prefill_ms_per_token = 0.08;
  double decode_ms_per_token = 1.20;
  double kv_bytes_per_token = 32768.0;
  double kv_budget_gib = 24.0;
  std::string name;
};

struct LongContextResult {
  LongContextScenario scenario;
  int chunk_count = 0;
  double full_prefill_ms = 0.0;
  double chunked_prefill_ms = 0.0;
  double prefix_cache_saved_ms = 0.0;
  double ttft_ms = 0.0;
  double decode_ms = 0.0;
  double kv_cache_gb = 0.0;
};

LongContextResult estimate_long_context(const LongContextScenario& scenario);

struct ChunkedPrefillSchedulerScenario {
  std::string name;
  std::string backend;
  int prompt_tokens = 8192;
  int output_tokens = 256;
  int chunk_size = 2048;
  int max_prefill_batch_tokens = 8192;
  int concurrent_decode_streams = 8;
  double prefix_cache_hit_rate = 0.0;
  double prefill_ms_per_token = 0.08;
  double decode_ms_per_token = 1.20;
  double scheduler_tick_ms = 2.0;
  double chunk_launch_overhead_ms = 0.35;
  double kv_bytes_per_token = 32768.0;
  double kv_budget_gb = 24.0;
};

struct ChunkedPrefillSchedulerResult {
  ChunkedPrefillSchedulerScenario scenario;
  int effective_prompt_tokens = 0;
  int chunk_count = 0;
  int prefill_waves = 0;
  double first_chunk_ttft_ms = 0.0;
  double total_prefill_ms = 0.0;
  double scheduler_gap_ms = 0.0;
  double decode_stall_ms = 0.0;
  double prefix_cache_saved_ms = 0.0;
  double kv_cache_gb = 0.0;
  double kv_budget_utilization = 0.0;
  double throughput_efficiency = 0.0;
  double long_context_pressure_score = 0.0;
  double sarathi_stall_free_score = 0.0;
  int decode_maximal_batch_size = 0;
  std::vector<std::string> prefill_chunk_timeline;
  double decode_starvation_risk = 0.0;
  double jain_fairness_estimate = 0.0;
  double p99_risk_score = 0.0;
  std::string chunk_policy;
  std::string recommendation;
};

ChunkedPrefillSchedulerResult estimate_chunked_prefill_scheduler(
    const ChunkedPrefillSchedulerScenario& scenario);

std::vector<DisaggregatedScenario> default_disaggregated_scenarios();
std::vector<SloScenario> default_slo_scenarios();
std::vector<LongContextScenario> default_long_context_scenarios();
std::vector<ChunkedPrefillSchedulerScenario>
default_chunked_prefill_scheduler_scenarios();

}  // namespace inferlab
