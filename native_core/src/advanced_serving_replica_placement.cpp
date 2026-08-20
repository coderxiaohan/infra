#include "inferlab/advanced_serving_deployment_planning.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

constexpr double kWeightTolerance = 1.0e-9;

std::size_t policy_index(const ReplicaPlacementPolicy policy) {
  switch (policy) {
    case ReplicaPlacementPolicy::spread:
      return 0U;
    case ReplicaPlacementPolicy::prefix_colocated:
      return 1U;
    case ReplicaPlacementPolicy::lora_colocated:
      return 2U;
    case ReplicaPlacementPolicy::slo_tiered:
      return 3U;
    case ReplicaPlacementPolicy::drain_rebalance:
      return 4U;
    case ReplicaPlacementPolicy::remote_overflow:
      return 5U;
  }
  throw std::invalid_argument("unknown replica placement policy");
}

void require_normalized_weight_total(const double total,
                                     const char* name) {
  const double finite_total =
      advanced_serving_internal::require_finite(total, name);
  if (std::fabs(finite_total - 1.0) > kWeightTolerance) {
    throw std::invalid_argument(std::string(name) + " must equal one");
  }
}

double add_weight(const double total, const double weight,
                  const char* name) {
  return internal::checked_add_finite_double(
      total,
      advanced_serving_internal::require_unit_interval(weight, name),
      name);
}

void validate_score_weights(const ReplicaPlacementScoreWeights& weights) {
  double total = 0.0;
  total = add_weight(total, weights.prefix_hit,
                     "replica placement prefix score weight");
  total = add_weight(total, weights.lora_hit,
                     "replica placement LoRA score weight");
  total = add_weight(total, weights.memory_headroom,
                     "replica placement memory score weight");
  total = add_weight(total, weights.replica_coverage,
                     "replica placement replica score weight");
  total = add_weight(total, weights.qps_fit,
                     "replica placement QPS score weight");
  total = add_weight(total, weights.tenant_fit,
                     "replica placement tenant score weight");
  total = add_weight(total, weights.cold_load_fit,
                     "replica placement cold-load score weight");
  total = add_weight(total, weights.rebalance_fit,
                     "replica placement rebalance score weight");
  total = add_weight(total, weights.slo_fit,
                     "replica placement SLO score weight");
  require_normalized_weight_total(total,
                                  "replica placement score weight total");
}

void validate_risk_weights(const ReplicaPlacementRiskWeights& weights) {
  double total = 0.0;
  total = add_weight(total, weights.slo_overrun,
                     "replica placement SLO risk weight");
  total = add_weight(total, weights.memory_pressure,
                     "replica placement memory risk weight");
  total = add_weight(total, weights.replica_shortage,
                     "replica placement shortage risk weight");
  total = add_weight(total, weights.qps_pressure,
                     "replica placement QPS risk weight");
  total = add_weight(total, weights.tenant_pressure,
                     "replica placement tenant risk weight");
  total = add_weight(total, weights.cold_load,
                     "replica placement cold-load risk weight");
  total = add_weight(total, weights.adapter_fault,
                     "replica placement adapter-fault risk weight");
  total = add_weight(total, weights.rebalance,
                     "replica placement rebalance risk weight");
  total = add_weight(total, weights.prefix_miss,
                     "replica placement prefix-miss risk weight");
  total = add_weight(total, weights.lora_miss,
                     "replica placement LoRA-miss risk weight");
  require_normalized_weight_total(total,
                                  "replica placement risk weight total");
}

void validate_assumptions(const ReplicaPlacementAssumptions& assumptions) {
  using advanced_serving_internal::require_non_negative_finite;
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_positive_finite;
  using advanced_serving_internal::require_positive_int;
  using advanced_serving_internal::require_unit_interval;

  require_nonempty(assumptions.profile_name,
                   "replica placement assumption profile");
  require_positive_int(assumptions.gpus_per_replica,
                       "replica placement GPUs per replica");
  require_unit_interval(assumptions.model_runtime_overhead_ratio,
                        "replica placement runtime overhead ratio");
  require_unit_interval(assumptions.reserved_memory_headroom_ratio,
                        "replica placement reserved headroom ratio");
  require_unit_interval(assumptions.adapter_remaining_memory_share,
                        "replica placement adapter memory share");
  require_unit_interval(assumptions.prefix_remaining_memory_share,
                        "replica placement prefix memory share");
  const double allocation_share = internal::checked_add_finite_double(
      assumptions.adapter_remaining_memory_share,
      assumptions.prefix_remaining_memory_share,
      "replica placement remaining-memory allocation share");
  if (allocation_share > 1.0 + kWeightTolerance) {
    throw std::invalid_argument(
        "replica placement remaining-memory shares exceed one");
  }
  require_unit_interval(assumptions.prefix_residency_hit_gain,
                        "replica placement prefix residency gain");
  require_unit_interval(assumptions.lora_residency_hit_gain,
                        "replica placement LoRA residency gain");
  require_positive_finite(assumptions.reference_qps_per_replica,
                          "replica placement reference QPS per replica");
  require_positive_finite(assumptions.reference_tenants_per_replica,
                          "replica placement reference tenants per replica");
  require_non_negative_finite(assumptions.model_load_ms_per_gib,
                              "replica placement model-load ms per GiB");
  require_non_negative_finite(assumptions.model_load_fixed_ms,
                              "replica placement fixed model-load ms");
  require_non_negative_finite(assumptions.adapter_fault_ms_per_gib,
                              "replica placement adapter-fault ms per GiB");
  require_positive_finite(assumptions.rebalance_bandwidth_gbps,
                          "replica placement rebalance bandwidth Gbps");
  require_positive_finite(assumptions.cold_load_score_cap_ms,
                          "replica placement cold-load score cap");
  require_positive_finite(assumptions.adapter_fault_risk_cap_ms,
                          "replica placement adapter-fault risk cap");
  require_positive_finite(assumptions.rebalance_score_cap_ms,
                          "replica placement rebalance score cap");
  require_positive_finite(assumptions.rebalance_risk_cap_ms,
                          "replica placement rebalance risk cap");
  require_unit_interval(assumptions.p99_risk_decision_threshold,
                        "replica placement P99 risk threshold");
  require_unit_interval(assumptions.memory_pressure_decision_threshold,
                        "replica placement memory decision threshold");
  require_unit_interval(assumptions.lora_hit_decision_floor,
                        "replica placement LoRA hit decision floor");
  require_unit_interval(assumptions.prefix_hit_decision_floor,
                        "replica placement prefix hit decision floor");
  require_unit_interval(assumptions.placement_score_recommendation_floor,
                        "replica placement score recommendation floor");
  require_non_negative_finite(
      assumptions.rebalance_recommendation_threshold_ms,
      "replica placement rebalance recommendation threshold");
  require_unit_interval(
      assumptions.memory_headroom_recommendation_threshold,
      "replica placement memory recommendation threshold");
  validate_score_weights(assumptions.score_weights);
  validate_risk_weights(assumptions.risk_weights);
}

