#include "kv_tier_decision_deep_dive_view.hpp"

#include "deep_dive_core_artifact_registry.hpp"
#include "checked_index.hpp"
#include "deep_dive_artifact_identity.hpp"
#include "mature_product_profile_registry.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::kv_tier_decision_deep_dive {
namespace {

constexpr const char* kArtifactContractName =
    "native_kv_tier_decision_deep_dive";
constexpr const char* kClaimScope =
    "compiled native KV tier decision mechanism slice; modeled candidates only, not a product KV offload benchmark or executed connector";

void validate_plan_for_view(const Plan& plan) {
  if (plan.records.empty() ||
      plan.best_record_index >= plan.records.size() ||
      plan.highest_pressure_record_index >= plan.records.size()) {
    throw std::invalid_argument(
        "KV tier decision view received an invalid plan");
  }
  const std::size_t selection_total = internal::checked_sum_size(
      {plan.hbm_selected_count, plan.external_tier_selected_count,
       plan.recompute_selected_count},
      "KV tier decision view selection total");
  if (selection_total != plan.records.size()) {
    throw std::invalid_argument(
        "KV tier decision view selection summary is inconsistent");
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
  const auto& transport =
      inferlab_artifact_contract::require_contract_extension(
          "native_kv_transport_timeline_deep_dive");
  std::vector<std::string> inputs = {
      "results/raw/" + paged.artifact,
      transport.artifact,
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
      "canonical runtime/communication transfer serialization",
      "HBM capacity fit and explicit shortfall",
      "hit-weighted restore versus prefill recompute",
      "reuse correctness policy gate",
      "mature-product profile foreign keys"};
  return basis;
}

std::string transfer_path_json(const TransferPath& path,
                               const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"bandwidth_gbps\": "
      << inferlab_plan::fixed6(path.bandwidth_gbps) << ",\n";
  out << nested << "\"receiver_wait_ms\": "
      << inferlab_plan::fixed6(path.receiver_wait_ms) << ",\n";
  out << nested << "\"restore_ms\": "
      << inferlab_plan::fixed6(path.restore_ms) << ",\n";
  out << nested << "\"sender_wait_ms\": "
      << inferlab_plan::fixed6(path.sender_wait_ms) << ",\n";
  out << nested << "\"setup_ms\": "
      << inferlab_plan::fixed6(path.setup_ms) << "\n";
  out << pad << "}";
  return out.str();
}

std::string scenario_json(const Scenario& scenario, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"correctness_risk\": "
      << inferlab_plan::fixed6(scenario.correctness_risk) << ",\n";
  out << nested << "\"cpu_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.cpu_dram.bandwidth_gbps)
      << ",\n";
  out << nested << "\"cpu_dram_path\": "
      << transfer_path_json(scenario.cpu_dram, indent + 2) << ",\n";
  out << nested << "\"decode_tokens\": " << scenario.decode_tokens
      << ",\n";
  out << nested << "\"hbm_free_gb\": "
      << inferlab_plan::fixed6(scenario.hbm_free_gib) << ",\n";
  out << nested << "\"hbm_free_gib\": "
      << inferlab_plan::fixed6(scenario.hbm_free_gib) << ",\n";
  out << nested << "\"hit_probability\": "
      << inferlab_plan::fixed6(scenario.hit_probability) << ",\n";
  out << nested << "\"kv_bytes_per_token\": "
      << inferlab_plan::fixed6(scenario.kv_bytes_per_token) << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"nvme_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.nvme.bandwidth_gbps) << ",\n";
  out << nested << "\"nvme_path\": "
      << transfer_path_json(scenario.nvme, indent + 2) << ",\n";
  out << nested << "\"prefill_ms_per_token\": "
      << inferlab_plan::fixed6(scenario.prefill_ms_per_token) << ",\n";
  out << nested << "\"prefill_recompute_tokens\": "
      << scenario.prefill_recompute_tokens << ",\n";
  out << nested << "\"product_profile_ids\": "
      << inferlab_plan::json_string_array(scenario.product_profile_ids,
                                          indent + 2)
      << ",\n";
  out << nested << "\"prompt_tokens\": " << scenario.prompt_tokens
      << ",\n";
  out << nested << "\"remote_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.remote.bandwidth_gbps) << ",\n";
  out << nested << "\"remote_path\": "
      << transfer_path_json(scenario.remote, indent + 2) << ",\n";
  out << nested << "\"transfer_payload_ratio\": "
      << inferlab_plan::fixed6(scenario.transfer_payload_ratio) << "\n";
  out << pad << "}";
  return out.str();
}

std::string assumptions_json(const Assumptions& assumptions,
                             const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"bytes_per_gib\": "
      << inferlab_plan::fixed6(assumptions.bytes_per_gib) << ",\n";
  out << nested << "\"expected_cost_scope\": "
      << inferlab_plan::json_string(
             "hit_probability_times_restore_plus_miss_probability_times_prefill")
      << ",\n";
  out << nested << "\"fixture_name\": "
      << inferlab_plan::json_string(assumptions.fixture_name) << ",\n";
  out << nested << "\"expected_cost_tie_tolerance_ms\": "
      << inferlab_plan::fixed6(
             assumptions.expected_cost_tie_tolerance_ms)
      << ",\n";
  out << nested << "\"hbm_lookup_ms\": "
      << inferlab_plan::fixed6(assumptions.hbm_lookup_ms) << ",\n";
  out << nested << "\"max_kv_bytes_per_token\": "
      << inferlab_plan::fixed6(assumptions.max_kv_bytes_per_token)
      << ",\n";
  out << nested << "\"max_tokens\": " << assumptions.max_tokens
      << ",\n";
  out << nested << "\"maximum_reuse_correctness_risk\": "
      << inferlab_plan::fixed6(
             assumptions.maximum_reuse_correctness_risk)
      << ",\n";
  out << nested << "\"replay_alignment_score_threshold\": "
      << inferlab_plan::fixed6(
             assumptions.replay_alignment_score_threshold)
      << ",\n";
  out << nested << "\"score_weights\": {\n";
  out << nested << "  \"correctness\": "
      << inferlab_plan::fixed6(assumptions.correctness_weight) << ",\n";
  out << nested << "  \"hbm_headroom\": "
      << inferlab_plan::fixed6(assumptions.hbm_headroom_weight) << ",\n";
  out << nested << "  \"hit_probability\": "
      << inferlab_plan::fixed6(assumptions.hit_probability_weight)
      << ",\n";
  out << nested << "  \"latency_advantage\": "
      << inferlab_plan::fixed6(assumptions.latency_advantage_weight)
      << "\n";
  out << nested << "}\n";
  out << pad << "}";
  return out.str();
}

std::string selected_fallback_reason(const Result& result) {
  const auto& selected =
      result.candidates[result.selected_candidate_index];
  if (selected.requires_external_system) {
    return "selected_modeled_candidate_requires_product_connector_replay_or_execute";
  }
  if (selected.tier == TierKind::hbm_keep) {
    return "selected_modeled_hbm_capacity_fit";
  }
  return "selected_modeled_prefill_recompute_fallback";
}

std::string result_json(const Result& result, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  const auto& selected =
      result.candidates[result.selected_candidate_index];
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(decision_name(result.decision))
      << ",\n";
  out << nested << "\"deep_dive_score\": "
      << inferlab_plan::fixed6(result.deep_dive_score) << ",\n";
  out << nested << "\"fallback_reason\": "
      << inferlab_plan::json_string(selected_fallback_reason(result))
      << ",\n";
  out << nested << "\"hbm_pressure\": "
      << inferlab_plan::fixed6(result.hbm_pressure) << ",\n";
  out << nested << "\"hbm_shortfall_gib\": "
      << inferlab_plan::fixed6(result.hbm_shortfall_gib) << ",\n";
  out << nested << "\"kv_bytes\": "
      << inferlab_plan::fixed6(result.kv_bytes) << ",\n";
  out << nested << "\"kv_gb\": "
      << inferlab_plan::fixed6(result.kv_gib) << ",\n";
  out << nested << "\"kv_gib\": "
      << inferlab_plan::fixed6(result.kv_gib) << ",\n";
  out << nested << "\"kv_token_count\": " << result.kv_token_count
      << ",\n";
  out << nested << "\"latency_advantage_score\": "
      << inferlab_plan::fixed6(result.latency_advantage_score) << ",\n";
  out << nested << "\"net_latency_delta_ms\": "
      << inferlab_plan::fixed6(result.net_latency_delta_ms) << ",\n";
  out << nested << "\"prefill_recompute_ms\": "
      << inferlab_plan::fixed6(result.prefill_recompute_ms) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(
             recommendation_name(result.recommendation))
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, indent + 2) << ",\n";
  out << nested << "\"selected_expected_cost_ms\": "
      << inferlab_plan::fixed6(result.selected_expected_cost_ms)
      << ",\n";
  out << nested << "\"selected_latency_ms\": "
      << inferlab_plan::fixed6(result.selected_latency_ms) << ",\n";
  out << nested << "\"selected_tier\": "
      << inferlab_plan::json_string(tier_kind_name(selected.tier))
      << ",\n";
  out << nested << "\"tier_candidates\": "
      << tier_candidates_json(result.candidates, indent + 2) << ",\n";
  out << nested << "\"transfer_payload_gib\": "
      << inferlab_plan::fixed6(result.transfer_payload_gib) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string tier_candidate_json(const TierCandidate& candidate,
                                const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"analytically_eligible\": "
      << inferlab_plan::json_bool(candidate.analytically_eligible)
      << ",\n";
  out << nested << "\"bandwidth_gbps\": "
      << inferlab_plan::fixed6(candidate.bandwidth_gbps) << ",\n";
  out << nested << "\"expected_cost_ms\": "
      << (candidate.expected_cost_modeled
              ? inferlab_plan::fixed6(candidate.expected_cost_ms)
              : "null")
      << ",\n";
  out << nested << "\"expected_cost_modeled\": "
      << inferlab_plan::json_bool(candidate.expected_cost_modeled)
      << ",\n";
  out << nested << "\"fallback_reason\": "
      << inferlab_plan::json_string(
             candidate_disposition_name(candidate.disposition))
      << ",\n";
  out << nested << "\"hbm_saved_gb\": "
      << inferlab_plan::fixed6(candidate.hbm_saved_gib) << ",\n";
  out << nested << "\"hbm_saved_gib\": "
      << inferlab_plan::fixed6(candidate.hbm_saved_gib) << ",\n";
  out << nested << "\"latency_ms\": "
      << inferlab_plan::fixed6(candidate.latency_ms) << ",\n";
  out << nested << "\"payload_gib\": "
      << inferlab_plan::fixed6(candidate.payload_gib) << ",\n";
  out << nested << "\"receiver_wait_ms\": "
      << inferlab_plan::fixed6(candidate.receiver_wait_ms) << ",\n";
  out << nested << "\"requires_external_system\": "
      << inferlab_plan::json_bool(candidate.requires_external_system)
      << ",\n";
  out << nested << "\"restore_ms\": "
      << inferlab_plan::fixed6(candidate.restore_ms) << ",\n";
  out << nested << "\"selected\": "
      << inferlab_plan::json_bool(candidate.selected) << ",\n";
  out << nested << "\"sender_wait_ms\": "
      << inferlab_plan::fixed6(candidate.sender_wait_ms) << ",\n";
  out << nested << "\"serialization_ms\": "
      << inferlab_plan::fixed6(candidate.serialization_ms) << ",\n";
  out << nested << "\"setup_ms\": "
      << inferlab_plan::fixed6(candidate.setup_ms) << ",\n";
  out << nested << "\"tier\": "
      << inferlab_plan::json_string(tier_kind_name(candidate.tier))
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string tier_candidates_json(
    const std::vector<TierCandidate>& candidates, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0U; index < candidates.size(); ++index) {
    out << tier_candidate_json(candidates[index], indent + 2)
        << (index == candidates.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string artifact_json(const Plan& plan) {
  validate_plan_for_view(plan);
  const auto identity =
      inferlab_deep_dive::require_artifact_identity(
          kArtifactContractName);
  const auto products = product_projection(plan);
  const auto inputs = evidence_bridge_inputs(products);
  const auto commands = recommended_commands(identity, products);
  const auto& best = plan.records[plan.best_record_index];
  const auto& highest_pressure =
      plan.records[plan.highest_pressure_record_index];

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
  out << "  \"deep_dive_type\": \"kv_tier_decision\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(kClaimScope) << ",\n";
  out << "  \"claim_boundary_detail\": "
      << inferlab_plan::json_string(
             "compiled analytical tier selection only; external connector candidates remain modeled and require product replay or execute evidence before any operational, latency, hit-rate, or release claim")
      << ",\n";
  out << "  \"evidence_confidence\": \"schema_only\",\n";
  out << "  \"metric_evidence\": \"modeled_not_measured\",\n";
  out << "  \"metric_units\": {\n";
  out << "    \"bandwidth_gbps\": \"decimal_gigabits_per_second\",\n";
  out << "    \"hbm_free_gb\": \"legacy_alias_binary_gib\",\n";
  out << "    \"hbm_free_gib\": \"binary_gib\",\n";
  out << "    \"hbm_saved_gb\": \"legacy_alias_binary_gib\",\n";
  out << "    \"hbm_saved_gib\": \"binary_gib\",\n";
  out << "    \"kv_gb\": \"legacy_alias_binary_gib\",\n";
  out << "    \"kv_gib\": \"binary_gib\",\n";
  out << "    \"latency\": \"milliseconds\"\n";
  out << "  },\n";
  out << "  \"fixture_basis\": "
      << inferlab_plan::json_string(plan.assumptions.fixture_name)
      << ",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"artifact_identity\": \"deep_dive_artifact_identity.hpp/.cpp\",\n";
  out << "    \"product_identity\": \"mature_product_profile_registry.hpp\",\n";
  out << "    \"transfer_serialization\": \"runtime_communication_internal::transfer_milliseconds\",\n";
  out << "    \"tier_decision\": \"kv_tier_decision_deep_dive_model.hpp/.cpp\",\n";
  out << "    \"artifact_view\": \"kv_tier_decision_deep_dive_view.hpp/.cpp\"\n";
  out << "  },\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(research_basis(), 2) << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(products.display_names, 2)
      << ",\n";
  out << "  \"assumptions\": "
      << assumptions_json(plan.assumptions, 2) << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_scenario\": "
      << inferlab_plan::json_string(best.scenario.name) << ",\n";
  out << "    \"best_deep_dive_score\": "
      << inferlab_plan::fixed6(best.deep_dive_score) << ",\n";
  out << "    \"external_tier_selected_count\": "
      << plan.external_tier_selected_count << ",\n";
  out << "    \"hbm_selected_count\": " << plan.hbm_selected_count
      << ",\n";
  out << "    \"highest_hbm_pressure\": "
      << inferlab_plan::fixed6(highest_pressure.hbm_pressure) << ",\n";
  out << "    \"highest_hbm_pressure_scenario\": "
      << inferlab_plan::json_string(highest_pressure.scenario.name)
      << ",\n";
  out << "    \"recompute_selected_count\": "
      << plan.recompute_selected_count << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(inputs, 2) << ",\n";
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

}  // namespace inferlab::kv_tier_decision_deep_dive
