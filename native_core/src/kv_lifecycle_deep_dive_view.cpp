#include "kv_lifecycle_deep_dive_view.hpp"

#include "checked_index.hpp"
#include "deep_dive_core_artifact_registry.hpp"
#include "kv_paged_attention_deep_dive_view.hpp"
#include "kv_tier_decision_deep_dive_view.hpp"
#include "deep_dive_artifact_identity.hpp"
#include "mature_product_profile_registry.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::kv_lifecycle_deep_dive {
namespace {

constexpr const char* kArtifactContractName =
    "native_kv_lifecycle_deep_dive";
constexpr const char* kClaimScope =
    "compiled native KV lifecycle composition slice; modeled mechanism evidence only, not a production KV engine, distributed KV store, measured latency, product replay, or execute result";

void validate_plan_for_view(const Plan& plan) {
  if (plan.records.empty() ||
      plan.best_record_index >= plan.records.size() ||
      plan.worst_read_record_index >= plan.records.size() ||
      plan.worst_fragmentation_record_index >= plan.records.size()) {
    throw std::invalid_argument(
        "KV lifecycle view received an invalid plan");
  }
  const std::size_t selection_total = internal::checked_sum_size(
      {plan.hbm_selected_count, plan.transfer_selected_count,
       plan.recompute_selected_count},
      "KV lifecycle view selection total");
  if (selection_total != plan.records.size()) {
    throw std::invalid_argument(
        "KV lifecycle view selection summary is inconsistent");
  }
}

mature_product_profiles::ProductProfileProjection product_projection(
    const Plan& plan) {
  std::vector<std::string> ids;
  for (const auto& record : plan.records) {
    for (const auto& id : record.scenario.product_profile_ids) {
      inferlab_plan::append_unique_string(ids, id);
    }
  }
  return mature_product_profiles::project_product_profiles(ids);
}

std::vector<std::string> evidence_bridge_inputs(
    const mature_product_profiles::ProductProfileProjection& products) {
  const auto& paged =
      inferlab_deep_dive_core::require_artifact_spec(
          "kv_paged_attention_deep_dive");
  std::vector<std::string> inputs = {
      "results/raw/" + paged.artifact,
      inferlab_artifact_contract::require_contract_extension(
          "native_kv_tier_decision_deep_dive")
          .artifact,
      inferlab_artifact_contract::require_contract_extension(
          "native_pd_split_crossover_deep_dive")
          .artifact,
      inferlab_artifact_contract::require_contract_extension(
          "native_kv_transport_timeline_deep_dive")
          .artifact,
      inferlab_artifact_contract::require_contract_extension(
          "native_radix_prefix_cache_deep_dive")
          .artifact,
      inferlab_artifact_contract::require_contract_extension(
          "native_kv_compression_blend_risk")
          .artifact,
  };
  for (const auto& artifact : products.artifact_paths) {
    inferlab_plan::append_unique_string(inputs, artifact);
  }
  return inputs;
}

std::vector<std::string> recommended_commands(
    const inferlab_deep_dive::ArtifactIdentity& identity,
    const mature_product_profiles::ProductProfileProjection& products) {
  std::vector<std::string> commands = {identity.wiring.command};
  for (const auto& command : products.replay_commands) {
    inferlab_plan::append_unique_string(commands, command);
  }
  return commands;
}

const std::vector<std::string>& research_basis() {
  static const std::vector<std::string> basis = {
      "canonical Paged KV allocation and token mapping",
      "canonical Radix Prefix cache analytical model",
      "canonical KV compression and correctness-risk model",
      "shared KV tier expected-cost decision model",
      "canonical KV transport timeline and serialization",
      "cross-component typed reconciliation"};
  return basis;
}

std::string assumptions_json(const Assumptions& assumptions,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"fixture_name\": "
      << inferlab_plan::json_string(assumptions.fixture_name) << ",\n";
  out << nested << "\"expected_cost_tie_tolerance_ms\": "
      << inferlab_plan::fixed6(
             assumptions.tier.expected_cost_tie_tolerance_ms)
      << ",\n";
  out << nested << "\"maximum_correctness_risk\": "
      << inferlab_plan::fixed6(assumptions.maximum_correctness_risk)
      << ",\n";
  out << nested << "\"metric_scope\": "
      << inferlab_plan::json_string(
             "compiled_fixture_modeled_not_measured")
      << ",\n";
  out << nested << "\"paged_fixture\": "
      << inferlab_plan::json_string(assumptions.paged.fixture_name)
      << ",\n";
  out << nested << "\"prefix_reference_requests\": "
      << assumptions.prefix_reference_requests << ",\n";
  out << nested << "\"radix_keep_hit_threshold\": "
      << inferlab_plan::fixed6(assumptions.radix_keep_hit_threshold)
      << ",\n";
  out << nested << "\"replay_alignment_score_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.replay_alignment_score_threshold)
      << ",\n";
  out << nested << "\"score_weights\": {\n";
  out << nested << "  \"correctness\": "
      << inferlab_plan::fixed6(assumptions.correctness_weight) << ",\n";
  out << nested << "  \"fragmentation\": "
      << inferlab_plan::fixed6(assumptions.fragmentation_weight)
      << ",\n";
  out << nested << "  \"prefix_sharing\": "
      << inferlab_plan::fixed6(assumptions.prefix_sharing_weight)
      << ",\n";
  out << nested << "  \"read_efficiency\": "
      << inferlab_plan::fixed6(assumptions.read_efficiency_weight)
      << ",\n";
  out << nested << "  \"tier_value\": "
      << inferlab_plan::fixed6(assumptions.tier_value_weight) << "\n";
  out << nested << "},\n";
  out << nested << "\"tier_fixture\": "
      << inferlab_plan::json_string(assumptions.tier.fixture_name)
      << ",\n";
  out << nested << "\"transport_fixture\": "
      << inferlab_plan::json_string(assumptions.transport.profile_name)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const Result& result, const int indent) {
  const auto& scenario = result.scenario;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"compression_ratio\": "
      << inferlab_plan::fixed6(scenario.compression_ratio) << ",\n";
  out << nested << "\"compression_reusable_tokens\": "
      << scenario.compression_reusable_tokens << ",\n";
  out << nested << "\"compression_strategy\": "
      << inferlab_plan::json_string(kv_compression_blend_strategy_name(
             scenario.compression_strategy))
      << ",\n";
  out << nested << "\"correctness_risk\": "
      << inferlab_plan::fixed6(scenario.correctness_risk) << ",\n";
  out << nested << "\"hbm_free_gib\": "
      << inferlab_plan::fixed6(scenario.hbm_free_gib) << ",\n";
  out << nested << "\"kv_bytes_per_token\": "
      << scenario.paged.kv_bytes_per_token << ",\n";
  out << nested << "\"live_tokens\": "
      << scenario.paged.live_tokens << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"page_size_tokens\": "
      << scenario.paged.page_size_tokens << ",\n";
  out << nested << "\"prefill_ms_per_token\": "
      << inferlab_plan::fixed6(scenario.paged.prefill_ms_per_token)
      << ",\n";
  out << nested << "\"prefix_reuse_rate\": "
      << inferlab_plan::fixed6(scenario.prefix_reuse_rate) << ",\n";
  out << nested << "\"product_profile_ids\": "
      << inferlab_plan::json_string_array(scenario.product_profile_ids,
                                          indent + 2)
      << ",\n";
  out << nested << "\"remote_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.remote.bandwidth_gbps) << ",\n";
  out << nested << "\"request_tokens\": "
      << scenario.paged.request_tokens << ",\n";
  out << nested << "\"reserved_tokens\": "
      << result.paged.reserved_tokens << ",\n";
  out << nested << "\"shared_prefix_tokens\": "
      << scenario.paged.shared_prefix_tokens << "\n";
  out << pad << "}";
  return out.str();
}

std::string score_components_json(const ScoreComponents& components,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"correctness\": "
      << inferlab_plan::fixed6(components.correctness) << ",\n";
  out << nested << "\"fragmentation_efficiency\": "
      << inferlab_plan::fixed6(components.fragmentation_efficiency)
      << ",\n";
  out << nested << "\"prefix_sharing\": "
      << inferlab_plan::fixed6(components.prefix_sharing) << ",\n";
  out << nested << "\"read_efficiency\": "
      << inferlab_plan::fixed6(components.read_efficiency) << ",\n";
  out << nested << "\"tier_value\": "
      << inferlab_plan::fixed6(components.tier_value) << "\n";
  out << pad << "}";
  return out.str();
}

std::string prefix_json(const RadixPrefixCacheResult& prefix,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"block_sharing_ratio\": "
      << inferlab_plan::fixed6(prefix.block_sharing_ratio) << ",\n";
  out << nested << "\"cache_efficiency_score\": "
      << inferlab_plan::fixed6(prefix.cache_efficiency_score) << ",\n";
  out << nested << "\"eviction_cost_ms\": "
      << inferlab_plan::fixed6(prefix.eviction_cost_ms) << ",\n";
  out << nested << "\"exact_prefix_hit_rate\": "
      << inferlab_plan::fixed6(prefix.exact_prefix_hit_rate) << ",\n";
  out << nested << "\"fragmentation_ratio\": "
      << inferlab_plan::fixed6(prefix.fragmentation_ratio) << ",\n";
  out << nested << "\"radix_prefix_hit_rate\": "
      << inferlab_plan::fixed6(prefix.radix_prefix_hit_rate) << "\n";
  out << pad << "}";
  return out.str();
}

std::string compression_json(const KvCompressionBlendResult& compression,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"compressed_reusable_kv_gib\": "
      << inferlab_plan::fixed6(compression.compressed_reusable_kv_gib)
      << ",\n";
  out << nested << "\"quality_risk_score\": "
      << inferlab_plan::fixed6(compression.quality_risk_score) << ",\n";
  out << nested << "\"raw_reusable_kv_gib\": "
      << inferlab_plan::fixed6(compression.raw_reusable_kv_gib)
      << ",\n";
  out << nested << "\"restore_latency_ms\": "
      << inferlab_plan::fixed6(compression.restore_latency_ms) << ",\n";
  out << nested << "\"safety_policy\": "
      << inferlab_plan::json_string(compression.safety_policy) << "\n";
  out << pad << "}";
  return out.str();
}

std::string timeline_event_json(const KvTransportTimelineEvent& event,
                                const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"duration_ms\": "
      << inferlab_plan::fixed6(event.duration_ms) << ",\n";
  out << nested << "\"owner\": "
      << inferlab_plan::json_string(
             kv_transport_timeline_resource_name(event.phase))
      << ",\n";
  out << nested << "\"stage\": "
      << inferlab_plan::json_string(
             kv_transport_timeline_phase_name(event.phase))
      << ",\n";
  out << nested << "\"start_ms\": "
      << inferlab_plan::fixed6(event.start_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string timeline_json(const std::vector<KvTransportTimelineEvent>& events,
                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0U; index < events.size(); ++index) {
    out << timeline_event_json(events[index], indent + 2)
        << (index == events.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string transport_json(const KvTransportReplayResult& transport,
                           const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"active_transfer_workload\": "
      << inferlab_plan::json_bool(transport.active_transfer_workload)
      << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(
             kv_transport_replay_backend_name(transport.scenario.transport))
      << ",\n";
  out << nested << "\"decode_blocked_ms\": "
      << inferlab_plan::fixed6(transport.decode_blocked_ms) << ",\n";
  out << nested << "\"end_to_end_ms\": "
      << inferlab_plan::fixed6(transport.end_to_end_ms) << ",\n";
  out << nested << "\"overlap_ratio\": "
      << inferlab_plan::fixed6(transport.overlap_ratio) << ",\n";
  out << nested << "\"payload_gib\": "
      << inferlab_plan::fixed6(transport.scenario.kv_gib) << ",\n";
  out << nested << "\"serialization_ms\": "
      << inferlab_plan::fixed6(transport.payload_serialization_ms)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string result_json(const Result& result, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& selected =
      result.tier.candidates[result.tier.selected_candidate_index];
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cache_policy\": "
      << inferlab_plan::json_string(cache_policy_name(result.cache_policy))
      << ",\n";
  out << nested << "\"compression\": "
      << compression_json(result.compression, indent + 2) << ",\n";
  out << nested << "\"compression_restore_ms\": "
      << inferlab_plan::fixed6(result.compression.restore_latency_ms)
      << ",\n";
  out << nested << "\"external_fragmentation_ratio\": "
      << inferlab_plan::fixed6(
             result.paged.external_fragmentation_ratio)
      << ",\n";
  out << nested << "\"internal_fragmentation_ratio\": "
      << inferlab_plan::fixed6(
             result.paged.internal_fragmentation_ratio)
      << ",\n";
  out << nested << "\"lifecycle_score\": "
      << inferlab_plan::fixed6(result.lifecycle_score) << ",\n";
  out << nested << "\"logical_to_physical_blocks\": "
      << kv_paged_attention_deep_dive::mapping_samples_json(
             result.paged.mapping_samples, indent + 2)
      << ",\n";
  out << nested << "\"maximum_fragmentation_ratio\": "
      << inferlab_plan::fixed6(result.maximum_fragmentation_ratio)
      << ",\n";
  out << nested << "\"prefix_cache\": "
      << prefix_json(result.prefix, indent + 2) << ",\n";
  out << nested << "\"prefix_sharing_rate\": "
      << inferlab_plan::fixed6(result.paged.prefix_sharing_rate)
      << ",\n";
  out << nested << "\"prefill_recompute_ms\": "
      << inferlab_plan::fixed6(result.paged.prefill_recompute_ms)
      << ",\n";
  out << nested << "\"read_amplification\": "
      << inferlab_plan::fixed6(result.paged.read_amplification)
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommendation_name(result.recommendation))
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result, indent + 2) << ",\n";
  out << nested << "\"score_components\": "
      << score_components_json(result.score_components, indent + 2)
      << ",\n";
  out << nested << "\"selected_tier\": "
      << inferlab_plan::json_string(
             kv_tier_decision_deep_dive::tier_kind_name(selected.tier))
      << ",\n";
  out << nested << "\"tier_decisions\": "
      << kv_tier_decision_deep_dive::tier_candidates_json(
             result.tier.candidates, indent + 2)
      << ",\n";
  out << nested << "\"transfer_and_restore_ms\": "
      << inferlab_plan::fixed6(result.transfer_and_restore_ms)
      << ",\n";
  out << nested << "\"transfer_ms\": "
      << inferlab_plan::fixed6(result.transport.end_to_end_ms) << ",\n";
  out << nested << "\"transfer_vs_prefill\": "
      << inferlab_plan::json_string(
             transfer_vs_prefill_decision_name(
                 result.transfer_vs_prefill))
      << ",\n";
  out << nested << "\"transfer_vs_prefill_delta_ms\": "
      << inferlab_plan::fixed6(result.transfer_vs_prefill_delta_ms)
      << ",\n";
  out << nested << "\"transport\": "
      << transport_json(result.transport, indent + 2) << ",\n";
  out << nested << "\"transport_timeline\": "
      << timeline_json(result.transport.timeline, indent + 2) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan_for_view(plan);
  const auto identity =
      inferlab_deep_dive::require_artifact_identity(
          kArtifactContractName);
  const auto products = product_projection(plan);
  const auto inputs = evidence_bridge_inputs(products);
  const auto commands = recommended_commands(identity, products);
  const auto& best = plan.records[plan.best_record_index];
  const auto& worst_read = plan.records[plan.worst_read_record_index];
  const auto& worst_fragmentation =
      plan.records[plan.worst_fragmentation_record_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(identity.contract.name) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, identity.contract.name, kClaimScope, products.display_names,
      identity.wiring.target, identity.contract.artifact,
      identity.wiring.command);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"deep_dive_type\": \"kv_lifecycle\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"claim_boundary_detail\": "
      << inferlab_plan::json_string(
             "compiled composition of existing native reference models only; no product KV cache engine, remote store, connector, replay, CUDA timing, measured latency, operational policy, or release readiness is executed or inferred")
      << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"metric_evidence\": \"modeled_not_measured\",\n";
  out << "  \"metric_units\": {\n";
  out << "    \"bandwidth_gbps\": \"decimal_gigabits_per_second\",\n";
  out << "    \"capacity_and_payload\": \"binary_gib\",\n";
  out << "    \"latency_and_timeline\": \"milliseconds\"\n";
  out << "  },\n";
  out << "  \"fixture_basis\": "
      << inferlab_plan::json_string(plan.assumptions.fixture_name)
      << ",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"artifact_identity\": \"deep_dive_artifact_identity.hpp/.cpp\",\n";
  out << "    \"paged_allocation_and_mapping\": \"kv_paged_attention_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"prefix_cache\": \"inferlab::estimate_radix_prefix_cache\",\n";
  out << "    \"compression_and_correctness\": \"inferlab::estimate_kv_compression_blend\",\n";
  out << "    \"tier_decision\": \"kv_tier_decision_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"transport_timeline\": \"inferlab::estimate_kv_transport_replay\",\n";
  out << "    \"lifecycle_composition\": \"kv_lifecycle_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"artifact_view\": \"kv_lifecycle_deep_dive_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(research_basis(), 2) << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(products.display_names, 2)
      << ",\n";
  out << "  \"assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(inputs, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"average_lifecycle_score\": "
      << inferlab_plan::fixed6(plan.average_lifecycle_score) << ",\n";
  out << "    \"best_lifecycle_scenario\": "
      << inferlab_plan::json_string(best.scenario.name) << ",\n";
  out << "    \"best_lifecycle_score\": "
      << inferlab_plan::fixed6(best.lifecycle_score) << ",\n";
  out << "    \"hbm_selected_count\": " << plan.hbm_selected_count
      << ",\n";
  out << "    \"recompute_selected_count\": "
      << plan.recompute_selected_count << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << ",\n";
  out << "    \"transfer_selected_count\": "
      << plan.transfer_selected_count << ",\n";
  out << "    \"worst_fragmentation_ratio\": "
      << inferlab_plan::fixed6(
             worst_fragmentation.maximum_fragmentation_ratio)
      << ",\n";
  out << "    \"worst_fragmentation_scenario\": "
      << inferlab_plan::json_string(
             worst_fragmentation.scenario.name)
      << ",\n";
  out << "    \"worst_read_amplification\": "
      << inferlab_plan::fixed6(worst_read.paged.read_amplification)
      << ",\n";
  out << "    \"worst_read_amplification_scenario\": "
      << inferlab_plan::json_string(worst_read.scenario.name) << "\n";
  out << "  },\n";
  out << "  \"logical_to_physical_blocks\": "
      << kv_paged_attention_deep_dive::mapping_samples_json(
             best.paged.mapping_samples, 2)
      << ",\n";
  out << "  \"prefix_sharing\": {\n";
  out << "    \"best_rate\": "
      << inferlab_plan::fixed6(best.paged.prefix_sharing_rate) << ",\n";
  out << "    \"policy\": "
      << inferlab_plan::json_string(cache_policy_name(best.cache_policy))
      << ",\n";
  out << "    \"radix_hit_rate\": "
      << inferlab_plan::fixed6(best.prefix.radix_prefix_hit_rate) << "\n";
  out << "  },\n";
  out << "  \"fragmentation\": {\n";
  out << "    \"worst_external_ratio\": "
      << inferlab_plan::fixed6(
             worst_fragmentation.paged.external_fragmentation_ratio)
      << ",\n";
  out << "    \"worst_internal_ratio\": "
      << inferlab_plan::fixed6(
             worst_fragmentation.paged.internal_fragmentation_ratio)
      << ",\n";
  out << "    \"worst_radix_ratio\": "
      << inferlab_plan::fixed6(
             worst_fragmentation.prefix.fragmentation_ratio)
      << "\n";
  out << "  },\n";
  out << "  \"tier_decisions\": "
      << kv_tier_decision_deep_dive::tier_candidates_json(
             best.tier.candidates, 2)
      << ",\n";
  out << "  \"transport_timeline\": "
      << timeline_json(best.transport.timeline, 2) << ",\n";
  out << "  \"transfer_vs_prefill\": {\n";
  out << "    \"best_decision\": "
      << inferlab_plan::json_string(
             transfer_vs_prefill_decision_name(
                 best.transfer_vs_prefill))
      << ",\n";
  out << "    \"prefill_recompute_ms\": "
      << inferlab_plan::fixed6(best.paged.prefill_recompute_ms)
      << ",\n";
  out << "    \"transfer_and_restore_ms\": "
      << inferlab_plan::fixed6(best.transfer_and_restore_ms) << ",\n";
  out << "    \"transfer_ms\": "
      << inferlab_plan::fixed6(best.transport.end_to_end_ms) << ",\n";
  out << "    \"transfer_vs_prefill_delta_ms\": "
      << inferlab_plan::fixed6(best.transfer_vs_prefill_delta_ms)
      << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << result_json(plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(commands, 2) << ",\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::kv_lifecycle_deep_dive