void validate_policy_profiles(
    const std::vector<ReplicaPlacementPolicyProfile>& profiles) {
  if (profiles.size() != 6U) {
    throw std::logic_error(
        "replica placement policy profile count drift");
  }
  std::vector<bool> seen(6U, false);
  for (const auto& profile : profiles) {
    const std::size_t index = policy_index(profile.policy);
    if (seen[index]) {
      throw std::logic_error(
          "duplicate replica placement policy profile");
    }
    seen[index] = true;
    advanced_serving_internal::require_unit_interval(
        profile.prefix_hit_bias,
        "replica placement policy prefix bias");
    advanced_serving_internal::require_unit_interval(
        profile.lora_hit_bias,
        "replica placement policy LoRA bias");
    advanced_serving_internal::require_unit_interval(
        profile.local_prefix_fraction,
        "replica placement local prefix fraction");
    advanced_serving_internal::require_non_negative_finite(
        profile.rebalance_penalty_ms,
        "replica placement policy rebalance penalty");
    advanced_serving_internal::require_positive_int(
        profile.prefill_gpu_numerator,
        "replica placement prefill GPU numerator");
    advanced_serving_internal::require_positive_int(
        profile.prefill_gpu_denominator,
        "replica placement prefill GPU denominator");
    if (profile.prefill_gpu_numerator > profile.prefill_gpu_denominator) {
      throw std::logic_error(
          "replica placement prefill GPU fraction exceeds one");
    }
  }
  if (std::find(seen.begin(), seen.end(), false) != seen.end()) {
    throw std::logic_error(
        "replica placement policy profile coverage is incomplete");
  }
}

const ReplicaPlacementPolicyProfile& require_policy_profile(
    const ReplicaPlacementPolicy policy) {
  const auto& profiles = replica_placement_policy_profiles();
  const auto found = std::find_if(
      profiles.begin(), profiles.end(),
      [policy](const ReplicaPlacementPolicyProfile& profile) {
        return profile.policy == policy;
      });
  if (found == profiles.end()) {
    throw std::invalid_argument("unknown replica placement policy");
  }
  return *found;
}

void validate_scenario(const ReplicaPlacementScenario& scenario) {
  using advanced_serving_internal::require_non_negative_finite;
  using advanced_serving_internal::require_non_negative_int;
  using advanced_serving_internal::require_nonempty;
  using advanced_serving_internal::require_positive_finite;
  using advanced_serving_internal::require_positive_int;
  using advanced_serving_internal::require_unit_interval;

  require_nonempty(scenario.name, "replica placement scenario name");
  static_cast<void>(require_policy_profile(scenario.policy));
  require_positive_int(scenario.gpu_count,
                       "replica placement GPU count");
  require_positive_finite(scenario.gpu_memory_gib,
                          "replica placement GPU memory GiB");
  require_positive_finite(scenario.model_weight_gib,
                          "replica placement model weight GiB");
  require_positive_int(scenario.target_replicas,
                       "replica placement target replicas");
  require_non_negative_int(scenario.tenant_count,
                           "replica placement tenant count");
  require_non_negative_int(scenario.hot_adapter_count,
                           "replica placement hot adapter count");
  require_non_negative_finite(scenario.adapter_gib,
                              "replica placement adapter GiB");
  if ((scenario.hot_adapter_count == 0 && scenario.adapter_gib != 0.0) ||
      (scenario.hot_adapter_count > 0 && scenario.adapter_gib <= 0.0)) {
    throw std::invalid_argument(
        "replica placement adapter count and size are inconsistent");
  }
  require_non_negative_finite(scenario.hot_prefix_gib,
                              "replica placement hot prefix GiB");
  require_non_negative_finite(scenario.request_qps,
                              "replica placement request QPS");
  if (scenario.request_qps > 0.0 && scenario.tenant_count == 0) {
    throw std::invalid_argument(
        "replica placement active QPS requires tenant inventory");
  }
  require_unit_interval(scenario.prefix_reuse_rate,
                        "replica placement prefix reuse rate");
  require_unit_interval(scenario.lora_reuse_rate,
                        "replica placement LoRA reuse rate");
  if (scenario.hot_adapter_count == 0 && scenario.lora_reuse_rate != 0.0) {
    throw std::invalid_argument(
        "replica placement LoRA reuse requires hot adapters");
  }
  require_non_negative_finite(scenario.modeled_p99_ttft_ms,
                              "replica placement modeled P99 TTFT");
  if (scenario.request_qps > 0.0 && scenario.modeled_p99_ttft_ms <= 0.0) {
    throw std::invalid_argument(
        "replica placement active QPS requires modeled P99 TTFT");
  }
  require_positive_finite(scenario.p99_ttft_slo_ms,
                          "replica placement P99 TTFT SLO");
  require_unit_interval(scenario.rollout_churn_rate,
                        "replica placement rollout churn rate");
}

int prefill_gpu_count(const ReplicaPlacementScenario& scenario,
                      const ReplicaPlacementPolicyProfile& profile) {
  if (scenario.gpu_count == 1) {
    return 0;
  }
  const int scaled = internal::checked_multiply_int(
      scenario.gpu_count, profile.prefill_gpu_numerator,
      "replica placement prefill GPU scale");
  return std::min(scenario.gpu_count - 1,
                  std::max(1, scaled / profile.prefill_gpu_denominator));
}

double bounded_ratio(const double numerator, const double denominator,
                     const char* name) {
  const double finite_numerator =
      advanced_serving_internal::require_non_negative_finite(numerator, name);
  const double finite_denominator =
      advanced_serving_internal::require_positive_finite(denominator, name);
  return advanced_serving_internal::finite_unit_interval(
      finite_numerator / finite_denominator, name);
}

double capped_reference_ratio(const double value, const double cap,
                              const char* name) {
  return bounded_ratio(value, cap, name);
}

bool same_pool(const ReplicaWorkerPoolPlacement& lhs,
               const ReplicaWorkerPoolPlacement& rhs) {
  return lhs.kind == rhs.kind && lhs.gpu_count == rhs.gpu_count &&
         lhs.resident_replicas == rhs.resident_replicas &&
         lhs.warm_adapter_slots == rhs.warm_adapter_slots &&
         lhs.prefix_shard_gib == rhs.prefix_shard_gib &&
         lhs.remote_kv_enabled == rhs.remote_kv_enabled;
}

