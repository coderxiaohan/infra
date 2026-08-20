#include "inferlab/advanced_serving_kv_tier.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

const std::array<KvOffloadTierKind, 4>& required_tier_order() {
  static const std::array<KvOffloadTierKind, 4> kinds = {
      KvOffloadTierKind::hbm,
      KvOffloadTierKind::cpu_dram,
      KvOffloadTierKind::nvme,
      KvOffloadTierKind::remote_kv,
  };
  return kinds;
}

const std::array<KvOffloadRetentionPolicy, 4>& required_policy_order() {
  static const std::array<KvOffloadRetentionPolicy, 4> policies = {
      KvOffloadRetentionPolicy::lru,
      KvOffloadRetentionPolicy::s3fifo,
      KvOffloadRetentionPolicy::priority,
      KvOffloadRetentionPolicy::session_pin,
  };
  return policies;
}

void validate_assumptions(
    const KvOffloadHierarchyAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "KV offload assumption profile");
  require_positive_finite(assumptions.bytes_per_gib,
                          "KV offload bytes per GiB");
  require_unit_interval(assumptions.hbm_session_reservation_weight,
                        "KV offload HBM reservation weight");
  require_non_negative_finite(
      assumptions.hierarchy_pressure_multiplier_threshold,
      "KV offload hierarchy pressure threshold");
  require_unit_interval(
      assumptions.session_pin_recommendation_threshold,
      "KV offload session-pin recommendation threshold");
  require_unit_interval(
      assumptions.cross_engine_reuse_recommendation_threshold,
      "KV offload cross-engine recommendation threshold");
  if (!assumptions.prompt_reuse_priority ||
      !assumptions.one_time_population_transfer ||
      !assumptions.one_access_latency_per_occupied_tier) {
    throw std::invalid_argument(
        "KV offload fixture requires its disclosed placement and timing semantics");
  }
}

void validate_tiers(const std::vector<KvOffloadTier>& tiers) {
  if (tiers.size() != required_tier_order().size()) {
    throw std::invalid_argument(
        "KV offload hierarchy requires exactly four tiers");
  }
  for (std::size_t index = 0; index < tiers.size(); ++index) {
    const auto& tier = tiers[index];
    if (tier.kind != required_tier_order()[index]) {
      throw std::invalid_argument(
          "KV offload tiers must use canonical HBM/DRAM/NVMe/remote order");
    }
    kv_offload_tier_kind_name(tier.kind);
    require_non_negative_finite(tier.capacity_gib,
                                "KV offload tier capacity");
    if (tier.capacity_gib > 0.0) {
      require_positive_finite(tier.bandwidth_gbps,
                              "KV offload tier bandwidth");
    } else {
      require_non_negative_finite(tier.bandwidth_gbps,
                                  "KV offload tier bandwidth");
    }
    require_non_negative_finite(tier.access_latency_us,
                                "KV offload tier access latency");
    require_non_negative_finite(tier.cost_per_gib_month,
                                "KV offload tier monthly cost");
  }
}

void validate_policy_profiles(
    const std::vector<KvOffloadPolicyProfile>& profiles) {
  if (profiles.size() != required_policy_order().size()) {
    throw std::invalid_argument(
        "KV offload hierarchy requires exactly four policy profiles");
  }
  for (std::size_t index = 0; index < profiles.size(); ++index) {
    const auto& profile = profiles[index];
    if (profile.policy != required_policy_order()[index]) {
      throw std::invalid_argument(
          "KV offload policy profiles must use canonical order");
    }
    kv_offload_retention_policy_name(profile.policy);
    require_unit_interval(profile.retention_multiplier,
                          "KV offload retention multiplier");
  }
}

struct ValidatedWorkload {
  KvOffloadWorkload workload;
  bool active = false;
  std::int64_t prompt_token_instances = 0;
  std::int64_t output_token_instances = 0;
  std::int64_t total_token_instances = 0;
};

