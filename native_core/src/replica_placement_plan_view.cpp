#include "replica_placement_plan_view.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab::replica_placement_view {
namespace {

int nested_indent(const int indent, const char* name) {
  return internal::checked_add_int(indent, 2, name);
}

std::string score_weights_json(
    const ReplicaPlacementScoreWeights& weights, const int indent) {
  const int nested =
      nested_indent(indent, "replica placement score weights indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"cold_load_fit\": "
      << inferlab_plan::fixed6(weights.cold_load_fit) << ",\n";
  out << nested_pad << "\"lora_hit\": "
      << inferlab_plan::fixed6(weights.lora_hit) << ",\n";
  out << nested_pad << "\"memory_headroom\": "
      << inferlab_plan::fixed6(weights.memory_headroom) << ",\n";
  out << nested_pad << "\"prefix_hit\": "
      << inferlab_plan::fixed6(weights.prefix_hit) << ",\n";
  out << nested_pad << "\"qps_fit\": "
      << inferlab_plan::fixed6(weights.qps_fit) << ",\n";
  out << nested_pad << "\"rebalance_fit\": "
      << inferlab_plan::fixed6(weights.rebalance_fit) << ",\n";
  out << nested_pad << "\"replica_coverage\": "
      << inferlab_plan::fixed6(weights.replica_coverage) << ",\n";
  out << nested_pad << "\"slo_fit\": "
      << inferlab_plan::fixed6(weights.slo_fit) << ",\n";
  out << nested_pad << "\"tenant_fit\": "
      << inferlab_plan::fixed6(weights.tenant_fit) << "\n";
  out << pad << "}";
  return out.str();
}

std::string risk_weights_json(
    const ReplicaPlacementRiskWeights& weights, const int indent) {
  const int nested =
      nested_indent(indent, "replica placement risk weights indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"adapter_fault\": "
      << inferlab_plan::fixed6(weights.adapter_fault) << ",\n";
  out << nested_pad << "\"cold_load\": "
      << inferlab_plan::fixed6(weights.cold_load) << ",\n";
  out << nested_pad << "\"lora_miss\": "
      << inferlab_plan::fixed6(weights.lora_miss) << ",\n";
  out << nested_pad << "\"memory_pressure\": "
      << inferlab_plan::fixed6(weights.memory_pressure) << ",\n";
  out << nested_pad << "\"prefix_miss\": "
      << inferlab_plan::fixed6(weights.prefix_miss) << ",\n";
  out << nested_pad << "\"qps_pressure\": "
      << inferlab_plan::fixed6(weights.qps_pressure) << ",\n";
  out << nested_pad << "\"rebalance\": "
      << inferlab_plan::fixed6(weights.rebalance) << ",\n";
  out << nested_pad << "\"replica_shortage\": "
      << inferlab_plan::fixed6(weights.replica_shortage) << ",\n";
  out << nested_pad << "\"slo_overrun\": "
      << inferlab_plan::fixed6(weights.slo_overrun) << ",\n";
  out << nested_pad << "\"tenant_pressure\": "
      << inferlab_plan::fixed6(weights.tenant_pressure) << "\n";
  out << pad << "}";
  return out.str();
}

std::string assumptions_json(
    const ReplicaPlacementAssumptions& assumptions, const int indent) {
  const int nested =
      nested_indent(indent, "replica placement assumptions indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"adapter_fault_ms_per_gib\": "
      << inferlab_plan::fixed6(assumptions.adapter_fault_ms_per_gib)
      << ",\n";
  out << nested_pad << "\"adapter_fault_risk_cap_ms\": "
      << inferlab_plan::fixed6(
             assumptions.adapter_fault_risk_cap_ms)
      << ",\n";
  out << nested_pad << "\"adapter_remaining_memory_share\": "
      << inferlab_plan::fixed6(
             assumptions.adapter_remaining_memory_share)
      << ",\n";
  out << nested_pad << "\"cold_load_score_cap_ms\": "
      << inferlab_plan::fixed6(assumptions.cold_load_score_cap_ms)
      << ",\n";
  out << nested_pad << "\"gpus_per_replica\": "
      << assumptions.gpus_per_replica << ",\n";
  out << nested_pad << "\"lora_hit_decision_floor\": "
      << inferlab_plan::fixed6(assumptions.lora_hit_decision_floor)
      << ",\n";
  out << nested_pad << "\"lora_residency_hit_gain\": "
      << inferlab_plan::fixed6(assumptions.lora_residency_hit_gain)
      << ",\n";
  out << nested_pad << "\"memory_headroom_recommendation_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.memory_headroom_recommendation_threshold)
      << ",\n";
  out << nested_pad << "\"memory_pressure_decision_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.memory_pressure_decision_threshold)
      << ",\n";
  out << nested_pad << "\"model_load_fixed_ms\": "
      << inferlab_plan::fixed6(assumptions.model_load_fixed_ms)
      << ",\n";
  out << nested_pad << "\"model_load_ms_per_gib\": "
      << inferlab_plan::fixed6(assumptions.model_load_ms_per_gib)
      << ",\n";
  out << nested_pad << "\"model_runtime_overhead_ratio\": "
      << inferlab_plan::fixed6(
             assumptions.model_runtime_overhead_ratio)
      << ",\n";
  out << nested_pad << "\"p99_risk_decision_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.p99_risk_decision_threshold)
      << ",\n";
  out << nested_pad << "\"placement_score_recommendation_floor\": "
      << inferlab_plan::fixed6(
             assumptions.placement_score_recommendation_floor)
      << ",\n";
  out << nested_pad << "\"prefix_hit_decision_floor\": "
      << inferlab_plan::fixed6(
             assumptions.prefix_hit_decision_floor)
      << ",\n";
  out << nested_pad << "\"prefix_remaining_memory_share\": "
      << inferlab_plan::fixed6(
             assumptions.prefix_remaining_memory_share)
      << ",\n";
  out << nested_pad << "\"prefix_residency_hit_gain\": "
      << inferlab_plan::fixed6(
             assumptions.prefix_residency_hit_gain)
      << ",\n";
  out << nested_pad << "\"profile_name\": "
      << inferlab_plan::json_string(assumptions.profile_name) << ",\n";
  out << nested_pad << "\"rebalance_bandwidth_gbps\": "
      << inferlab_plan::fixed6(assumptions.rebalance_bandwidth_gbps)
      << ",\n";
  out << nested_pad << "\"rebalance_recommendation_threshold_ms\": "
      << inferlab_plan::fixed6(
             assumptions.rebalance_recommendation_threshold_ms)
      << ",\n";
  out << nested_pad << "\"rebalance_risk_cap_ms\": "
      << inferlab_plan::fixed6(assumptions.rebalance_risk_cap_ms)
      << ",\n";
  out << nested_pad << "\"rebalance_score_cap_ms\": "
      << inferlab_plan::fixed6(assumptions.rebalance_score_cap_ms)
      << ",\n";
  out << nested_pad << "\"reference_qps_per_replica\": "
      << inferlab_plan::fixed6(
             assumptions.reference_qps_per_replica)
      << ",\n";
  out << nested_pad << "\"reference_tenants_per_replica\": "
      << inferlab_plan::fixed6(
             assumptions.reference_tenants_per_replica)
      << ",\n";
  out << nested_pad << "\"reserved_memory_headroom_ratio\": "
      << inferlab_plan::fixed6(
             assumptions.reserved_memory_headroom_ratio)
      << ",\n";
  out << nested_pad << "\"risk_weights\": "
      << risk_weights_json(assumptions.risk_weights, nested) << ",\n";
  out << nested_pad << "\"score_weights\": "
      << score_weights_json(assumptions.score_weights, nested) << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const ReplicaPlacementScenario& scenario,
                          const int indent) {
  const int nested =
      nested_indent(indent, "replica placement scenario indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"adapter_gb\": "
      << inferlab_plan::fixed6(scenario.adapter_gib) << ",\n";
  out << nested_pad << "\"adapter_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"adapter_gib\": "
      << inferlab_plan::fixed6(scenario.adapter_gib) << ",\n";
  out << nested_pad << "\"gpu_count\": " << scenario.gpu_count
      << ",\n";
  out << nested_pad << "\"gpu_memory_gb\": "
      << inferlab_plan::fixed6(scenario.gpu_memory_gib) << ",\n";
  out << nested_pad << "\"gpu_memory_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"gpu_memory_gib\": "
      << inferlab_plan::fixed6(scenario.gpu_memory_gib) << ",\n";
  out << nested_pad << "\"hot_adapter_count\": "
      << scenario.hot_adapter_count << ",\n";
  out << nested_pad << "\"hot_prefix_gb\": "
      << inferlab_plan::fixed6(scenario.hot_prefix_gib) << ",\n";
  out << nested_pad << "\"hot_prefix_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"hot_prefix_gib\": "
      << inferlab_plan::fixed6(scenario.hot_prefix_gib) << ",\n";
  out << nested_pad << "\"lora_reuse_rate\": "
      << inferlab_plan::fixed6(scenario.lora_reuse_rate) << ",\n";
  out << nested_pad << "\"model_weight_gb\": "
      << inferlab_plan::fixed6(scenario.model_weight_gib) << ",\n";
  out << nested_pad << "\"model_weight_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"model_weight_gib\": "
      << inferlab_plan::fixed6(scenario.model_weight_gib) << ",\n";
  out << nested_pad << "\"modeled_p99_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.modeled_p99_ttft_ms)
      << ",\n";
  out << nested_pad << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested_pad << "\"observed_p99_ttft_ms\": "
      << inferlab_plan::fixed6(scenario.modeled_p99_ttft_ms)
      << ",\n";
  out << nested_pad << "\"observed_p99_ttft_ms_scope\": "
      << inferlab_plan::json_string(
             "legacy alias for compiled modeled_p99_ttft_ms; no observation occurred")
      << ",\n";
  out << nested_pad << "\"p99_ttft_slo_ms\": "
      << inferlab_plan::fixed6(scenario.p99_ttft_slo_ms) << ",\n";
  out << nested_pad << "\"placement_policy\": "
      << inferlab_plan::json_string(
             replica_placement_policy_name(scenario.policy))
      << ",\n";
  out << nested_pad << "\"prefix_reuse_rate\": "
      << inferlab_plan::fixed6(scenario.prefix_reuse_rate) << ",\n";
  out << nested_pad << "\"request_qps\": "
      << inferlab_plan::fixed6(scenario.request_qps) << ",\n";
  out << nested_pad << "\"rollout_churn_rate\": "
      << inferlab_plan::fixed6(scenario.rollout_churn_rate) << ",\n";
  out << nested_pad << "\"target_replicas\": "
      << scenario.target_replicas << ",\n";
  out << nested_pad << "\"tenant_count\": "
      << scenario.tenant_count << "\n";
  out << pad << "}";
  return out.str();
}

std::string policy_profile_json(
    const ReplicaPlacementPolicyProfile& profile, const int indent) {
  const int nested =
      nested_indent(indent, "replica placement policy profile indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"local_prefix_fraction\": "
      << inferlab_plan::fixed6(profile.local_prefix_fraction) << ",\n";
  out << nested_pad << "\"lora_hit_bias\": "
      << inferlab_plan::fixed6(profile.lora_hit_bias) << ",\n";
  out << nested_pad << "\"policy\": "
      << inferlab_plan::json_string(
             replica_placement_policy_name(profile.policy))
      << ",\n";
  out << nested_pad << "\"prefill_gpu_denominator\": "
      << profile.prefill_gpu_denominator << ",\n";
  out << nested_pad << "\"prefill_gpu_numerator\": "
      << profile.prefill_gpu_numerator << ",\n";
  out << nested_pad << "\"prefix_hit_bias\": "
      << inferlab_plan::fixed6(profile.prefix_hit_bias) << ",\n";
  out << nested_pad << "\"rebalance_penalty_ms\": "
      << inferlab_plan::fixed6(profile.rebalance_penalty_ms) << ",\n";
  out << nested_pad << "\"requires_drain_metrics\": "
      << inferlab_plan::json_bool(profile.requires_drain_metrics)
      << ",\n";
  out << nested_pad << "\"requires_remote_kv_metrics\": "
      << inferlab_plan::json_bool(
             profile.requires_remote_kv_metrics)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string worker_pool_json(const ReplicaWorkerPoolPlacement& pool,
                             const int indent) {
  const int nested =
      nested_indent(indent, "replica placement worker pool indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"gpu_count\": " << pool.gpu_count << ",\n";
  out << nested_pad << "\"kind\": "
      << inferlab_plan::json_string(
             replica_worker_pool_kind_name(pool.kind))
      << ",\n";
  out << nested_pad << "\"prefix_shard_gib\": "
      << inferlab_plan::fixed6(pool.prefix_shard_gib) << ",\n";
  out << nested_pad << "\"remote_kv_enabled\": "
      << inferlab_plan::json_bool(pool.remote_kv_enabled) << ",\n";
  out << nested_pad << "\"resident_replicas\": "
      << pool.resident_replicas << ",\n";
  out << nested_pad << "\"warm_adapter_slots\": "
      << pool.warm_adapter_slots << "\n";
  out << pad << "}";
  return out.str();
}

std::string worker_pools_json(
    const std::vector<ReplicaWorkerPoolPlacement>& pools,
    const int indent) {
  const int nested =
      nested_indent(indent, "replica placement worker pools indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < pools.size(); ++index) {
    out << worker_pool_json(pools[index], nested)
        << (index + 1U == pools.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::vector<std::string> legacy_worker_pool_layout(
    const std::vector<ReplicaWorkerPoolPlacement>& pools) {
  std::vector<std::string> layout;
  layout.reserve(pools.size());
  for (const auto& pool : pools) {
    std::ostringstream value;
    if (pool.kind == ReplicaWorkerPoolKind::prefill) {
      value << "prefill_pool:gpus=" << pool.gpu_count
            << ",resident_replicas=" << pool.resident_replicas
            << ",prefix_shard_gb="
            << inferlab_plan::fixed6(pool.prefix_shard_gib);
    } else if (pool.kind == ReplicaWorkerPoolKind::decode) {
      value << "decode_pool:gpus=" << pool.gpu_count
            << ",resident_replicas=" << pool.resident_replicas
            << ",warm_adapter_slots=" << pool.warm_adapter_slots;
    } else {
      value << "remote_kv_pool:store=cpu_nvme_rdma,prefix_spill=enabled";
    }
    layout.push_back(value.str());
  }
  return layout;
}

std::string isolation_key_text(
    const std::vector<std::string>& fields) {
  std::ostringstream out;
  for (std::size_t index = 0; index < fields.size(); ++index) {
    if (index != 0U) out << " + ";
    out << fields[index];
  }
  return out.str();
}

std::string result_json(const ReplicaPlacementPlan& plan,
                        const ReplicaPlacementResult& result,
                        const int indent) {
  const int nested =
      nested_indent(indent, "replica placement result indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested_pad = inferlab_plan::json_indent(nested);
  const auto legacy_layout =
      legacy_worker_pool_layout(result.worker_pools);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested_pad << "\"adapter_fault_ms\": "
      << inferlab_plan::fixed6(result.adapter_fault_ms) << ",\n";
  out << nested_pad << "\"cold_model_load_ms\": "
      << inferlab_plan::fixed6(result.cold_model_load_ms) << ",\n";
  out << nested_pad << "\"expected_lora_hit_rate\": "
      << inferlab_plan::fixed6(result.expected_lora_hit_rate)
      << ",\n";
  out << nested_pad << "\"expected_prefix_hit_rate\": "
      << inferlab_plan::fixed6(result.expected_prefix_hit_rate)
      << ",\n";
  out << nested_pad << "\"gpu_memory_pressure\": "
      << inferlab_plan::fixed6(result.gpu_memory_pressure) << ",\n";
  out << nested_pad << "\"gpu_memory_used_gb\": "
      << inferlab_plan::fixed6(result.gpu_memory_used_gib) << ",\n";
  out << nested_pad << "\"isolation_key\": "
      << inferlab_plan::json_string(
             isolation_key_text(plan.isolation_key_fields))
      << ",\n";
  out << nested_pad << "\"migration_commands\": "
      << inferlab_plan::json_string_array(
             result.migration_commands, nested)
      << ",\n";
  out << nested_pad << "\"p99_risk_score\": "
      << inferlab_plan::fixed6(result.p99_risk_score) << ",\n";
  out << nested_pad << "\"placement_decision\": "
      << inferlab_plan::json_string(
             replica_placement_decision_name(result.decision))
      << ",\n";
  out << nested_pad << "\"placement_score\": "
      << inferlab_plan::fixed6(result.placement_score) << ",\n";
  out << nested_pad << "\"prefix_shard_gb\": "
      << inferlab_plan::fixed6(result.prefix_shard_gib) << ",\n";
  out << nested_pad << "\"rebalance_gb\": "
      << inferlab_plan::fixed6(result.rebalance_gib) << ",\n";
  out << nested_pad << "\"rebalance_window_ms\": "
      << inferlab_plan::fixed6(result.rebalance_window_ms) << ",\n";
  out << nested_pad << "\"recommendation\": "
      << inferlab_plan::json_string(
             replica_placement_recommendation_text(
                 result.recommendation))
      << ",\n";
  out << nested_pad << "\"required_metrics\": "
      << inferlab_plan::json_string_array(
             result.required_metrics, nested)
      << ",\n";
  out << nested_pad << "\"resident_replicas\": "
      << result.resident_replicas << ",\n";
  out << nested_pad << "\"scenario\": "
      << scenario_json(result.scenario, nested) << ",\n";
  out << nested_pad << "\"warm_adapter_slots\": "
      << result.warm_adapter_slots << ",\n";
  out << nested_pad << "\"worker_pool_layout\": "
      << inferlab_plan::json_string_array(legacy_layout, nested)
      << ",\n";
  out << nested_pad << "\"active_workload\": "
      << inferlab_plan::json_bool(result.active_workload) << ",\n";
  out << nested_pad << "\"cold_adapter_count\": "
      << result.cold_adapter_count << ",\n";
  out << nested_pad << "\"expected_lora_hit_rate_uncapped\": "
      << inferlab_plan::fixed6(
             result.expected_lora_hit_rate_uncapped)
      << ",\n";
  out << nested_pad << "\"expected_prefix_hit_rate_uncapped\": "
      << inferlab_plan::fixed6(
             result.expected_prefix_hit_rate_uncapped)
      << ",\n";
  out << nested_pad << "\"gpu_memory_used_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"gpu_memory_used_gib\": "
      << inferlab_plan::fixed6(result.gpu_memory_used_gib) << ",\n";
  out << nested_pad << "\"lora_cache_applicable\": "
      << inferlab_plan::json_bool(result.lora_cache_applicable)
      << ",\n";
  out << nested_pad << "\"metric_evidence_level\": "
      << inferlab_plan::json_string("modeled_not_measured") << ",\n";
  out << nested_pad << "\"missing_replicas\": "
      << result.missing_replicas << ",\n";
  out << nested_pad << "\"modeled_qps_capacity\": "
      << inferlab_plan::fixed6(result.modeled_qps_capacity) << ",\n";
  out << nested_pad << "\"modeled_tenant_capacity\": "
      << inferlab_plan::fixed6(result.modeled_tenant_capacity)
      << ",\n";
  out << nested_pad << "\"placement_score_scope\": "
      << inferlab_plan::json_string(
             "bounded weighted score over compiled placement assumptions; comparable only among active fixture records")
      << ",\n";
  out << nested_pad << "\"policy_profile\": "
      << policy_profile_json(result.policy_profile, nested) << ",\n";
  out << nested_pad << "\"prefix_cache_applicable\": "
      << inferlab_plan::json_bool(result.prefix_cache_applicable)
      << ",\n";
  out << nested_pad << "\"prefix_shard_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"prefix_shard_gib\": "
      << inferlab_plan::fixed6(result.prefix_shard_gib) << ",\n";
  out << nested_pad << "\"qps_capacity_available\": "
      << inferlab_plan::json_bool(result.qps_capacity_available)
      << ",\n";
  out << nested_pad << "\"qps_pressure\": "
      << inferlab_plan::fixed6(result.qps_pressure) << ",\n";
  out << nested_pad << "\"qps_pressure_uncapped\": "
      << inferlab_plan::fixed6(result.qps_pressure_uncapped)
      << ",\n";
  out << nested_pad << "\"qps_pressure_uncapped_scope\": "
      << inferlab_plan::json_string(
             "ratio is defined only when qps_capacity_available; zero is a non-applicable placeholder and bounded pressure fails closed to one for positive demand without capacity")
      << ",\n";
  out << nested_pad << "\"rebalance_gb_scope\": "
      << inferlab_plan::json_string(
             "legacy field name containing binary GiB for compatibility")
      << ",\n";
  out << nested_pad << "\"rebalance_gib\": "
      << inferlab_plan::fixed6(result.rebalance_gib) << ",\n";
  out << nested_pad << "\"replica_coverage\": "
      << inferlab_plan::fixed6(result.replica_coverage) << ",\n";
  out << nested_pad << "\"replica_footprint_fits_gpu_group\": "
      << inferlab_plan::json_bool(
             result.replica_footprint_fits_gpu_group)
      << ",\n";
  out << nested_pad << "\"replica_footprint_gib\": "
      << inferlab_plan::fixed6(result.replica_footprint_gib)
      << ",\n";
  out << nested_pad << "\"usable_gpu_group_memory_gib\": "
      << inferlab_plan::fixed6(
             result.usable_gpu_group_memory_gib)
      << ",\n";
  out << nested_pad << "\"replica_reserved_memory_gib\": "
      << inferlab_plan::fixed6(
             result.replica_reserved_memory_gib)
      << ",\n";
  out << nested_pad << "\"remaining_usable_memory_gib\": "
      << inferlab_plan::fixed6(
             result.remaining_usable_memory_gib)
      << ",\n";
  out << nested_pad << "\"resident_capacity_available\": "
      << inferlab_plan::json_bool(
             result.resident_capacity_available)
      << ",\n";
  out << nested_pad << "\"resident_capacity_from_gpu_inventory\": "
      << result.resident_capacity_from_gpu_inventory << ",\n";
  out << nested_pad << "\"resident_capacity_from_memory\": "
      << result.resident_capacity_from_memory << ",\n";
  out << nested_pad << "\"risk_score_scope\": "
      << inferlab_plan::json_string(
             "bounded weighted risk over compiled fixture inputs; not observed P99, incident probability, or release evidence")
      << ",\n";
  out << nested_pad << "\"slo_overrun_ratio\": "
      << inferlab_plan::fixed6(result.slo_overrun_ratio) << ",\n";
  out << nested_pad << "\"slo_overrun_ratio_uncapped\": "
      << inferlab_plan::fixed6(
             result.slo_overrun_ratio_uncapped)
      << ",\n";
  out << nested_pad << "\"tenant_capacity_available\": "
      << inferlab_plan::json_bool(
             result.tenant_capacity_available)
      << ",\n";
  out << nested_pad << "\"tenant_pressure\": "
      << inferlab_plan::fixed6(result.tenant_pressure) << ",\n";
  out << nested_pad << "\"tenant_pressure_uncapped\": "
      << inferlab_plan::fixed6(result.tenant_pressure_uncapped)
      << ",\n";
  out << nested_pad << "\"tenant_pressure_uncapped_scope\": "
      << inferlab_plan::json_string(
             "ratio is defined only when tenant_capacity_available; zero is a non-applicable placeholder and bounded pressure fails closed to one for positive demand without capacity")
      << ",\n";
  out << nested_pad << "\"total_gpu_memory_gib\": "
      << inferlab_plan::fixed6(result.total_gpu_memory_gib)
      << ",\n";
  out << nested_pad << "\"usable_gpu_memory_gib\": "
      << inferlab_plan::fixed6(result.usable_gpu_memory_gib)
      << ",\n";
  out << nested_pad << "\"worker_pools\": "
      << worker_pools_json(result.worker_pools, nested) << "\n";
  out << pad << "}";
  return out.str();
}

std::string records_json(const ReplicaPlacementPlan& plan,
                         const int indent) {
  const int nested =
      nested_indent(indent, "replica placement records indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    out << result_json(plan, plan.records[index], nested)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace

std::string artifact_json(const ReplicaPlacementPlan& plan) {
  validate_replica_placement_plan(plan);
  const auto& artifact = inferlab_high_value::require_managed_artifact(
      "replica_placement_plan");
  const std::string artifact_type =
      inferlab_catalog::registry_artifact_type(artifact);
  const std::string artifact_path =
      inferlab_catalog::raw_artifact_path(artifact.artifact);
  const auto& best = plan.records[plan.best_record_index];
  const auto& riskiest = plan.records[plan.riskiest_record_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_type) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, artifact_type, artifact.claim_boundary, plan.research_basis,
      artifact.target, artifact_path, artifact.command);
  out << "  \"status\": \"ok\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(artifact.claim_boundary) << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"fixture_basis\": \"compiled replica placement scenarios\",\n";
  out << "  \"metric_evidence_level\": \"modeled_not_measured\",\n";
  out << "  \"scheduler_placement_executed\": false,\n";
  out << "  \"cache_residency_observed\": false,\n";
  out << "  \"gateway_routing_observed\": false,\n";
  out << "  \"kubernetes_commands_executed\": false,\n";
  out << "  \"reference_config\": \"configs/replica_placement.example.json\",\n";
  out << "  \"reference_config_consumed\": false,\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << ",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"public_contract\": \"advanced_serving_deployment_planning.hpp\",\n";
  out << "    \"fixture\": \"advanced_serving_replica_placement_fixture.cpp\",\n";
  out << "    \"validation_analysis_policy_decision_summary\": \"advanced_serving_replica_placement.cpp\",\n";
  out << "    \"artifact_projection\": \"replica_placement_plan_view.hpp/.cpp\",\n";
  out << "    \"transfer_time\": \"advanced_serving_runtime_communication_internal.hpp::transfer_milliseconds\",\n";
  out << "    \"product_identity\": \"mature_product_profile_registry.hpp\",\n";
  out << "    \"cli_request_output\": \"replica_placement_plan_main.cpp\"\n";
  out << "  },\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(plan.research_basis, 2)
      << ",\n";
  out << "  \"research_basis_scope\": "
      << inferlab_plan::json_string(
             "mature product and external research alignment only; no product runtime was executed or reimplemented")
      << ",\n";
  out << "  \"catalog_evidence_contract\": "
      << inferlab_plan::catalog_evidence_contract_json(
             "dry_run_ready", 1U, artifact.claim_boundary,
             artifact.command, 2)
      << ",\n";
  out << "  \"isolation_key_fields\": "
      << inferlab_plan::json_string_array(
             plan.isolation_key_fields, 2)
      << ",\n";
  out << "  \"model_assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"action_required_scenario_count\": "
      << plan.action_required_record_count << ",\n";
  out << "    \"active_scenario_count\": "
      << plan.active_record_count << ",\n";
  out << "    \"best_placement_policy\": "
      << inferlab_plan::json_string(
             replica_placement_policy_name(best.scenario.policy))
      << ",\n";
  out << "    \"best_placement_score\": "
      << inferlab_plan::fixed6(plan.best_placement_score) << ",\n";
  out << "    \"capacity_blocked_scenario_count\": "
      << plan.capacity_blocked_record_count << ",\n";
  out << "    \"decision\": "
      << inferlab_plan::json_string(
             replica_placement_plan_decision_name(plan.decision))
      << ",\n";
  out << "    \"ranking_scope\": "
      << inferlab_plan::json_string(
             "stable first tie among active compiled fixture records only")
      << ",\n";
  out << "    \"riskiest_scenario\": "
      << inferlab_plan::json_string(riskiest.scenario.name) << ",\n";
  out << "    \"riskiest_p99_score\": "
      << inferlab_plan::fixed6(plan.riskiest_p99_score) << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": " << records_json(plan, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({artifact.command}, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::replica_placement_view