bool same_result(const ReplicaPlacementResult& lhs,
                 const ReplicaPlacementResult& rhs) {
  if (lhs.scenario.name != rhs.scenario.name ||
      lhs.scenario.policy != rhs.scenario.policy ||
      lhs.scenario.gpu_count != rhs.scenario.gpu_count ||
      lhs.scenario.gpu_memory_gib != rhs.scenario.gpu_memory_gib ||
      lhs.scenario.model_weight_gib != rhs.scenario.model_weight_gib ||
      lhs.scenario.target_replicas != rhs.scenario.target_replicas ||
      lhs.scenario.tenant_count != rhs.scenario.tenant_count ||
      lhs.scenario.hot_adapter_count != rhs.scenario.hot_adapter_count ||
      lhs.scenario.adapter_gib != rhs.scenario.adapter_gib ||
      lhs.scenario.hot_prefix_gib != rhs.scenario.hot_prefix_gib ||
      lhs.scenario.request_qps != rhs.scenario.request_qps ||
      lhs.scenario.prefix_reuse_rate != rhs.scenario.prefix_reuse_rate ||
      lhs.scenario.lora_reuse_rate != rhs.scenario.lora_reuse_rate ||
      lhs.scenario.modeled_p99_ttft_ms != rhs.scenario.modeled_p99_ttft_ms ||
      lhs.scenario.p99_ttft_slo_ms != rhs.scenario.p99_ttft_slo_ms ||
      lhs.scenario.rollout_churn_rate != rhs.scenario.rollout_churn_rate ||
      lhs.policy_profile.policy != rhs.policy_profile.policy ||
      lhs.policy_profile.prefix_hit_bias !=
          rhs.policy_profile.prefix_hit_bias ||
      lhs.policy_profile.lora_hit_bias != rhs.policy_profile.lora_hit_bias ||
      lhs.policy_profile.local_prefix_fraction !=
          rhs.policy_profile.local_prefix_fraction ||
      lhs.policy_profile.rebalance_penalty_ms !=
          rhs.policy_profile.rebalance_penalty_ms ||
      lhs.policy_profile.prefill_gpu_numerator !=
          rhs.policy_profile.prefill_gpu_numerator ||
      lhs.policy_profile.prefill_gpu_denominator !=
          rhs.policy_profile.prefill_gpu_denominator ||
      lhs.policy_profile.requires_drain_metrics !=
          rhs.policy_profile.requires_drain_metrics ||
      lhs.policy_profile.requires_remote_kv_metrics !=
          rhs.policy_profile.requires_remote_kv_metrics ||
      lhs.active_workload != rhs.active_workload ||
      lhs.resident_capacity_available != rhs.resident_capacity_available ||
      lhs.replica_footprint_fits_gpu_group !=
          rhs.replica_footprint_fits_gpu_group ||
      lhs.replica_footprint_gib != rhs.replica_footprint_gib ||
      lhs.usable_gpu_group_memory_gib !=
          rhs.usable_gpu_group_memory_gib ||
      lhs.resident_capacity_from_memory !=
          rhs.resident_capacity_from_memory ||
      lhs.resident_capacity_from_gpu_inventory !=
          rhs.resident_capacity_from_gpu_inventory ||
      lhs.resident_replicas != rhs.resident_replicas ||
      lhs.missing_replicas != rhs.missing_replicas ||
      lhs.warm_adapter_slots != rhs.warm_adapter_slots ||
      lhs.cold_adapter_count != rhs.cold_adapter_count ||
      lhs.total_gpu_memory_gib != rhs.total_gpu_memory_gib ||
      lhs.usable_gpu_memory_gib != rhs.usable_gpu_memory_gib ||
      lhs.replica_reserved_memory_gib !=
          rhs.replica_reserved_memory_gib ||
      lhs.remaining_usable_memory_gib !=
          rhs.remaining_usable_memory_gib ||
      lhs.prefix_shard_gib != rhs.prefix_shard_gib ||
      lhs.gpu_memory_used_gib != rhs.gpu_memory_used_gib ||
      lhs.gpu_memory_pressure != rhs.gpu_memory_pressure ||
      lhs.prefix_cache_applicable != rhs.prefix_cache_applicable ||
      lhs.lora_cache_applicable != rhs.lora_cache_applicable ||
      lhs.expected_prefix_hit_rate_uncapped !=
          rhs.expected_prefix_hit_rate_uncapped ||
      lhs.expected_prefix_hit_rate != rhs.expected_prefix_hit_rate ||
      lhs.expected_lora_hit_rate_uncapped !=
          rhs.expected_lora_hit_rate_uncapped ||
      lhs.expected_lora_hit_rate != rhs.expected_lora_hit_rate ||
      lhs.modeled_qps_capacity != rhs.modeled_qps_capacity ||
      lhs.qps_capacity_available != rhs.qps_capacity_available ||
      lhs.qps_pressure_uncapped != rhs.qps_pressure_uncapped ||
      lhs.qps_pressure != rhs.qps_pressure ||
      lhs.modeled_tenant_capacity != rhs.modeled_tenant_capacity ||
      lhs.tenant_capacity_available != rhs.tenant_capacity_available ||
      lhs.tenant_pressure_uncapped != rhs.tenant_pressure_uncapped ||
      lhs.tenant_pressure != rhs.tenant_pressure ||
      lhs.replica_coverage != rhs.replica_coverage ||
      lhs.cold_model_load_ms != rhs.cold_model_load_ms ||
      lhs.adapter_fault_ms != rhs.adapter_fault_ms ||
      lhs.rebalance_gib != rhs.rebalance_gib ||
      lhs.rebalance_window_ms != rhs.rebalance_window_ms ||
      lhs.slo_overrun_ratio_uncapped != rhs.slo_overrun_ratio_uncapped ||
      lhs.slo_overrun_ratio != rhs.slo_overrun_ratio ||
      lhs.placement_score != rhs.placement_score ||
      lhs.p99_risk_score != rhs.p99_risk_score ||
      lhs.decision != rhs.decision ||
      lhs.required_metrics != rhs.required_metrics ||
      lhs.migration_commands != rhs.migration_commands ||
      lhs.recommendation != rhs.recommendation ||
      lhs.worker_pools.size() != rhs.worker_pools.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.worker_pools.size(); ++index) {
    if (!same_pool(lhs.worker_pools[index], rhs.worker_pools[index])) {
      return false;
    }
  }
  return true;
}

bool has_capacity_blocker(const ReplicaPlacementResult& result) {
  return result.missing_replicas > 0 ||
         (result.scenario.request_qps > 0.0 &&
          (!result.qps_capacity_available ||
           result.qps_pressure_uncapped > 1.0)) ||
         (result.scenario.tenant_count > 0 &&
          (!result.tenant_capacity_available ||
           result.tenant_pressure_uncapped > 1.0));
}

