#include "kv_lifecycle_deep_dive_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"
#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace inferlab::kv_lifecycle_deep_dive {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unique_nonempty_strings;
using advanced_serving_internal::require_unit_interval;

void validate_assumptions(const Assumptions& assumptions) {
  require_nonempty(assumptions.fixture_name,
                   "KV lifecycle fixture name");
  if (assumptions.prefix_reference_requests <= 0) {
    throw std::invalid_argument(
        "KV lifecycle prefix reference request count must be positive");
  }
  require_unit_interval(assumptions.radix_keep_hit_threshold,
                        "KV lifecycle radix keep threshold");
  require_unit_interval(assumptions.maximum_correctness_risk,
                        "KV lifecycle maximum correctness risk");
  require_unit_interval(
      assumptions.replay_alignment_score_threshold,
      "KV lifecycle replay alignment score threshold");
  if (std::fabs(assumptions.maximum_correctness_risk -
                assumptions.tier.maximum_reuse_correctness_risk) > 1e-12) {
    throw std::invalid_argument(
        "KV lifecycle and KV tier correctness risk thresholds must match");
  }
  if (std::fabs(assumptions.paged.bytes_per_gib -
                assumptions.compression.bytes_per_gib) > 1e-6) {
    throw std::invalid_argument(
        "KV lifecycle component byte-to-GiB constants must match");
  }

  const double prefix_weight = require_unit_interval(
      assumptions.prefix_sharing_weight,
      "KV lifecycle prefix sharing weight");
  const double fragmentation_weight = require_unit_interval(
      assumptions.fragmentation_weight,
      "KV lifecycle fragmentation weight");
  const double read_weight = require_unit_interval(
      assumptions.read_efficiency_weight,
      "KV lifecycle read efficiency weight");
  const double tier_weight = require_unit_interval(
      assumptions.tier_value_weight, "KV lifecycle tier value weight");
  const double correctness_weight = require_unit_interval(
      assumptions.correctness_weight,
      "KV lifecycle correctness weight");
  double weight_sum = internal::checked_add_finite_double(
      prefix_weight, fragmentation_weight,
      "KV lifecycle score weight sum");
  weight_sum = internal::checked_add_finite_double(
      weight_sum, read_weight, "KV lifecycle score weight sum");
  weight_sum = internal::checked_add_finite_double(
      weight_sum, tier_weight, "KV lifecycle score weight sum");
  weight_sum = internal::checked_add_finite_double(
      weight_sum, correctness_weight,
      "KV lifecycle score weight sum");
  if (std::fabs(weight_sum - 1.0) > 1e-12) {
    throw std::invalid_argument(
        "KV lifecycle score weights must sum to one");
  }
}

