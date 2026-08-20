#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab {

struct KserveLlmdManifestScenario {
  std::string name;
  std::string backend;
  std::string model_uri;
  int decode_replicas = 1;
  int prefill_replicas = 0;
  int tensor_parallel_size = 1;
  int gpu_per_replica = 1;
  bool kv_cache_aware_routing = false;
  bool disaggregated_prefill = false;
  double target_qps = 1.0;
  double target_ttft_ms = 1000.0;
  double target_tpot_ms = 80.0;
  int pipeline_parallel_size = 1;
  int accelerators_per_node = 0;
  double accelerator_memory_gib = 0.0;
};

struct KserveLlmdManifestResult {
  KserveLlmdManifestScenario scenario;
  int total_gpu_request = 0;
  int total_replicas = 0;
  double routing_readiness_score = 0.0;
  double slo_fit_score = 0.0;
  double deployment_complexity_score = 0.0;
  std::string service_kind;
  std::string gateway_policy;
  std::string worker_topology;
  std::string yaml_profile;
  std::string deployment_mode;
  std::string docker_compose_guidance;
  std::string recommendation;
};

KserveLlmdManifestResult estimate_kserve_llmd_manifest(
    const KserveLlmdManifestScenario& scenario);

std::vector<KserveLlmdManifestScenario> default_kserve_llmd_manifest_scenarios();

enum class ReplicaPlacementPolicy {
  spread,
  prefix_colocated,
  lora_colocated,
  slo_tiered,
  drain_rebalance,
  remote_overflow,
};

enum class ReplicaPlacementDecision {
  add_capacity_or_reduce_replica_footprint,
  split_hot_tenants_and_add_warm_replicas,
  reduce_prefix_shards_or_spill_remote_kv,
  co_locate_hot_adapters_with_decode_pool,
  pin_hot_prefixes_near_router_affinity,
  keep_current_placement_and_collect_trace,
};

enum class ReplicaPlacementRecommendation {
  add_gpu_capacity_or_reduce_replica_footprint,
  add_capacity_or_reduce_hot_adapter_prefix_residency,
  use_drain_based_incremental_rebalance,
  reserve_memory_headroom_before_canary,
  ship_dry_run_descriptor_and_validate_with_real_gateway_trace,
};

enum class ReplicaPlacementPlanDecision {
  requires_capacity_or_policy_change,
  dry_run_descriptor_ready_for_target_validation,
};

enum class ReplicaWorkerPoolKind {
  prefill,
  decode,
  remote_kv,
};

struct ReplicaPlacementScoreWeights {
  double prefix_hit = 0.18;
  double lora_hit = 0.16;
  double memory_headroom = 0.12;
  double replica_coverage = 0.16;
  double qps_fit = 0.12;
  double tenant_fit = 0.08;
  double cold_load_fit = 0.06;
  double rebalance_fit = 0.05;
  double slo_fit = 0.07;
};

struct ReplicaPlacementRiskWeights {
  double slo_overrun = 0.20;
  double memory_pressure = 0.14;
  double replica_shortage = 0.16;
  double qps_pressure = 0.12;
  double tenant_pressure = 0.10;
  double cold_load = 0.08;
  double adapter_fault = 0.06;
  double rebalance = 0.06;
  double prefix_miss = 0.04;
  double lora_miss = 0.04;
};

struct ReplicaPlacementAssumptions {
  std::string profile_name;
  int gpus_per_replica = 1;
  double model_runtime_overhead_ratio = 0.08;
  double reserved_memory_headroom_ratio = 0.10;
  double adapter_remaining_memory_share = 0.42;
  double prefix_remaining_memory_share = 0.38;
  double prefix_residency_hit_gain = 0.25;
  double lora_residency_hit_gain = 0.22;
  double reference_qps_per_replica = 32.0;
  double reference_tenants_per_replica = 8.0;
  double model_load_ms_per_gib = 42.0;
  double model_load_fixed_ms = 260.0;
  double adapter_fault_ms_per_gib = 22.0;
  double rebalance_bandwidth_gbps = 206.158430208;
  double cold_load_score_cap_ms = 2500.0;
  double adapter_fault_risk_cap_ms = 600.0;
  double rebalance_score_cap_ms = 4000.0;
  double rebalance_risk_cap_ms = 5000.0;
  double p99_risk_decision_threshold = 0.72;
  double memory_pressure_decision_threshold = 0.88;
  double lora_hit_decision_floor = 0.68;
  double prefix_hit_decision_floor = 0.62;
  double placement_score_recommendation_floor = 0.45;
  double rebalance_recommendation_threshold_ms = 3000.0;
  double memory_headroom_recommendation_threshold = 0.82;
  ReplicaPlacementScoreWeights score_weights;
  ReplicaPlacementRiskWeights risk_weights;
};

struct ReplicaPlacementPolicyProfile {
  ReplicaPlacementPolicy policy = ReplicaPlacementPolicy::spread;
  double prefix_hit_bias = 0.0;
  double lora_hit_bias = 0.0;
  double local_prefix_fraction = 1.0;
  double rebalance_penalty_ms = 0.0;
  int prefill_gpu_numerator = 1;
  int prefill_gpu_denominator = 4;
  bool requires_drain_metrics = false;
  bool requires_remote_kv_metrics = false;
};