bool requires_placement_action(const ReplicaPlacementResult& result) {
  return result.decision !=
             ReplicaPlacementDecision::
                 keep_current_placement_and_collect_trace ||
         result.recommendation !=
             ReplicaPlacementRecommendation::
                 ship_dry_run_descriptor_and_validate_with_real_gateway_trace;
}

}  // namespace

const char* replica_placement_policy_name(
    const ReplicaPlacementPolicy policy) {
  switch (policy) {
    case ReplicaPlacementPolicy::spread:
      return "spread";
    case ReplicaPlacementPolicy::prefix_colocated:
      return "prefix_colocated";
    case ReplicaPlacementPolicy::lora_colocated:
      return "lora_colocated";
    case ReplicaPlacementPolicy::slo_tiered:
      return "slo_tiered";
    case ReplicaPlacementPolicy::drain_rebalance:
      return "drain_rebalance";
    case ReplicaPlacementPolicy::remote_overflow:
      return "remote_overflow";
  }
  throw std::invalid_argument("unknown replica placement policy");
}

const char* replica_placement_decision_name(
    const ReplicaPlacementDecision decision) {
  switch (decision) {
    case ReplicaPlacementDecision::
        add_capacity_or_reduce_replica_footprint:
      return "add_capacity_or_reduce_replica_footprint";
    case ReplicaPlacementDecision::
        split_hot_tenants_and_add_warm_replicas:
      return "split_hot_tenants_and_add_warm_replicas";
    case ReplicaPlacementDecision::
        reduce_prefix_shards_or_spill_remote_kv:
      return "reduce_prefix_shards_or_spill_remote_kv";
    case ReplicaPlacementDecision::
        co_locate_hot_adapters_with_decode_pool:
      return "co_locate_hot_adapters_with_decode_pool";
    case ReplicaPlacementDecision::
        pin_hot_prefixes_near_router_affinity:
      return "pin_hot_prefixes_near_router_affinity";
    case ReplicaPlacementDecision::
        keep_current_placement_and_collect_trace:
      return "keep_current_placement_and_collect_trace";
  }
  throw std::invalid_argument("unknown replica placement decision");
}

const char* replica_placement_recommendation_text(
    const ReplicaPlacementRecommendation recommendation) {
  switch (recommendation) {
    case ReplicaPlacementRecommendation::
        add_gpu_capacity_or_reduce_replica_footprint:
      return "add_gpu_capacity_or_reduce_replica_footprint";
    case ReplicaPlacementRecommendation::
        add_capacity_or_reduce_hot_adapter_prefix_residency:
      return "add_capacity_or_reduce_hot_adapter_prefix_residency";
    case ReplicaPlacementRecommendation::
        use_drain_based_incremental_rebalance:
      return "use_drain_based_incremental_rebalance";
    case ReplicaPlacementRecommendation::
        reserve_memory_headroom_before_canary:
      return "reserve_memory_headroom_before_canary";
    case ReplicaPlacementRecommendation::
        ship_dry_run_descriptor_and_validate_with_real_gateway_trace:
      return "ship_dry_run_descriptor_and_validate_with_real_gateway_trace";
  }
  throw std::invalid_argument(
      "unknown replica placement recommendation");
}

const char* replica_placement_plan_decision_name(
    const ReplicaPlacementPlanDecision decision) {
  switch (decision) {
    case ReplicaPlacementPlanDecision::requires_capacity_or_policy_change:
      return "requires_capacity_or_policy_change";
    case ReplicaPlacementPlanDecision::
        dry_run_descriptor_ready_for_target_validation:
      return "dry_run_descriptor_ready_for_target_validation";
  }
  throw std::invalid_argument("unknown replica placement plan decision");
}

const char* replica_worker_pool_kind_name(
    const ReplicaWorkerPoolKind kind) {
  switch (kind) {
    case ReplicaWorkerPoolKind::prefill:
      return "prefill_pool";
    case ReplicaWorkerPoolKind::decode:
      return "decode_pool";
    case ReplicaWorkerPoolKind::remote_kv:
      return "remote_kv_pool";
  }
  throw std::invalid_argument("unknown replica worker pool kind");
}

const std::vector<ReplicaPlacementPolicyProfile>&
replica_placement_policy_profiles() {
  static const std::vector<ReplicaPlacementPolicyProfile> profiles = {
      {ReplicaPlacementPolicy::spread, 0.05, 0.04, 1.00, 0.0,
       1, 4, false, false},
      {ReplicaPlacementPolicy::prefix_colocated, 0.26, 0.04, 1.00, 0.0,
       1, 4, false, false},
      {ReplicaPlacementPolicy::lora_colocated, 0.05, 0.24, 1.00, 0.0,
       1, 4, false, false},
      {ReplicaPlacementPolicy::slo_tiered, 0.18, 0.15, 1.00, 0.0,
       1, 3, true, false},
      {ReplicaPlacementPolicy::drain_rebalance, 0.05, 0.04, 1.00, 77.0,
       1, 4, true, false},
      {ReplicaPlacementPolicy::remote_overflow, 0.12, 0.04, 0.46, 0.0,
       1, 4, false, true},
  };
  static const bool validated = [] {
    validate_policy_profiles(profiles);
    return true;
  }();
  static_cast<void>(validated);
  return profiles;
}

std::vector<std::string> replica_placement_isolation_key_fields() {
  return {"model_id", "adapter_id", "tenant_id",
          "tokenizer_template_hash", "prefix_hash"};
}