Scenario validate_scenario(const Assumptions& assumptions,
                           Scenario scenario) {
  require_nonempty(scenario.name, "KV lifecycle scenario name");
  if (scenario.paged.name != scenario.name) {
    throw std::invalid_argument(
        "KV lifecycle and Paged KV scenario identities must match");
  }
  require_unique_nonempty_strings(
      scenario.product_profile_ids,
      "KV lifecycle product profile ids");
  if (scenario.product_profile_ids.empty()) {
    throw std::invalid_argument(
        "KV lifecycle scenario requires product profile foreign keys");
  }
  for (const auto& id : scenario.product_profile_ids) {
    (void)mature_product_profiles::require_product_profile(id);
  }
  require_nonempty(scenario.prefix_backend,
                   "KV lifecycle prefix backend label");
  scenario.prefix_reuse_rate = require_unit_interval(
      scenario.prefix_reuse_rate,
      "KV lifecycle prefix reuse rate");
  scenario.prefix_node_overhead_bytes = require_positive_finite(
      scenario.prefix_node_overhead_bytes,
      "KV lifecycle prefix node overhead");
  scenario.prefix_lookup_us_per_node = require_positive_finite(
      scenario.prefix_lookup_us_per_node,
      "KV lifecycle prefix lookup latency");
  if (scenario.paged.live_tokens <= 0 ||
      scenario.compression_reusable_tokens < 0 ||
      scenario.compression_reusable_tokens > scenario.paged.live_tokens ||
      scenario.compression_reusable_tokens >
          scenario.paged.request_tokens) {
    throw std::invalid_argument(
        "KV lifecycle compression reuse tokens must fit a positive transfer payload");
  }
  scenario.compression_ratio = require_positive_finite(
      scenario.compression_ratio,
      "KV lifecycle compression ratio");
  if (scenario.compression_ratio > 1.0) {
    throw std::invalid_argument(
        "KV lifecycle compression ratio must not exceed one");
  }
  scenario.restore_ms_per_1k_tokens = require_non_negative_finite(
      scenario.restore_ms_per_1k_tokens,
      "KV lifecycle compression restore latency");
  scenario.rag_context_overlap = require_unit_interval(
      scenario.rag_context_overlap,
      "KV lifecycle RAG context overlap");
  scenario.correctness_risk = require_unit_interval(
      scenario.correctness_risk,
      "KV lifecycle correctness risk");
  scenario.transport_compute_overlap_ms = require_non_negative_finite(
      scenario.transport_compute_overlap_ms,
      "KV lifecycle transport compute overlap");
  scenario.transport_retry_risk = require_unit_interval(
      scenario.transport_retry_risk,
      "KV lifecycle transport retry risk");
  scenario.hbm_free_gib = require_non_negative_finite(
      scenario.hbm_free_gib, "KV lifecycle HBM free GiB");
  if (std::fabs(static_cast<double>(scenario.paged.kv_bytes_per_token) -
                assumptions.compression.kv_bytes_per_token) > 1e-6) {
    throw std::invalid_argument(
        "KV lifecycle Paged KV and compression bytes per token must match");
  }
  if (scenario.compression_strategy ==
          KvCompressionBlendStrategy::exact_prefix_reuse &&
      (scenario.compression_reusable_tokens !=
           scenario.paged.shared_prefix_tokens ||
       std::fabs(scenario.compression_ratio - 1.0) > 1e-12)) {
    throw std::invalid_argument(
        "KV lifecycle exact prefix reuse must match the shared prefix and retain full KV precision");
  }
  return scenario;
}

RadixPrefixCacheResult analyze_prefix(
    const Assumptions& assumptions, const Scenario& scenario) {
  RadixPrefixCacheScenario prefix;
  prefix.name = scenario.name;
  prefix.backend = scenario.prefix_backend;
  prefix.requests = assumptions.prefix_reference_requests;
  prefix.avg_prompt_tokens = scenario.paged.request_tokens;
  prefix.shared_prefix_tokens = scenario.paged.shared_prefix_tokens;
  prefix.unique_suffix_tokens =
      scenario.paged.request_tokens - scenario.paged.shared_prefix_tokens;
  prefix.prefix_reuse_rate = scenario.prefix_reuse_rate;
  prefix.node_overhead_bytes = scenario.prefix_node_overhead_bytes;
  prefix.kv_bytes_per_token =
      static_cast<double>(scenario.paged.kv_bytes_per_token);
  prefix.prefill_ms_per_token = scenario.paged.prefill_ms_per_token;
  prefix.radix_lookup_us_per_node =
      scenario.prefix_lookup_us_per_node;
  return estimate_radix_prefix_cache(prefix);
}

KvCompressionBlendResult analyze_compression(
    const Assumptions& assumptions, const Scenario& scenario) {
  KvCompressionBlendScenario compression;
  compression.name = scenario.name;
  compression.strategy = scenario.compression_strategy;
  compression.prompt_tokens = scenario.paged.request_tokens;
  compression.reusable_tokens = scenario.compression_reusable_tokens;
  compression.compression_ratio = scenario.compression_ratio;
  compression.restore_ms_per_1k_tokens =
      scenario.restore_ms_per_1k_tokens;
  compression.prefill_ms_per_token =
      scenario.paged.prefill_ms_per_token;
  compression.rag_context_overlap = scenario.rag_context_overlap;
  compression.correctness_risk = scenario.correctness_risk;
  return estimate_kv_compression_blend(
      assumptions.compression, compression);
}