ValidatedWorkload validate_workload(KvOffloadWorkload workload) {
  require_nonempty(workload.name, "KV offload workload name");
  if (workload.requests < 0 || workload.prompt_tokens < 0 ||
      workload.output_tokens < 0) {
    throw std::invalid_argument(
        "KV offload request and token counts must be non-negative");
  }
  workload.prompt_reuse_rate = require_unit_interval(
      workload.prompt_reuse_rate, "KV offload prompt reuse rate");
  workload.cross_engine_reuse_rate = require_unit_interval(
      workload.cross_engine_reuse_rate,
      "KV offload cross-engine reuse rate");
  workload.session_pin_rate = require_unit_interval(
      workload.session_pin_rate, "KV offload session pin rate");
  workload.kv_bytes_per_token = require_non_negative_finite(
      workload.kv_bytes_per_token, "KV offload bytes per token");
  workload.prefill_ms_per_token = require_non_negative_finite(
      workload.prefill_ms_per_token, "KV offload prefill milliseconds per token");
  if (workload.prefill_chunk_tokens <= 0 ||
      (workload.prompt_tokens > 0 &&
       workload.prefill_chunk_tokens > workload.prompt_tokens)) {
    throw std::invalid_argument(
        "KV offload prefill chunk must fit the prompt token count");
  }

  ValidatedWorkload validated;
  validated.workload = std::move(workload);
  validated.prompt_token_instances = internal::checked_product_i64(
      validated.workload.requests, validated.workload.prompt_tokens,
      "KV offload prompt token instances");
  validated.output_token_instances = internal::checked_product_i64(
      validated.workload.requests, validated.workload.output_tokens,
      "KV offload output token instances");
  validated.total_token_instances = internal::checked_add_i64(
      validated.prompt_token_instances, validated.output_token_instances,
      "KV offload total token instances");
  validated.active = validated.total_token_instances > 0;
  if (validated.active && validated.workload.kv_bytes_per_token <= 0.0) {
    throw std::invalid_argument(
        "active KV offload workload requires positive bytes per token");
  }
  return validated;
}

double token_instances_to_gib(
    const std::int64_t token_instances,
    const double bytes_per_token,
    const KvOffloadHierarchyAssumptions& assumptions,
    const char* name) {
  const double bytes = finite_product(
      static_cast<double>(token_instances), bytes_per_token, name);
  return require_non_negative_finite(bytes / assumptions.bytes_per_gib,
                                     name);
}

double checked_non_negative_add(const double lhs,
                                const double rhs,
                                const char* name) {
  return require_non_negative_finite(
      internal::checked_add_finite_double(lhs, rhs, name), name);
}

double subtract_placed(const double remaining,
                       const double placed,
                       const char* name) {
  if (placed < 0.0 || placed > remaining) {
    throw std::logic_error(std::string(name) +
                           " placement exceeds remaining capacity");
  }
  return placed == remaining
             ? 0.0
             : require_non_negative_finite(remaining - placed, name);
}

double usable_capacity(
    const KvOffloadTier& tier,
    const KvOffloadWorkload& workload,
    const KvOffloadHierarchyAssumptions& assumptions) {
  if (tier.kind != KvOffloadTierKind::hbm) {
    return tier.capacity_gib;
  }
  const double reservation_fraction = finite_product(
      workload.session_pin_rate,
      assumptions.hbm_session_reservation_weight,
      "KV offload HBM reservation fraction");
  return finite_product(
      tier.capacity_gib, 1.0 - reservation_fraction,
      "KV offload usable HBM capacity");
}

KvOffloadRecommendation select_recommendation(
    const KvOffloadHierarchyResult& result,
    const KvOffloadHierarchyAssumptions& assumptions) {
  if (!result.active_workload || result.reusable_kv_gib == 0.0) {
    return KvOffloadRecommendation::no_offload_required;
  }
  if (result.unplaced_reusable_kv_gib > 0.0) {
    return KvOffloadRecommendation::
        increase_remote_capacity_or_lower_retention;
  }
  const bool hierarchy_pressure =
      result.hbm_pressure_gib > 0.0 &&
      (result.usable_hbm_gib == 0.0 ||
       result.hbm_pressure_gib >
           finite_product(
               result.usable_hbm_gib,
               assumptions.hierarchy_pressure_multiplier_threshold,
               "KV offload hierarchy recommendation threshold"));
  if (hierarchy_pressure) {
    return KvOffloadRecommendation::
        prefer_hierarchical_offload_with_prefix_aware_routing;
  }
  if (result.policy_profile.policy ==
          KvOffloadRetentionPolicy::session_pin &&
      result.workload.session_pin_rate >
          assumptions.session_pin_recommendation_threshold) {
    return KvOffloadRecommendation::
        pin_hot_agent_sessions_and_spill_cold_prefixes;
  }
  if (result.workload.cross_engine_reuse_rate >
      assumptions.cross_engine_reuse_recommendation_threshold) {
    return KvOffloadRecommendation::enable_cross_engine_kv_reuse;
  }
  return KvOffloadRecommendation::local_dram_offload_is_sufficient;
}