namespace {

std::vector<std::string> required_metrics_for(
    const ReplicaPlacementPolicyProfile& profile) {
  std::vector<std::string> metrics = {
      "placement_gpu_memory_used_gb",
      "placement_gpu_memory_pressure",
      "placement_resident_replicas",
      "placement_prefix_shard_gb",
      "placement_modeled_qps_capacity",
      "placement_qps_pressure",
      "placement_tenant_pressure",
      "router_prefix_hit_rate",
      "lora_adapter_cache_hit_rate",
      "model_cold_load_ms",
      "request_p99_ttft_ms",
  };
  if (profile.requires_drain_metrics) {
    metrics.push_back("replica_rebalance_bytes");
    metrics.push_back("drain_inflight_request_count");
  }
  if (profile.requires_remote_kv_metrics) {
    metrics.push_back("remote_prefix_fetch_ms");
    metrics.push_back("remote_kv_store_hit_rate");
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      metrics, "replica placement required metrics");
  return metrics;
}

std::vector<std::string> migration_commands_for(
    const ReplicaPlacementPolicyProfile& profile) {
  std::vector<std::string> commands = {
      "kubectl get llminferenceservice -A -o yaml",
      std::string("curl ${GATEWAY_URL}/inferlab/placement/dry-run?policy=") +
          replica_placement_policy_name(profile.policy),
  };
  if (profile.policy == ReplicaPlacementPolicy::drain_rebalance) {
    commands.push_back("kubectl get node ${DECODE_NODE} -o yaml");
    commands.push_back(
        "kubectl drain ${DECODE_NODE} --dry-run=server");
  }
  if (profile.requires_remote_kv_metrics) {
    commands.push_back("curl ${KVCACHE_URL}/v1/cache/stats");
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      commands, "replica placement migration commands");
  return commands;
}

std::vector<ReplicaWorkerPoolPlacement> worker_pool_layout(
    const ReplicaPlacementAssumptions& assumptions,
    const ReplicaPlacementScenario& scenario,
    const ReplicaPlacementPolicyProfile& profile,
    const int resident_replicas,
    const int warm_adapter_slots,
    const double prefix_shard_gib) {
  const int prefill_gpus = prefill_gpu_count(scenario, profile);
  const int decode_gpus = scenario.gpu_count - prefill_gpus;
  const int prefill_capacity =
      prefill_gpus / assumptions.gpus_per_replica;
  const int decode_capacity = decode_gpus / assumptions.gpus_per_replica;
  const int proportional_product = internal::checked_multiply_int(
      resident_replicas, prefill_gpus,
      "replica placement proportional prefill replicas");
  int prefill_replicas =
      scenario.gpu_count == 0 ? 0 : proportional_product / scenario.gpu_count;
  prefill_replicas = std::min(prefill_replicas, prefill_capacity);
  int decode_replicas = resident_replicas - prefill_replicas;
  if (decode_replicas > decode_capacity) {
    const int moved = decode_replicas - decode_capacity;
    prefill_replicas = internal::checked_add_int(
        prefill_replicas, moved,
        "replica placement prefill replica rebalance");
    decode_replicas = decode_capacity;
  }
  if (prefill_replicas > prefill_capacity || decode_replicas < 0 ||
      internal::checked_add_int(
          prefill_replicas, decode_replicas,
          "replica placement pool replica total") != resident_replicas) {
    throw std::logic_error(
        "replica placement worker-pool replica allocation drift");
  }

  const double prefill_prefix_ratio =
      static_cast<double>(prefill_gpus) /
      static_cast<double>(scenario.gpu_count);
  const double prefill_prefix_gib =
      advanced_serving_internal::finite_product(
          prefix_shard_gib, prefill_prefix_ratio,
          "replica placement prefill prefix GiB");
  const double decode_prefix_gib =
      advanced_serving_internal::require_non_negative_finite(
          prefix_shard_gib - prefill_prefix_gib,
          "replica placement decode prefix GiB");

  const int prefill_warm_adapter_slots =
      decode_replicas == 0 ? warm_adapter_slots : 0;
  const int decode_warm_adapter_slots =
      decode_replicas > 0 ? warm_adapter_slots : 0;
  if (internal::checked_add_int(
          prefill_warm_adapter_slots, decode_warm_adapter_slots,
          "replica placement pool adapter total") != warm_adapter_slots) {
    throw std::logic_error(
        "replica placement worker-pool adapter allocation drift");
  }

  std::vector<ReplicaWorkerPoolPlacement> pools;
  pools.push_back({ReplicaWorkerPoolKind::prefill, prefill_gpus,
                   prefill_replicas, prefill_warm_adapter_slots,
                   prefill_prefix_gib, false});
  pools.push_back({ReplicaWorkerPoolKind::decode, decode_gpus,
                   decode_replicas, decode_warm_adapter_slots,
                   decode_prefix_gib, false});
  if (profile.requires_remote_kv_metrics) {
    pools.push_back({ReplicaWorkerPoolKind::remote_kv, 0, 0, 0, 0.0, true});
  }
  return pools;
}

}  // namespace