bool nearly_equal(const double left, const double right) {
  const double scale = std::max({1.0, std::fabs(left), std::fabs(right)});
  return std::fabs(left - right) <= 1e-9 * scale;
}

kv_tier_decision_deep_dive::TransferPath with_restore_latency(
    kv_tier_decision_deep_dive::TransferPath path,
    const double restore_ms) {
  if (path.bandwidth_gbps > 0.0) {
    path.restore_ms = internal::checked_add_finite_double(
        path.restore_ms, restore_ms,
        "KV lifecycle tier restore latency");
  }
  return path;
}

kv_tier_decision_deep_dive::Result analyze_tier(
    const Assumptions& assumptions,
    const Scenario& scenario,
    const kv_paged_attention_deep_dive::Result& paged,
    const RadixPrefixCacheResult& prefix,
    const KvCompressionBlendResult& compression) {
  if (compression.raw_reusable_kv_gib > paged.transfer_payload_gib + 1e-9) {
    throw std::logic_error(
        "KV lifecycle reusable compression payload exceeds the Paged KV transfer payload");
  }
  const double non_reusable_payload_gib = std::max(
      0.0, require_finite(
               paged.transfer_payload_gib -
                   compression.raw_reusable_kv_gib,
               "KV lifecycle non-reusable transfer payload"));
  const double transfer_payload_gib =
      internal::checked_add_finite_double(
          non_reusable_payload_gib,
          compression.compressed_reusable_kv_gib,
          "KV lifecycle compressed transfer payload");
  const double transfer_payload_ratio = require_finite(
      transfer_payload_gib / paged.transfer_payload_gib,
      "KV lifecycle transfer payload ratio");

  kv_tier_decision_deep_dive::Scenario tier;
  tier.name = scenario.name;
  tier.product_profile_ids = scenario.product_profile_ids;
  tier.prompt_tokens = scenario.paged.request_tokens;
  tier.decode_tokens = 0;
  tier.prefill_recompute_tokens =
      scenario.paged.request_tokens - scenario.paged.shared_prefix_tokens;
  tier.kv_bytes_per_token =
      static_cast<double>(scenario.paged.kv_bytes_per_token);
  tier.transfer_payload_ratio = transfer_payload_ratio;
  tier.hbm_free_gib = scenario.hbm_free_gib;
  tier.cpu_dram = with_restore_latency(
      scenario.cpu_dram, compression.restore_latency_ms);
  tier.nvme = with_restore_latency(
      scenario.nvme, compression.restore_latency_ms);
  tier.remote = with_restore_latency(
      scenario.remote, compression.restore_latency_ms);
  tier.prefill_ms_per_token = scenario.paged.prefill_ms_per_token;
  tier.hit_probability = std::max(
      prefix.radix_prefix_hit_rate,
      compression.non_prefix_reuse_rate);
  tier.correctness_risk = compression.quality_risk_score;
  auto result = kv_tier_decision_deep_dive::analyze(
      assumptions.tier, tier);
  if (!nearly_equal(result.kv_gib,
                    static_cast<double>(paged.resident_kv_bytes) /
                        assumptions.paged.bytes_per_gib) ||
      !nearly_equal(result.transfer_payload_gib,
                    transfer_payload_gib) ||
      !nearly_equal(result.prefill_recompute_ms,
                    paged.prefill_recompute_ms)) {
    throw std::logic_error(
        "KV lifecycle Paged KV and tier projections do not reconcile");
  }
  return result;
}