KvOffloadHierarchyResult estimate_offload(
    const KvOffloadHierarchyAssumptions& assumptions,
    const std::vector<KvOffloadTier>& tiers,
    const ValidatedWorkload& validated,
    const KvOffloadPolicyProfile& profile) {
  KvOffloadHierarchyResult result;
  result.workload = validated.workload;
  result.policy_profile = profile;
  result.active_workload = validated.active;
  result.prompt_token_instances = validated.prompt_token_instances;
  result.output_token_instances = validated.output_token_instances;
  result.total_token_instances = validated.total_token_instances;
  result.effective_prompt_reuse_rate = require_unit_interval(
      finite_product(validated.workload.prompt_reuse_rate,
                     profile.retention_multiplier,
                     "KV offload effective prompt reuse"),
      "KV offload effective prompt reuse");
  result.effective_session_output_reuse_rate = require_unit_interval(
      finite_product(validated.workload.session_pin_rate,
                     profile.retention_multiplier,
                     "KV offload effective session output reuse"),
      "KV offload effective session output reuse");

  result.prompt_kv_gib = token_instances_to_gib(
      validated.prompt_token_instances,
      validated.workload.kv_bytes_per_token, assumptions,
      "KV offload prompt storage");
  result.output_kv_gib = token_instances_to_gib(
      validated.output_token_instances,
      validated.workload.kv_bytes_per_token, assumptions,
      "KV offload output storage");
  result.total_kv_gib = checked_non_negative_add(
      result.prompt_kv_gib, result.output_kv_gib,
      "KV offload total storage");
  result.reusable_prompt_kv_gib = finite_product(
      result.prompt_kv_gib, result.effective_prompt_reuse_rate,
      "KV offload reusable prompt storage");
  result.reusable_session_output_kv_gib = finite_product(
      result.output_kv_gib,
      result.effective_session_output_reuse_rate,
      "KV offload reusable session output storage");
  result.reusable_kv_gib = checked_non_negative_add(
      result.reusable_prompt_kv_gib,
      result.reusable_session_output_kv_gib,
      "KV offload reusable storage");
  result.cross_engine_eligible_kv_gib = finite_product(
      result.reusable_kv_gib,
      validated.workload.cross_engine_reuse_rate,
      "KV offload cross-engine eligible storage");
  result.ideal_saved_prefill_ms = finite_product(
      finite_product(
          static_cast<double>(validated.prompt_token_instances),
          result.effective_prompt_reuse_rate,
          "KV offload ideal reusable prompt tokens"),
      validated.workload.prefill_ms_per_token,
      "KV offload ideal prefill saving");

  double remaining_prompt = result.reusable_prompt_kv_gib;
  double remaining_output = result.reusable_session_output_kv_gib;
  for (const auto& tier : tiers) {
    KvOffloadPlacement placement;
    placement.tier = tier;
    placement.usable_capacity_gib = usable_capacity(
        tier, validated.workload, assumptions);
    if (tier.kind == KvOffloadTierKind::hbm) {
      result.usable_hbm_gib = placement.usable_capacity_gib;
    }

    placement.placed_prompt_reuse_gib =
        std::min(remaining_prompt, placement.usable_capacity_gib);
    const double capacity_after_prompt = subtract_placed(
        placement.usable_capacity_gib,
        placement.placed_prompt_reuse_gib,
        "KV offload remaining tier capacity");
    placement.placed_session_output_gib =
        std::min(remaining_output, capacity_after_prompt);
    placement.placed_gib = checked_non_negative_add(
        placement.placed_prompt_reuse_gib,
        placement.placed_session_output_gib,
        "KV offload tier placement");

    remaining_prompt = subtract_placed(
        remaining_prompt, placement.placed_prompt_reuse_gib,
        "KV offload remaining prompt storage");
    remaining_output = subtract_placed(
        remaining_output, placement.placed_session_output_gib,
        "KV offload remaining session output storage");
    if (tier.kind != KvOffloadTierKind::hbm) {
      placement.transfer_ms =
          runtime_communication_internal::transfer_milliseconds(
              placement.placed_gib, tier.bandwidth_gbps,
              "KV offload tier transfer");
    }
    placement.access_ms = placement.placed_gib == 0.0
                              ? 0.0
                              : require_non_negative_finite(
                                    tier.access_latency_us / 1000.0,
                                    "KV offload tier access milliseconds");
    placement.monthly_cost = finite_product(
        placement.placed_gib, tier.cost_per_gib_month,
        "KV offload tier monthly cost");

    result.placed_reusable_kv_gib = checked_non_negative_add(
        result.placed_reusable_kv_gib, placement.placed_gib,
        "KV offload placed storage");
    result.placed_prompt_reuse_kv_gib = checked_non_negative_add(
        result.placed_prompt_reuse_kv_gib,
        placement.placed_prompt_reuse_gib,
        "KV offload placed prompt storage");
    result.transfer_ms = checked_non_negative_add(
        result.transfer_ms, placement.transfer_ms,
        "KV offload aggregate transfer");
    result.access_ms = checked_non_negative_add(
        result.access_ms, placement.access_ms,
        "KV offload aggregate access latency");
    result.monthly_cache_cost = checked_non_negative_add(
        result.monthly_cache_cost, placement.monthly_cost,
        "KV offload aggregate monthly cost");
    if (tier.kind != KvOffloadTierKind::hbm) {
      result.hbm_saved_gib = checked_non_negative_add(
          result.hbm_saved_gib, placement.placed_gib,
          "KV offload HBM saved storage");
    }
    result.placements.push_back(std::move(placement));
  }

  result.unplaced_reusable_kv_gib = checked_non_negative_add(
      remaining_prompt, remaining_output,
      "KV offload unplaced reusable storage");
  result.hbm_pressure_gib = result.reusable_kv_gib > result.usable_hbm_gib
                                ? result.reusable_kv_gib -
                                      result.usable_hbm_gib
                                : 0.0;
  const double placed_prompt_fraction =
      result.reusable_prompt_kv_gib == 0.0
          ? 0.0
          : require_unit_interval(
                result.placed_prompt_reuse_kv_gib /
                    result.reusable_prompt_kv_gib,
                "KV offload placed prompt fraction");
  result.saved_prefill_ms = finite_product(
      result.ideal_saved_prefill_ms, placed_prompt_fraction,
      "KV offload placed prefill saving");
  result.net_ttft_saving_ms = require_finite(
      result.saved_prefill_ms - result.transfer_ms - result.access_ms,
      "KV offload net TTFT saving");
  result.reuse_efficiency = result.saved_prefill_ms == 0.0
                                ? 0.0
                                : require_finite(
                                      result.net_ttft_saving_ms /
                                          result.saved_prefill_ms,
                                      "KV offload reuse efficiency");
  result.comparable = result.active_workload &&
                      result.saved_prefill_ms > 0.0;
  result.recommendation = select_recommendation(result, assumptions);
  return result;
}

}  // namespace