ReplicaPlacementResult estimate_replica_placement(
    const ReplicaPlacementAssumptions& assumptions,
    const ReplicaPlacementScenario& scenario) {
  validate_assumptions(assumptions);
  validate_scenario(scenario);
  const ReplicaPlacementPolicyProfile profile =
      require_policy_profile(scenario.policy);

  ReplicaPlacementResult result;
  result.scenario = scenario;
  result.policy_profile = profile;
  result.active_workload =
      scenario.request_qps > 0.0 || scenario.tenant_count > 0 ||
      scenario.hot_adapter_count > 0 || scenario.hot_prefix_gib > 0.0;

  const int prefill_gpus = prefill_gpu_count(scenario, profile);
  const int decode_gpus = scenario.gpu_count - prefill_gpus;
  const int prefill_replica_capacity =
      prefill_gpus / assumptions.gpus_per_replica;
  const int decode_replica_capacity =
      decode_gpus / assumptions.gpus_per_replica;
  result.resident_capacity_from_gpu_inventory = internal::checked_add_int(
      prefill_replica_capacity, decode_replica_capacity,
      "replica placement GPU inventory capacity");

  result.total_gpu_memory_gib = advanced_serving_internal::finite_product(
      static_cast<double>(scenario.gpu_count), scenario.gpu_memory_gib,
      "replica placement total GPU memory GiB");
  result.usable_gpu_memory_gib = advanced_serving_internal::finite_product(
      result.total_gpu_memory_gib,
      1.0 - assumptions.reserved_memory_headroom_ratio,
      "replica placement usable GPU memory GiB");
  result.replica_footprint_gib =
      advanced_serving_internal::finite_product(
          scenario.model_weight_gib,
          1.0 + assumptions.model_runtime_overhead_ratio,
          "replica placement per-replica footprint GiB");
  const double gpu_group_memory_gib =
      advanced_serving_internal::finite_product(
          static_cast<double>(assumptions.gpus_per_replica),
          scenario.gpu_memory_gib,
          "replica placement GPU-group memory GiB");
  result.usable_gpu_group_memory_gib =
      advanced_serving_internal::finite_product(
          gpu_group_memory_gib,
          1.0 - assumptions.reserved_memory_headroom_ratio,
          "replica placement usable GPU-group memory GiB");
  result.replica_footprint_fits_gpu_group =
      result.replica_footprint_gib <= result.usable_gpu_group_memory_gib;
  result.resident_capacity_from_memory =
      result.replica_footprint_fits_gpu_group
          ? internal::checked_non_negative_floor_double_to_int(
                result.usable_gpu_memory_gib / result.replica_footprint_gib,
                "replica placement memory capacity")
          : 0;
  result.resident_replicas =
      std::min(scenario.target_replicas,
               std::min(result.resident_capacity_from_memory,
                        result.resident_capacity_from_gpu_inventory));
  result.resident_capacity_available = result.resident_replicas > 0;
  result.missing_replicas = scenario.target_replicas - result.resident_replicas;

  result.replica_reserved_memory_gib =
      advanced_serving_internal::finite_product(
          static_cast<double>(result.resident_replicas),
          result.replica_footprint_gib,
          "replica placement reserved replica memory GiB");
  result.remaining_usable_memory_gib =
      advanced_serving_internal::require_non_negative_finite(
          result.usable_gpu_memory_gib -
              result.replica_reserved_memory_gib,
          "replica placement remaining usable memory GiB");

  const double adapter_budget_gib =
      advanced_serving_internal::finite_product(
          result.resident_capacity_available
              ? result.remaining_usable_memory_gib
              : 0.0,
          assumptions.adapter_remaining_memory_share,
          "replica placement adapter budget GiB");
  result.warm_adapter_slots =
      scenario.hot_adapter_count == 0
          ? 0
          : std::min(
                scenario.hot_adapter_count,
                internal::checked_non_negative_floor_double_to_int(
                    adapter_budget_gib / scenario.adapter_gib,
                    "replica placement warm adapter slots"));
  result.cold_adapter_count =
      scenario.hot_adapter_count - result.warm_adapter_slots;
  const double warm_adapter_memory_gib =
      advanced_serving_internal::finite_product(
          static_cast<double>(result.warm_adapter_slots),
          scenario.adapter_gib,
          "replica placement warm adapter memory GiB");

  const double local_prefix_demand_gib =
      advanced_serving_internal::finite_product(
          scenario.hot_prefix_gib, profile.local_prefix_fraction,
          "replica placement local prefix demand GiB");
  const double prefix_budget_gib =
      advanced_serving_internal::finite_product(
          result.resident_capacity_available
              ? result.remaining_usable_memory_gib
              : 0.0,
          assumptions.prefix_remaining_memory_share,
          "replica placement prefix budget GiB");
  result.prefix_shard_gib =
      std::min(local_prefix_demand_gib, prefix_budget_gib);

  result.gpu_memory_used_gib = internal::checked_add_finite_double(
      result.replica_reserved_memory_gib,
      warm_adapter_memory_gib,
      "replica placement used GPU memory GiB");
  result.gpu_memory_used_gib = internal::checked_add_finite_double(
      result.gpu_memory_used_gib, result.prefix_shard_gib,
      "replica placement used GPU memory GiB");
  result.gpu_memory_pressure = bounded_ratio(
      result.gpu_memory_used_gib, result.total_gpu_memory_gib,
      "replica placement GPU memory pressure");

  result.prefix_cache_applicable = scenario.hot_prefix_gib > 0.0;
  result.lora_cache_applicable = scenario.hot_adapter_count > 0;
  const double prefix_residency_ratio =
      result.prefix_cache_applicable
          ? bounded_ratio(result.prefix_shard_gib,
                          scenario.hot_prefix_gib,
                          "replica placement prefix residency ratio")
          : 0.0;
  const double lora_residency_ratio =
      result.lora_cache_applicable
          ? bounded_ratio(static_cast<double>(result.warm_adapter_slots),
                          static_cast<double>(scenario.hot_adapter_count),
                          "replica placement LoRA residency ratio")
          : 0.0;
  result.expected_prefix_hit_rate_uncapped =
      internal::checked_add_finite_double(
          scenario.prefix_reuse_rate,
          advanced_serving_internal::finite_product(
              prefix_residency_ratio,
              assumptions.prefix_residency_hit_gain,
              "replica placement prefix residency hit gain"),
          "replica placement uncapped prefix hit rate");
  result.expected_prefix_hit_rate_uncapped =
      internal::checked_add_finite_double(
          result.expected_prefix_hit_rate_uncapped,
          profile.prefix_hit_bias,
          "replica placement uncapped prefix hit rate");
  result.expected_prefix_hit_rate =
      advanced_serving_internal::finite_unit_interval(
          result.expected_prefix_hit_rate_uncapped,
          "replica placement expected prefix hit rate");
  result.expected_lora_hit_rate_uncapped =
      internal::checked_add_finite_double(
          scenario.lora_reuse_rate,
          advanced_serving_internal::finite_product(
              lora_residency_ratio,
              assumptions.lora_residency_hit_gain,
              "replica placement LoRA residency hit gain"),
          "replica placement uncapped LoRA hit rate");
  result.expected_lora_hit_rate_uncapped =
      internal::checked_add_finite_double(
          result.expected_lora_hit_rate_uncapped,
          profile.lora_hit_bias,
          "replica placement uncapped LoRA hit rate");
  result.expected_lora_hit_rate =
      advanced_serving_internal::finite_unit_interval(
          result.expected_lora_hit_rate_uncapped,
          "replica placement expected LoRA hit rate");

  result.modeled_qps_capacity = advanced_serving_internal::finite_product(
      static_cast<double>(result.resident_replicas),
      assumptions.reference_qps_per_replica,
      "replica placement modeled QPS capacity");
  result.qps_capacity_available = result.modeled_qps_capacity > 0.0;
  result.qps_pressure_uncapped =
      scenario.request_qps == 0.0 || !result.qps_capacity_available
          ? 0.0
          : advanced_serving_internal::require_non_negative_finite(
                scenario.request_qps / result.modeled_qps_capacity,
                "replica placement uncapped QPS pressure");
  result.qps_pressure =
      scenario.request_qps == 0.0
          ? 0.0
          : (result.qps_capacity_available
                 ? advanced_serving_internal::finite_unit_interval(
                       result.qps_pressure_uncapped,
                       "replica placement QPS pressure")
                 : 1.0);
  result.modeled_tenant_capacity =
      advanced_serving_internal::finite_product(
          static_cast<double>(result.resident_replicas),
          assumptions.reference_tenants_per_replica,
          "replica placement modeled tenant capacity");
  result.tenant_capacity_available = result.modeled_tenant_capacity > 0.0;
  result.tenant_pressure_uncapped =
      scenario.tenant_count == 0 || !result.tenant_capacity_available
          ? 0.0
          : advanced_serving_internal::require_non_negative_finite(
                static_cast<double>(scenario.tenant_count) /
                    result.modeled_tenant_capacity,
                "replica placement uncapped tenant pressure");
  result.tenant_pressure =
      scenario.tenant_count == 0
          ? 0.0
          : (result.tenant_capacity_available
                 ? advanced_serving_internal::finite_unit_interval(
                       result.tenant_pressure_uncapped,
                       "replica placement tenant pressure")
                 : 1.0);
  result.replica_coverage = bounded_ratio(
      static_cast<double>(result.resident_replicas),
      static_cast<double>(scenario.target_replicas),
      "replica placement replica coverage");

  const double model_load_per_replica_ms =
      internal::checked_add_finite_double(
          advanced_serving_internal::finite_product(
              scenario.model_weight_gib,
              assumptions.model_load_ms_per_gib,
              "replica placement model-load variable milliseconds"),
          assumptions.model_load_fixed_ms,
          "replica placement model-load milliseconds per replica");
  result.cold_model_load_ms = advanced_serving_internal::finite_product(
      static_cast<double>(result.missing_replicas),
      model_load_per_replica_ms,
      "replica placement cold model-load milliseconds");
  const double lora_miss_rate =
      result.lora_cache_applicable
          ? 1.0 - result.expected_lora_hit_rate
          : 0.0;
  result.adapter_fault_ms = advanced_serving_internal::finite_product(
      static_cast<double>(result.cold_adapter_count), scenario.adapter_gib,
      "replica placement cold adapter GiB");
  result.adapter_fault_ms = advanced_serving_internal::finite_product(
      result.adapter_fault_ms, assumptions.adapter_fault_ms_per_gib,
      "replica placement adapter-fault milliseconds");
  result.adapter_fault_ms = advanced_serving_internal::finite_product(
      result.adapter_fault_ms, lora_miss_rate,
      "replica placement adapter-fault milliseconds");

  result.rebalance_gib = advanced_serving_internal::finite_product(
      scenario.rollout_churn_rate, result.gpu_memory_used_gib,
      "replica placement rebalance payload GiB");
  result.rebalance_window_ms =
      runtime_communication_internal::transfer_milliseconds(
          result.rebalance_gib, assumptions.rebalance_bandwidth_gbps,
          "replica placement rebalance transfer milliseconds");
  if (result.rebalance_gib > 0.0) {
    result.rebalance_window_ms = internal::checked_add_finite_double(
        result.rebalance_window_ms, profile.rebalance_penalty_ms,
        "replica placement rebalance window milliseconds");
  }

  result.slo_overrun_ratio_uncapped =
      std::max(0.0,
               advanced_serving_internal::require_finite(
                   scenario.modeled_p99_ttft_ms /
                           scenario.p99_ttft_slo_ms -
                       1.0,
                   "replica placement uncapped SLO overrun"));
  result.slo_overrun_ratio =
      advanced_serving_internal::finite_unit_interval(
          result.slo_overrun_ratio_uncapped,
          "replica placement bounded SLO overrun");

  const double prefix_fit =
      result.prefix_cache_applicable ? result.expected_prefix_hit_rate : 1.0;
  const double lora_fit =
      result.lora_cache_applicable ? result.expected_lora_hit_rate : 1.0;
  const double cold_load_risk = capped_reference_ratio(
      result.cold_model_load_ms, assumptions.cold_load_score_cap_ms,
      "replica placement cold-load score ratio");
  const double adapter_fault_risk = capped_reference_ratio(
      result.adapter_fault_ms, assumptions.adapter_fault_risk_cap_ms,
      "replica placement adapter-fault risk ratio");
  const double rebalance_score_risk = capped_reference_ratio(
      result.rebalance_window_ms, assumptions.rebalance_score_cap_ms,
      "replica placement rebalance score ratio");
  const double rebalance_p99_risk = capped_reference_ratio(
      result.rebalance_window_ms, assumptions.rebalance_risk_cap_ms,
      "replica placement rebalance P99 risk ratio");

  const auto weighted_component = [](const double value,
                                     const double weight,
                                     const char* name) {
    return advanced_serving_internal::finite_product(value, weight, name);
  };
  double score = 0.0;
  score = internal::checked_add_finite_double(
      score,
      weighted_component(prefix_fit, assumptions.score_weights.prefix_hit,
                         "replica placement prefix score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(lora_fit, assumptions.score_weights.lora_hit,
                         "replica placement LoRA score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - result.gpu_memory_pressure,
                         assumptions.score_weights.memory_headroom,
                         "replica placement memory score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(result.replica_coverage,
                         assumptions.score_weights.replica_coverage,
                         "replica placement replica score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - result.qps_pressure,
                         assumptions.score_weights.qps_fit,
                         "replica placement QPS score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - result.tenant_pressure,
                         assumptions.score_weights.tenant_fit,
                         "replica placement tenant score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - cold_load_risk,
                         assumptions.score_weights.cold_load_fit,
                         "replica placement cold-load score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - rebalance_score_risk,
                         assumptions.score_weights.rebalance_fit,
                         "replica placement rebalance score component"),
      "replica placement score");
  score = internal::checked_add_finite_double(
      score,
      weighted_component(1.0 - result.slo_overrun_ratio,
                         assumptions.score_weights.slo_fit,
                         "replica placement SLO score component"),
      "replica placement score");
  result.placement_score =
      advanced_serving_internal::require_unit_interval(
          score, "replica placement score");

  double risk = 0.0;
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(result.slo_overrun_ratio,
                         assumptions.risk_weights.slo_overrun,
                         "replica placement SLO risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(result.gpu_memory_pressure,
                         assumptions.risk_weights.memory_pressure,
                         "replica placement memory risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(1.0 - result.replica_coverage,
                         assumptions.risk_weights.replica_shortage,
                         "replica placement shortage risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(result.qps_pressure,
                         assumptions.risk_weights.qps_pressure,
                         "replica placement QPS risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(result.tenant_pressure,
                         assumptions.risk_weights.tenant_pressure,
                         "replica placement tenant risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(cold_load_risk,
                         assumptions.risk_weights.cold_load,
                         "replica placement cold-load risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(adapter_fault_risk,
                         assumptions.risk_weights.adapter_fault,
                         "replica placement adapter-fault risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(rebalance_p99_risk,
                         assumptions.risk_weights.rebalance,
                         "replica placement rebalance risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(1.0 - prefix_fit,
                         assumptions.risk_weights.prefix_miss,
                         "replica placement prefix-miss risk component"),
      "replica placement risk score");
  risk = internal::checked_add_finite_double(
      risk,
      weighted_component(1.0 - lora_fit,
                         assumptions.risk_weights.lora_miss,
                         "replica placement LoRA-miss risk component"),
      "replica placement risk score");
  result.p99_risk_score =
      advanced_serving_internal::require_unit_interval(
          risk, "replica placement P99 risk score");

  result.decision =
      has_capacity_blocker(result)
          ? ReplicaPlacementDecision::
                add_capacity_or_reduce_replica_footprint
          : (result.p99_risk_score >=
                     assumptions.p99_risk_decision_threshold
                 ? ReplicaPlacementDecision::
                       split_hot_tenants_and_add_warm_replicas
                 : (result.gpu_memory_pressure >=
                            assumptions.memory_pressure_decision_threshold
                        ? ReplicaPlacementDecision::
                              reduce_prefix_shards_or_spill_remote_kv
                        : (result.lora_cache_applicable &&
                                   result.expected_lora_hit_rate <
                                       assumptions.lora_hit_decision_floor
                               ? ReplicaPlacementDecision::
                                     co_locate_hot_adapters_with_decode_pool
                               : (result.prefix_cache_applicable &&
                                          result.expected_prefix_hit_rate <
                                              assumptions.
                                                  prefix_hit_decision_floor
                                      ? ReplicaPlacementDecision::
                                            pin_hot_prefixes_near_router_affinity
                                      : ReplicaPlacementDecision::
                                            keep_current_placement_and_collect_trace))));
  result.recommendation =
      has_capacity_blocker(result)
          ? ReplicaPlacementRecommendation::
                add_gpu_capacity_or_reduce_replica_footprint
          : (result.placement_score <
                     assumptions.placement_score_recommendation_floor
                 ? ReplicaPlacementRecommendation::
                       add_capacity_or_reduce_hot_adapter_prefix_residency
                 : (result.rebalance_window_ms >
                            assumptions.
                                rebalance_recommendation_threshold_ms
                        ? ReplicaPlacementRecommendation::
                              use_drain_based_incremental_rebalance
                        : (result.gpu_memory_pressure >
                                   assumptions.
                                       memory_headroom_recommendation_threshold
                               ? ReplicaPlacementRecommendation::
                                     reserve_memory_headroom_before_canary
                               : ReplicaPlacementRecommendation::
                                     ship_dry_run_descriptor_and_validate_with_real_gateway_trace)));
  result.worker_pools = worker_pool_layout(
      assumptions, scenario, profile, result.resident_replicas,
      result.warm_adapter_slots, result.prefix_shard_gib);
  result.required_metrics = required_metrics_for(profile);
  result.migration_commands = migration_commands_for(profile);
  return result;
}

