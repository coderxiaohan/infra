#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace inferlab {

enum class KvOffloadTierKind {
  hbm,
  cpu_dram,
  nvme,
  remote_kv,
};

const char* kv_offload_tier_kind_name(KvOffloadTierKind kind);

enum class KvOffloadRetentionPolicy {
  lru,
  s3fifo,
  priority,
  session_pin,
};

const char* kv_offload_retention_policy_name(
    KvOffloadRetentionPolicy policy);

enum class KvOffloadRecommendation {
  no_offload_required,
  increase_remote_capacity_or_lower_retention,
  prefer_hierarchical_offload_with_prefix_aware_routing,
  pin_hot_agent_sessions_and_spill_cold_prefixes,
  enable_cross_engine_kv_reuse,
  local_dram_offload_is_sufficient,
};

const char* kv_offload_recommendation_text(
    KvOffloadRecommendation recommendation);

struct KvOffloadHierarchyAssumptions {
  std::string profile_name = "compiled_kv_offload_hierarchy_v2";
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  double hbm_session_reservation_weight = 0.55;
  double hierarchy_pressure_multiplier_threshold = 2.0;
  double session_pin_recommendation_threshold = 0.50;
  double cross_engine_reuse_recommendation_threshold = 0.30;
  bool prompt_reuse_priority = true;
  bool one_time_population_transfer = true;
  bool one_access_latency_per_occupied_tier = true;
};

struct KvOffloadPolicyProfile {
  KvOffloadRetentionPolicy policy = KvOffloadRetentionPolicy::lru;
  double retention_multiplier = 0.0;
};

struct KvOffloadTier {
  KvOffloadTierKind kind = KvOffloadTierKind::hbm;
  double capacity_gib = 0.0;
  double bandwidth_gbps = 0.0;
  double access_latency_us = 0.0;
  double cost_per_gib_month = 0.0;
};

struct KvOffloadWorkload {
  std::string name;
  int requests = 0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double prompt_reuse_rate = 0.0;
  double cross_engine_reuse_rate = 0.0;
  double session_pin_rate = 0.0;
  double kv_bytes_per_token = 0.0;
  double prefill_ms_per_token = 0.0;
  int prefill_chunk_tokens = 0;
};

struct KvOffloadPlacement {
  KvOffloadTier tier;
  double usable_capacity_gib = 0.0;
  double placed_gib = 0.0;
  double placed_prompt_reuse_gib = 0.0;
  double placed_session_output_gib = 0.0;
  double transfer_ms = 0.0;
  double access_ms = 0.0;
  double monthly_cost = 0.0;
};

struct KvOffloadHierarchyResult {
  KvOffloadWorkload workload;
  KvOffloadPolicyProfile policy_profile;
  bool active_workload = false;
  bool comparable = false;
  std::int64_t prompt_token_instances = 0;
  std::int64_t output_token_instances = 0;
  std::int64_t total_token_instances = 0;
  double effective_prompt_reuse_rate = 0.0;
  double effective_session_output_reuse_rate = 0.0;
  double prompt_kv_gib = 0.0;
  double output_kv_gib = 0.0;
  double total_kv_gib = 0.0;
  double reusable_prompt_kv_gib = 0.0;
  double reusable_session_output_kv_gib = 0.0;
  double reusable_kv_gib = 0.0;
  double placed_reusable_kv_gib = 0.0;
  double placed_prompt_reuse_kv_gib = 0.0;
  double unplaced_reusable_kv_gib = 0.0;
  double cross_engine_eligible_kv_gib = 0.0;
  double usable_hbm_gib = 0.0;
  double hbm_pressure_gib = 0.0;
  double hbm_saved_gib = 0.0;
  double ideal_saved_prefill_ms = 0.0;
  double saved_prefill_ms = 0.0;
  double transfer_ms = 0.0;
  double access_ms = 0.0;
  double net_ttft_saving_ms = 0.0;
  double monthly_cache_cost = 0.0;
  double reuse_efficiency = 0.0;
  KvOffloadRecommendation recommendation =
      KvOffloadRecommendation::no_offload_required;
  std::vector<KvOffloadPlacement> placements;
};

struct KvOffloadHierarchyPlan {
  KvOffloadHierarchyAssumptions assumptions;
  std::vector<KvOffloadTier> tiers;
  std::vector<KvOffloadPolicyProfile> policy_profiles;
  std::vector<KvOffloadWorkload> workloads;
  std::vector<KvOffloadHierarchyResult> records;
  std::size_t active_record_count = 0;
  std::size_t comparable_record_count = 0;
  std::optional<std::size_t> best_record_index;
  std::optional<std::size_t> max_hbm_saved_record_index;
};

KvOffloadHierarchyAssumptions default_kv_offload_hierarchy_assumptions();

const std::vector<KvOffloadPolicyProfile>& kv_offload_policy_profiles();

std::vector<KvOffloadTier> default_kv_offload_tiers(
    double hbm_gib = 24.0,
    double dram_gib = 256.0,
    double nvme_gib = 2048.0,
    double remote_gib = 8192.0);

std::vector<KvOffloadWorkload> default_kv_offload_workloads();

KvOffloadHierarchyPlan build_kv_offload_hierarchy_plan(
    const KvOffloadHierarchyAssumptions& assumptions,
    const std::vector<KvOffloadTier>& tiers,
    const std::vector<KvOffloadWorkload>& workloads,
    const std::vector<KvOffloadPolicyProfile>& policy_profiles);

}  // namespace inferlab