const char* kv_offload_tier_kind_name(const KvOffloadTierKind kind) {
  switch (kind) {
    case KvOffloadTierKind::hbm:
      return "hbm";
    case KvOffloadTierKind::cpu_dram:
      return "cpu_dram";
    case KvOffloadTierKind::nvme:
      return "nvme";
    case KvOffloadTierKind::remote_kv:
      return "remote_kv";
  }
  throw std::logic_error("unknown KV offload tier kind");
}

const char* kv_offload_retention_policy_name(
    const KvOffloadRetentionPolicy policy) {
  switch (policy) {
    case KvOffloadRetentionPolicy::lru:
      return "lru";
    case KvOffloadRetentionPolicy::s3fifo:
      return "s3fifo";
    case KvOffloadRetentionPolicy::priority:
      return "priority";
    case KvOffloadRetentionPolicy::session_pin:
      return "session_pin";
  }
  throw std::logic_error("unknown KV offload retention policy");
}

const char* kv_offload_recommendation_text(
    const KvOffloadRecommendation recommendation) {
  switch (recommendation) {
    case KvOffloadRecommendation::no_offload_required:
      return "no KV offload required for this workload and policy";
    case KvOffloadRecommendation::
        increase_remote_capacity_or_lower_retention:
      return "increase remote_kv capacity or lower retention";
    case KvOffloadRecommendation::
        prefer_hierarchical_offload_with_prefix_aware_routing:
      return "prefer hierarchical offload with prefix-aware routing";
    case KvOffloadRecommendation::
        pin_hot_agent_sessions_and_spill_cold_prefixes:
      return "pin hot agent sessions and spill cold prefixes";
    case KvOffloadRecommendation::enable_cross_engine_kv_reuse:
      return "enable cross-engine KV reuse";
    case KvOffloadRecommendation::local_dram_offload_is_sufficient:
      return "local DRAM offload is sufficient";
  }
  throw std::logic_error("unknown KV offload recommendation");
}

KvOffloadHierarchyAssumptions
default_kv_offload_hierarchy_assumptions() {
  return {};
}