ReplicaPlacementPlan build_replica_placement_plan(
    const ReplicaPlacementAssumptions& assumptions,
    const std::vector<ReplicaPlacementScenario>& scenarios) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "replica placement scenarios must not be empty");
  }
  std::vector<std::string> scenario_names;
  scenario_names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    validate_scenario(scenario);
    scenario_names.push_back(scenario.name);
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      scenario_names, "replica placement scenario names");

  ReplicaPlacementPlan plan;
  plan.assumptions = assumptions;
  plan.research_basis = default_replica_placement_research_basis();
  plan.isolation_key_fields = replica_placement_isolation_key_fields();
  advanced_serving_internal::require_unique_nonempty_strings(
      plan.research_basis, "replica placement research basis");
  advanced_serving_internal::require_unique_nonempty_strings(
      plan.isolation_key_fields,
      "replica placement isolation key fields");
  plan.records.reserve(scenarios.size());

  bool selected = false;
  for (const auto& scenario : scenarios) {
    plan.records.push_back(
        estimate_replica_placement(assumptions, scenario));
    const std::size_t index = plan.records.size() - 1U;
    const auto& result = plan.records.back();
    if (result.active_workload) {
      plan.active_record_count = internal::checked_add_size(
          plan.active_record_count, 1U,
          "replica placement active record count");
      if (!selected ||
          result.placement_score > plan.best_placement_score) {
        plan.best_record_index = index;
        plan.best_placement_score = result.placement_score;
      }
      if (!selected || result.p99_risk_score > plan.riskiest_p99_score) {
        plan.riskiest_record_index = index;
        plan.riskiest_p99_score = result.p99_risk_score;
      }
      selected = true;
    }
    if (has_capacity_blocker(result)) {
      plan.capacity_blocked_record_count = internal::checked_add_size(
          plan.capacity_blocked_record_count, 1U,
          "replica placement capacity-blocked count");
    }
    if (requires_placement_action(result)) {
      plan.action_required_record_count = internal::checked_add_size(
          plan.action_required_record_count, 1U,
          "replica placement action-required count");
    }
  }
  if (!selected) {
    throw std::invalid_argument(
        "replica placement plan requires an active workload record");
  }
  if (plan.active_record_count > plan.records.size() ||
      plan.capacity_blocked_record_count > plan.records.size() ||
      plan.action_required_record_count > plan.records.size() ||
      plan.best_record_index >= plan.records.size() ||
      plan.riskiest_record_index >= plan.records.size()) {
    throw std::logic_error(
        "replica placement plan summary count drift");
  }
  plan.decision =
      plan.action_required_record_count == 0U
          ? ReplicaPlacementPlanDecision::
                dry_run_descriptor_ready_for_target_validation
          : ReplicaPlacementPlanDecision::requires_capacity_or_policy_change;
  return plan;
}