KvTransportReplayBackend transport_backend_for(
    const kv_tier_decision_deep_dive::TierKind tier) {
  switch (tier) {
    case kv_tier_decision_deep_dive::TierKind::cpu_dram_offload:
      return KvTransportReplayBackend::gpu_cpu_pcie;
    case kv_tier_decision_deep_dive::TierKind::nvme_offload:
      return KvTransportReplayBackend::nvme_gds;
    case kv_tier_decision_deep_dive::TierKind::remote_kv_store:
      return KvTransportReplayBackend::rdma;
    case kv_tier_decision_deep_dive::TierKind::hbm_keep:
    case kv_tier_decision_deep_dive::TierKind::prefill_recompute:
      return KvTransportReplayBackend::tcp_fallback;
  }
  throw std::logic_error("unknown KV lifecycle transport tier");
}

KvTransportReplayResult analyze_transport(
    const Assumptions& assumptions,
    const Scenario& scenario,
    const kv_tier_decision_deep_dive::Result& tier) {
  const auto& selected = tier.candidates[tier.selected_candidate_index];
  const bool active = selected.requires_external_system;
  KvTransportReplayScenario transport;
  transport.name = scenario.name;
  transport.transport = transport_backend_for(selected.tier);
  transport.kv_gib = active ? selected.payload_gib : 0.0;
  transport.bandwidth_gbps = active ? selected.bandwidth_gbps : 0.0;
  transport.setup_ms = active ? selected.setup_ms : 0.0;
  transport.sender_wait_ms = active ? selected.sender_wait_ms : 0.0;
  transport.receiver_wait_ms = active ? selected.receiver_wait_ms : 0.0;
  transport.compute_overlap_ms =
      active ? scenario.transport_compute_overlap_ms : 0.0;
  transport.retry_risk =
      active ? scenario.transport_retry_risk : 0.0;
  auto result = estimate_kv_transport_replay(
      assumptions.transport, transport);
  if (active &&
      (!nearly_equal(result.payload_serialization_ms,
                     selected.serialization_ms) ||
       !nearly_equal(result.end_to_end_ms + selected.restore_ms,
                     selected.latency_ms))) {
    throw std::logic_error(
        "KV lifecycle tier and transport timing projections do not reconcile");
  }
  return result;
}

double weighted_score(const Assumptions& assumptions,
                      const ScoreComponents& components) {
  double score = internal::checked_add_finite_double(
      finite_product(components.prefix_sharing,
                     assumptions.prefix_sharing_weight,
                     "KV lifecycle prefix score"),
      finite_product(components.fragmentation_efficiency,
                     assumptions.fragmentation_weight,
                     "KV lifecycle fragmentation score"),
      "KV lifecycle aggregate score");
  score = internal::checked_add_finite_double(
      score,
      finite_product(components.read_efficiency,
                     assumptions.read_efficiency_weight,
                     "KV lifecycle read score"),
      "KV lifecycle aggregate score");
  score = internal::checked_add_finite_double(
      score,
      finite_product(components.tier_value,
                     assumptions.tier_value_weight,
                     "KV lifecycle tier score"),
      "KV lifecycle aggregate score");
  return internal::checked_add_finite_double(
      score,
      finite_product(components.correctness,
                     assumptions.correctness_weight,
                     "KV lifecycle correctness score"),
      "KV lifecycle aggregate score");
}

