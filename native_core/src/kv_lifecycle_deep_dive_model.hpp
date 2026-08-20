#pragma once

#include "inferlab/advanced_serving_precision.hpp"
#include "inferlab/advanced_serving_reuse_cache.hpp"
#include "inferlab/advanced_serving_runtime_communication.hpp"
#include "kv_paged_attention_deep_dive_model.hpp"
#include "kv_tier_decision_deep_dive_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::kv_lifecycle_deep_dive {

enum class CachePolicy {
  radix_prefix_keep,
  strict_prefix_only,
  paged_lru_with_tier_gate,
};

enum class TransferVsPrefillDecision {
  hbm_residency_avoids_transfer_and_prefill,
  transfer_or_restore_beats_prefill_recompute,
  prefill_recompute_is_safer_than_transfer,
};

enum class Recommendation {
  ready_for_replay_alignment,
  require_correctness_contract,
  refresh_component_artifacts_before_release_claim,
};

struct Assumptions {
  std::string fixture_name =
      "compiled_kv_lifecycle_composition_fixture_v2";
  int prefix_reference_requests = 1024;
  double radix_keep_hit_threshold = 0.35;
  double maximum_correctness_risk = 0.15;
  double replay_alignment_score_threshold = 0.72;
  double prefix_sharing_weight = 0.25;
  double fragmentation_weight = 0.20;
  double read_efficiency_weight = 0.15;
  double tier_value_weight = 0.20;
  double correctness_weight = 0.20;
  kv_paged_attention_deep_dive::Assumptions paged;
  kv_tier_decision_deep_dive::Assumptions tier;
  KvCompressionBlendAssumptions compression;
  KvTransportReplayAssumptions transport;
};

struct Scenario {
  std::string name;
  std::vector<std::string> product_profile_ids;
  kv_paged_attention_deep_dive::Scenario paged;
  double hbm_free_gib = 0.0;
  kv_tier_decision_deep_dive::TransferPath cpu_dram;
  kv_tier_decision_deep_dive::TransferPath nvme;
  kv_tier_decision_deep_dive::TransferPath remote;
  std::string prefix_backend;
  double prefix_reuse_rate = 0.0;
  double prefix_node_overhead_bytes = 96.0;
  double prefix_lookup_us_per_node = 0.5;
  KvCompressionBlendStrategy compression_strategy =
      KvCompressionBlendStrategy::exact_prefix_reuse;
  int compression_reusable_tokens = 0;
  double compression_ratio = 1.0;
  double restore_ms_per_1k_tokens = 0.0;
  double rag_context_overlap = 0.0;
  double correctness_risk = 0.0;
  double transport_compute_overlap_ms = 0.0;
  double transport_retry_risk = 0.0;
};

struct ScoreComponents {
  double prefix_sharing = 0.0;
  double fragmentation_efficiency = 0.0;
  double read_efficiency = 0.0;
  double tier_value = 0.0;
  double correctness = 0.0;
};

struct Result {
  Scenario scenario;
  kv_paged_attention_deep_dive::Result paged;
  RadixPrefixCacheResult prefix;
  KvCompressionBlendResult compression;
  kv_tier_decision_deep_dive::Result tier;
  KvTransportReplayResult transport;
  double transfer_and_restore_ms = 0.0;
  double transfer_vs_prefill_delta_ms = 0.0;
  double maximum_fragmentation_ratio = 0.0;
  ScoreComponents score_components;
  double lifecycle_score = 0.0;
  CachePolicy cache_policy = CachePolicy::paged_lru_with_tier_gate;
  TransferVsPrefillDecision transfer_vs_prefill =
      TransferVsPrefillDecision::prefill_recompute_is_safer_than_transfer;
  Recommendation recommendation =
      Recommendation::refresh_component_artifacts_before_release_claim;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Result> records;
  std::size_t best_record_index = 0U;
  std::size_t worst_read_record_index = 0U;
  std::size_t worst_fragmentation_record_index = 0U;
  std::size_t hbm_selected_count = 0U;
  std::size_t transfer_selected_count = 0U;
  std::size_t recompute_selected_count = 0U;
  double average_lifecycle_score = 0.0;
};

const char* cache_policy_name(CachePolicy policy);
const char* transfer_vs_prefill_decision_name(
    TransferVsPrefillDecision decision);
const char* recommendation_name(Recommendation recommendation);

Assumptions default_assumptions();
std::vector<Scenario> default_scenarios();
Result analyze(const Assumptions& assumptions, const Scenario& scenario);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios);

}  // namespace inferlab::kv_lifecycle_deep_dive