void validate_replica_placement_plan(
    const ReplicaPlacementPlan& plan) {
  validate_assumptions(plan.assumptions);
  static_cast<void>(replica_placement_plan_decision_name(plan.decision));
  if (plan.records.empty()) {
    throw std::invalid_argument(
        "replica placement plan records must not be empty");
  }
  std::vector<ReplicaPlacementScenario> scenarios;
  scenarios.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    static_cast<void>(replica_placement_policy_name(record.scenario.policy));
    static_cast<void>(replica_placement_decision_name(record.decision));
    static_cast<void>(replica_placement_recommendation_text(
        record.recommendation));
    for (const auto& pool : record.worker_pools) {
      static_cast<void>(replica_worker_pool_kind_name(pool.kind));
    }
    scenarios.push_back(record.scenario);
  }
  const ReplicaPlacementPlan expected =
      build_replica_placement_plan(plan.assumptions, scenarios);
  if (plan.research_basis != expected.research_basis ||
      plan.isolation_key_fields != expected.isolation_key_fields ||
      plan.active_record_count != expected.active_record_count ||
      plan.capacity_blocked_record_count !=
          expected.capacity_blocked_record_count ||
      plan.action_required_record_count !=
          expected.action_required_record_count ||
      plan.best_record_index != expected.best_record_index ||
      plan.riskiest_record_index != expected.riskiest_record_index ||
      plan.best_placement_score != expected.best_placement_score ||
      plan.riskiest_p99_score != expected.riskiest_p99_score ||
      plan.decision != expected.decision ||
      plan.records.size() != expected.records.size()) {
    throw std::invalid_argument(
        "replica placement plan summary does not match canonical model");
  }
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    if (!same_result(plan.records[index], expected.records[index])) {
      throw std::invalid_argument(
          "replica placement record does not match canonical model");
    }
  }
}

}  // namespace inferlab