void validate_result(const Result& result) {
  require_unit_interval(result.maximum_fragmentation_ratio,
                        "KV lifecycle maximum fragmentation result");
  require_unit_interval(result.score_components.prefix_sharing,
                        "KV lifecycle prefix score result");
  require_unit_interval(result.score_components.fragmentation_efficiency,
                        "KV lifecycle fragmentation score result");
  require_unit_interval(result.score_components.read_efficiency,
                        "KV lifecycle read score result");
  require_unit_interval(result.score_components.tier_value,
                        "KV lifecycle tier score result");
  require_unit_interval(result.score_components.correctness,
                        "KV lifecycle correctness score result");
  require_unit_interval(result.lifecycle_score,
                        "KV lifecycle aggregate score result");
  require_non_negative_finite(
      result.transfer_and_restore_ms,
      "KV lifecycle transfer and restore result");
  require_finite(result.transfer_vs_prefill_delta_ms,
                 "KV lifecycle transfer versus prefill result");
  const auto& selected =
      result.tier.candidates[result.tier.selected_candidate_index];
  if (selected.requires_external_system !=
      result.transport.active_transfer_workload) {
    throw std::logic_error(
        "KV lifecycle selected tier and transport activity disagree");
  }
}

}  // namespace

const char* cache_policy_name(const CachePolicy policy) {
  switch (policy) {
    case CachePolicy::radix_prefix_keep:
      return "radix_prefix_keep";
    case CachePolicy::strict_prefix_only:
      return "strict_prefix_only";
    case CachePolicy::paged_lru_with_tier_gate:
      return "paged_lru_with_tier_gate";
  }
  throw std::logic_error("unknown KV lifecycle cache policy");
}

const char* transfer_vs_prefill_decision_name(
    const TransferVsPrefillDecision decision) {
  switch (decision) {
    case TransferVsPrefillDecision::
        hbm_residency_avoids_transfer_and_prefill:
      return "hbm_residency_avoids_transfer_and_prefill";
    case TransferVsPrefillDecision::
        transfer_or_restore_beats_prefill_recompute:
      return "transfer_or_restore_beats_prefill_recompute";
    case TransferVsPrefillDecision::
        prefill_recompute_is_safer_than_transfer:
      return "prefill_recompute_is_safer_than_transfer";
  }
  throw std::logic_error("unknown KV lifecycle transfer decision");
}

const char* recommendation_name(const Recommendation recommendation) {
  switch (recommendation) {
    case Recommendation::ready_for_replay_alignment:
      return "aggregate_kv_lifecycle_is_ready_for_replay_alignment";
    case Recommendation::require_correctness_contract:
      return "block_non_prefix_reuse_and_require_correctness_contract";
    case Recommendation::refresh_component_artifacts_before_release_claim:
      return "refresh_component_kv_artifacts_before_release_claim";
  }
  throw std::logic_error("unknown KV lifecycle recommendation");
}

Assumptions default_assumptions() {
  Assumptions assumptions;
  assumptions.paged =
      kv_paged_attention_deep_dive::default_assumptions();
  assumptions.tier =
      kv_tier_decision_deep_dive::default_assumptions();
  assumptions.compression =
      default_kv_compression_blend_assumptions();
  assumptions.transport =
      default_kv_transport_replay_assumptions();
  assumptions.maximum_correctness_risk =
      assumptions.tier.maximum_reuse_correctness_risk;
  return assumptions;
}