const std::vector<KvOffloadPolicyProfile>&
kv_offload_policy_profiles() {
  static const std::vector<KvOffloadPolicyProfile> profiles = {
      {KvOffloadRetentionPolicy::lru, 0.82},
      {KvOffloadRetentionPolicy::s3fifo, 0.90},
      {KvOffloadRetentionPolicy::priority, 0.96},
      {KvOffloadRetentionPolicy::session_pin, 0.98},
  };
  validate_policy_profiles(profiles);
  return profiles;
}

std::vector<KvOffloadTier> default_kv_offload_tiers(
    const double hbm_gib,
    const double dram_gib,
    const double nvme_gib,
    const double remote_gib) {
  std::vector<KvOffloadTier> tiers = {
      {KvOffloadTierKind::hbm, hbm_gib, 2400.0, 2.0, 0.0},
      {KvOffloadTierKind::cpu_dram, dram_gib, 64.0, 35.0, 0.35},
      {KvOffloadTierKind::nvme, nvme_gib, 12.0, 120.0, 0.08},
      {KvOffloadTierKind::remote_kv, remote_gib, 25.0, 250.0, 0.18},
  };
  validate_tiers(tiers);
  return tiers;
}

std::vector<KvOffloadWorkload> default_kv_offload_workloads() {
  return {
      {"short_chat_shared_system", 2048, 768, 128, 0.42, 0.12,
       0.20, 32768.0, 0.08, 512},
      {"rag_long_context", 1024, 8192, 256, 0.64, 0.38, 0.28,
       32768.0, 0.08, 2048},
      {"agentic_multi_turn", 1536, 4096, 512, 0.71, 0.44, 0.62,
       32768.0, 0.08, 2048},
      {"tool_call_schema_heavy", 4096, 1536, 96, 0.55, 0.18,
       0.35, 32768.0, 0.08, 1024},
  };
}

KvOffloadHierarchyPlan build_kv_offload_hierarchy_plan(
    const KvOffloadHierarchyAssumptions& assumptions,
    const std::vector<KvOffloadTier>& tiers,
    const std::vector<KvOffloadWorkload>& workloads,
    const std::vector<KvOffloadPolicyProfile>& policy_profiles) {
  validate_assumptions(assumptions);
  validate_tiers(tiers);
  validate_policy_profiles(policy_profiles);
  if (workloads.empty()) {
    throw std::invalid_argument(
        "KV offload hierarchy requires at least one workload");
  }

  std::vector<ValidatedWorkload> validated_workloads;
  validated_workloads.reserve(workloads.size());
  std::vector<std::string> workload_names;
  workload_names.reserve(workloads.size());
  for (const auto& workload : workloads) {
    auto validated = validate_workload(workload);
    workload_names.push_back(validated.workload.name);
    validated_workloads.push_back(std::move(validated));
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      workload_names, "KV offload workload identities");

  KvOffloadHierarchyPlan plan;
  plan.assumptions = assumptions;
  plan.tiers = tiers;
  plan.policy_profiles = policy_profiles;
  plan.workloads.reserve(validated_workloads.size());
  for (const auto& validated : validated_workloads) {
    plan.workloads.push_back(validated.workload);
  }
  const std::size_t expected_record_count = internal::checked_product_size(
      validated_workloads.size(), policy_profiles.size(),
      "KV offload record count");
  plan.records.reserve(expected_record_count);
  for (const auto& validated : validated_workloads) {
    for (const auto& profile : policy_profiles) {
      plan.records.push_back(
          estimate_offload(assumptions, tiers, validated, profile));
    }
  }
  if (plan.records.size() != expected_record_count) {
    throw std::logic_error("KV offload record cardinality drift");
  }

  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    if (record.active_workload) {
      plan.active_record_count = internal::checked_add_size(
          plan.active_record_count, 1U,
          "KV offload active record count");
      if (!plan.max_hbm_saved_record_index.has_value() ||
          record.hbm_saved_gib >
              plan.records[*plan.max_hbm_saved_record_index].hbm_saved_gib) {
        plan.max_hbm_saved_record_index = index;
      }
    }
    if (record.comparable) {
      plan.comparable_record_count = internal::checked_add_size(
          plan.comparable_record_count, 1U,
          "KV offload comparable record count");
      if (!plan.best_record_index.has_value() ||
          record.net_ttft_saving_ms >
              plan.records[*plan.best_record_index].net_ttft_saving_ms) {
        plan.best_record_index = index;
      }
    }
  }
  return plan;
}

}  // namespace inferlab