struct ReplicaPlacementScenario {
  std::string name;
  ReplicaPlacementPolicy policy = ReplicaPlacementPolicy::spread;
  int gpu_count = 0;
  double gpu_memory_gib = 0.0;
  double model_weight_gib = 0.0;
  int target_replicas = 0;
  int tenant_count = 0;
  int hot_adapter_count = 0;
  double adapter_gib = 0.0;
  double hot_prefix_gib = 0.0;
  double request_qps = 0.0;
  double prefix_reuse_rate = 0.0;
  double lora_reuse_rate = 0.0;
  double modeled_p99_ttft_ms = 0.0;
  double p99_ttft_slo_ms = 1.0;
  double rollout_churn_rate = 0.0;
};

struct ReplicaWorkerPoolPlacement {
  ReplicaWorkerPoolKind kind = ReplicaWorkerPoolKind::decode;
  int gpu_count = 0;
  int resident_replicas = 0;
  int warm_adapter_slots = 0;
  double prefix_shard_gib = 0.0;
  bool remote_kv_enabled = false;
};

struct ReplicaPlacementResult {
  ReplicaPlacementScenario scenario;
  ReplicaPlacementPolicyProfile policy_profile;
  bool active_workload = false;
  bool resident_capacity_available = false;
  bool replica_footprint_fits_gpu_group = false;
  double replica_footprint_gib = 0.0;
  double usable_gpu_group_memory_gib = 0.0;
  int resident_capacity_from_memory = 0;
  int resident_capacity_from_gpu_inventory = 0;
  int resident_replicas = 0;
  int missing_replicas = 0;
  int warm_adapter_slots = 0;
  int cold_adapter_count = 0;
  double total_gpu_memory_gib = 0.0;
  double usable_gpu_memory_gib = 0.0;
  double replica_reserved_memory_gib = 0.0;
  double remaining_usable_memory_gib = 0.0;
  double prefix_shard_gib = 0.0;
  double gpu_memory_used_gib = 0.0;
  double gpu_memory_pressure = 0.0;
  bool prefix_cache_applicable = false;
  bool lora_cache_applicable = false;
  double expected_prefix_hit_rate_uncapped = 0.0;
  double expected_prefix_hit_rate = 0.0;
  double expected_lora_hit_rate_uncapped = 0.0;
  double expected_lora_hit_rate = 0.0;
  double modeled_qps_capacity = 0.0;
  bool qps_capacity_available = false;
  double qps_pressure_uncapped = 0.0;
  double qps_pressure = 0.0;
  double modeled_tenant_capacity = 0.0;
  bool tenant_capacity_available = false;
  double tenant_pressure_uncapped = 0.0;
  double tenant_pressure = 0.0;
  double replica_coverage = 0.0;
  double cold_model_load_ms = 0.0;
  double adapter_fault_ms = 0.0;
  double rebalance_gib = 0.0;
  double rebalance_window_ms = 0.0;
  double slo_overrun_ratio_uncapped = 0.0;
  double slo_overrun_ratio = 0.0;
  double placement_score = 0.0;
  double p99_risk_score = 0.0;
  ReplicaPlacementDecision decision =
      ReplicaPlacementDecision::keep_current_placement_and_collect_trace;
  std::vector<ReplicaWorkerPoolPlacement> worker_pools;
  std::vector<std::string> required_metrics;
  std::vector<std::string> migration_commands;
  ReplicaPlacementRecommendation recommendation =
      ReplicaPlacementRecommendation::
          ship_dry_run_descriptor_and_validate_with_real_gateway_trace;
};

struct ReplicaPlacementPlan {
  ReplicaPlacementAssumptions assumptions;
  std::vector<std::string> research_basis;
  std::vector<std::string> isolation_key_fields;
  std::vector<ReplicaPlacementResult> records;
  std::size_t active_record_count = 0;
  std::size_t capacity_blocked_record_count = 0;
  std::size_t action_required_record_count = 0;
  std::size_t best_record_index = 0;
  std::size_t riskiest_record_index = 0;
  double best_placement_score = 0.0;
  double riskiest_p99_score = 0.0;
  ReplicaPlacementPlanDecision decision =
      ReplicaPlacementPlanDecision::requires_capacity_or_policy_change;
};

const char* replica_placement_policy_name(ReplicaPlacementPolicy policy);
const char* replica_placement_decision_name(
    ReplicaPlacementDecision decision);
const char* replica_placement_recommendation_text(
    ReplicaPlacementRecommendation recommendation);
const char* replica_placement_plan_decision_name(
    ReplicaPlacementPlanDecision decision);
const char* replica_worker_pool_kind_name(ReplicaWorkerPoolKind kind);
const std::vector<ReplicaPlacementPolicyProfile>&
replica_placement_policy_profiles();
ReplicaPlacementAssumptions default_replica_placement_assumptions();
std::vector<ReplicaPlacementScenario> default_replica_placement_scenarios();
std::vector<std::string> default_replica_placement_research_basis();
std::vector<std::string> replica_placement_isolation_key_fields();
ReplicaPlacementResult estimate_replica_placement(
    const ReplicaPlacementAssumptions& assumptions,
    const ReplicaPlacementScenario& scenario);
ReplicaPlacementPlan build_replica_placement_plan(
    const ReplicaPlacementAssumptions& assumptions,
    const std::vector<ReplicaPlacementScenario>& scenarios);
void validate_replica_placement_plan(const ReplicaPlacementPlan& plan);

}  // namespace inferlab