std::vector<Scenario> default_scenarios() {
  return {
      {"shared_prefix_long_context",
       {"vllm", "sglang"},
       {"shared_prefix_long_context",
        "results/raw/native_kv_paged_attention_deep_dive.json",
        8192,
        16,
        8192,
        4096,
        512,
        524288,
        8192.0,
        48.0,
        0.50,
        0.035},
       8.0,
       {48.0, 0.40, 0.20, 0.20, 0.0},
       {12.0, 1.20, 0.60, 0.60, 0.0},
       {48.0, 0.50, 0.40, 0.40, 0.0},
       "compiled_radix_reference",
       0.65,
       96.0,
       0.50,
       KvCompressionBlendStrategy::exact_prefix_reuse,
       4096,
       1.0,
       0.0,
       0.50,
       0.02,
       8.0,
       0.01},
      {"remote_kv_pd_split",
       {"kv_offload", "mooncake", "dynamo"},
       {"remote_kv_pd_split",
        "results/raw/native_pd_split_crossover_deep_dive.json",
        12288,
        16,
        12288,
        2048,
        2048,
        524288,
        12288.0,
        200.0,
        0.62,
        0.040},
       2.0,
       {48.0, 0.65, 0.40, 0.40, 0.0},
       {12.0, 1.80, 0.80, 0.80, 0.0},
       {200.0, 0.62, 0.50, 0.50, 0.0},
       "compiled_radix_reference",
       0.35,
       96.0,
       0.60,
       KvCompressionBlendStrategy::compressed_kv_restore,
       12288,
       0.25,
       1.0,
       0.40,
       0.08,
       120.0,
       0.04},
      {"semantic_reuse_risky",
       {"vllm", "kv_offload"},
       {"semantic_reuse_risky",
        "results/raw/native_kv_compression_blend_risk.json",
        6144,
        16,
        6144,
        512,
        1536,
        524288,
        6144.0,
        80.0,
        0.38,
        0.030},
       1.0,
       {40.0, 0.65, 0.40, 0.40, 0.0},
       {10.0, 1.80, 0.80, 0.80, 0.0},
       {80.0, 0.80, 0.60, 0.60, 0.0},
       "compiled_radix_reference",
       0.12,
       96.0,
       0.70,
       KvCompressionBlendStrategy::non_prefix_blended_reuse,
       4096,
       0.38,
       2.0,
       0.65,
       0.76,
       40.0,
       0.10},
  };
}

Result analyze(const Assumptions& assumptions,
               const Scenario& input_scenario) {
  validate_assumptions(assumptions);
  const Scenario scenario = validate_scenario(assumptions, input_scenario);

  Result result;
  result.scenario = scenario;
  result.paged = kv_paged_attention_deep_dive::analyze(
      assumptions.paged, scenario.paged);
  result.prefix = analyze_prefix(assumptions, scenario);
  result.compression = analyze_compression(assumptions, scenario);
  result.tier = analyze_tier(
      assumptions, scenario, result.paged, result.prefix,
      result.compression);
  result.transport = analyze_transport(
      assumptions, scenario, result.tier);

  const auto& selected =
      result.tier.candidates[result.tier.selected_candidate_index];
  if (selected.requires_external_system) {
    result.transfer_and_restore_ms =
        internal::checked_add_finite_double(
            result.transport.end_to_end_ms, selected.restore_ms,
            "KV lifecycle transfer and restore latency");
    result.transfer_vs_prefill_delta_ms = require_finite(
        result.transfer_and_restore_ms -
            result.paged.prefill_recompute_ms,
        "KV lifecycle transfer versus prefill delta");
    if (result.transfer_vs_prefill_delta_ms >=
        -assumptions.tier.expected_cost_tie_tolerance_ms) {
      throw std::logic_error(
          "KV lifecycle selected transfer did not strictly beat prefill recompute");
    }
    result.transfer_vs_prefill = TransferVsPrefillDecision::
        transfer_or_restore_beats_prefill_recompute;
  } else if (selected.tier ==
             kv_tier_decision_deep_dive::TierKind::hbm_keep) {
    result.transfer_vs_prefill = TransferVsPrefillDecision::
        hbm_residency_avoids_transfer_and_prefill;
  } else {
    result.transfer_vs_prefill = TransferVsPrefillDecision::
        prefill_recompute_is_safer_than_transfer;
  }

  result.maximum_fragmentation_ratio = std::max(
      {result.paged.internal_fragmentation_ratio,
       result.paged.external_fragmentation_ratio,
       result.prefix.fragmentation_ratio});
  result.score_components.prefix_sharing =
      result.paged.prefix_sharing_rate;
  result.score_components.fragmentation_efficiency =
      1.0 - result.maximum_fragmentation_ratio;
  result.score_components.read_efficiency =
      std::min(1.0, require_finite(
                        1.0 / result.paged.read_amplification,
                        "KV lifecycle read efficiency"));
  result.score_components.tier_value =
      selected.tier == kv_tier_decision_deep_dive::TierKind::hbm_keep
          ? 1.0
          : (selected.requires_external_system
                 ? result.tier.latency_advantage_score
                 : 0.0);
  result.score_components.correctness =
      1.0 - result.compression.quality_risk_score;
  result.lifecycle_score = weighted_score(
      assumptions, result.score_components);

  if (result.compression.quality_risk_score >
      assumptions.maximum_correctness_risk) {
    result.cache_policy = CachePolicy::strict_prefix_only;
    result.recommendation =
        Recommendation::require_correctness_contract;
  } else if (result.prefix.radix_prefix_hit_rate >=
                 assumptions.radix_keep_hit_threshold &&
             result.paged.scenario.shared_prefix_tokens > 0) {
    result.cache_policy = CachePolicy::radix_prefix_keep;
  } else {
    result.cache_policy = CachePolicy::paged_lru_with_tier_gate;
  }
  if (result.recommendation !=
      Recommendation::require_correctness_contract) {
    result.recommendation =
        result.lifecycle_score >=
                assumptions.replay_alignment_score_threshold
            ? Recommendation::ready_for_replay_alignment
            : Recommendation::
                  refresh_component_artifacts_before_release_claim;
  }

  validate_result(result);
  return result;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "KV lifecycle scenario inventory must not be empty");
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.records.reserve(scenarios.size());
  double score_sum = 0.0;
  for (std::size_t index = 0U; index < scenarios.size(); ++index) {
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (scenarios[prior].name == scenarios[index].name) {
        throw std::invalid_argument(
            "duplicate KV lifecycle scenario name: " +
            scenarios[index].name);
      }
    }
    plan.records.push_back(analyze(assumptions, scenarios[index]));
    const auto& record = plan.records.back();
    score_sum = internal::checked_add_finite_double(
        score_sum, record.lifecycle_score,
        "KV lifecycle score sum");
    const auto& selected =
        record.tier.candidates[record.tier.selected_candidate_index];
    if (selected.tier ==
        kv_tier_decision_deep_dive::TierKind::hbm_keep) {
      plan.hbm_selected_count = internal::checked_add_size(
          plan.hbm_selected_count, 1U,
          "KV lifecycle HBM selected count");
    } else if (selected.requires_external_system) {
      plan.transfer_selected_count = internal::checked_add_size(
          plan.transfer_selected_count, 1U,
          "KV lifecycle transfer selected count");
    } else {
      plan.recompute_selected_count = internal::checked_add_size(
          plan.recompute_selected_count, 1U,
          "KV lifecycle recompute selected count");
    }
    if (index == 0U || record.lifecycle_score >
                           plan.records[plan.best_record_index]
                               .lifecycle_score) {
      plan.best_record_index = index;
    }
    if (index == 0U || record.paged.read_amplification >
                           plan.records[plan.worst_read_record_index]
                               .paged.read_amplification) {
      plan.worst_read_record_index = index;
    }
    if (index == 0U || record.maximum_fragmentation_ratio >
                           plan.records[plan.worst_fragmentation_record_index]
                               .maximum_fragmentation_ratio) {
      plan.worst_fragmentation_record_index = index;
    }
  }
  plan.average_lifecycle_score = require_finite(
      score_sum / static_cast<double>(plan.records.size()),
      "KV lifecycle average score");
  const std::size_t selection_total = internal::checked_sum_size(
      {plan.hbm_selected_count, plan.transfer_selected_count,
       plan.recompute_selected_count},
      "KV lifecycle selection total");
  if (plan.best_record_index >= plan.records.size() ||
      plan.worst_read_record_index >= plan.records.size() ||
      plan.worst_fragmentation_record_index >= plan.records.size() ||
      selection_total != plan.records.size()) {
    throw std::logic_error(
        "KV lifecycle plan summary reconciliation failed");
  }
  return plan;
}

}  // namespace inferlab::kv_lifecycle_deep_dive
