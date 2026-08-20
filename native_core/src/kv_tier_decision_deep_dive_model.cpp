#include "kv_tier_decision_deep_dive_model.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"
#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace inferlab::kv_tier_decision_deep_dive {
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
                   "KV tier decision fixture name");
  require_positive_finite(assumptions.bytes_per_gib,
                          "KV tier decision bytes per GiB");
  if (assumptions.max_tokens <= 0) {
    throw std::invalid_argument(
        "KV tier decision maximum token count must be positive");
  }
  require_positive_finite(assumptions.max_kv_bytes_per_token,
                          "KV tier decision maximum KV bytes per token");
  require_non_negative_finite(assumptions.hbm_lookup_ms,
                              "KV tier decision HBM lookup latency");
  require_non_negative_finite(
      assumptions.expected_cost_tie_tolerance_ms,
      "KV tier decision expected-cost tie tolerance");
  require_unit_interval(
      assumptions.maximum_reuse_correctness_risk,
      "KV tier decision maximum reuse correctness risk");
  require_unit_interval(
      assumptions.replay_alignment_score_threshold,
      "KV tier decision replay alignment threshold");

  const double hbm_weight = require_unit_interval(
      assumptions.hbm_headroom_weight,
      "KV tier decision HBM headroom weight");
  const double hit_weight = require_unit_interval(
      assumptions.hit_probability_weight,
      "KV tier decision hit probability weight");
  const double correctness_weight = require_unit_interval(
      assumptions.correctness_weight,
      "KV tier decision correctness weight");
  const double latency_weight = require_unit_interval(
      assumptions.latency_advantage_weight,
      "KV tier decision latency advantage weight");
  double weight_sum = internal::checked_add_finite_double(
      hbm_weight, hit_weight, "KV tier decision score weight sum");
  weight_sum = internal::checked_add_finite_double(
      weight_sum, correctness_weight,
      "KV tier decision score weight sum");
  weight_sum = internal::checked_add_finite_double(
      weight_sum, latency_weight, "KV tier decision score weight sum");
  if (std::fabs(weight_sum - 1.0) > 1e-12) {
    throw std::invalid_argument(
        "KV tier decision score weights must sum to one");
  }
}

TransferPath validate_transfer_path(TransferPath path,
                                    const char* path_name) {
  path.bandwidth_gbps = require_non_negative_finite(
      path.bandwidth_gbps, path_name);
  path.setup_ms = require_non_negative_finite(path.setup_ms, path_name);
  path.sender_wait_ms = require_non_negative_finite(
      path.sender_wait_ms, path_name);
  path.receiver_wait_ms = require_non_negative_finite(
      path.receiver_wait_ms, path_name);
  path.restore_ms = require_non_negative_finite(path.restore_ms, path_name);
  if (path.bandwidth_gbps == 0.0 &&
      (path.setup_ms != 0.0 || path.sender_wait_ms != 0.0 ||
       path.receiver_wait_ms != 0.0 || path.restore_ms != 0.0)) {
    throw std::invalid_argument(
        std::string(path_name) +
        " cannot carry latency when its bandwidth is zero");
  }
  return path;
}

Scenario validate_scenario(const Assumptions& assumptions,
                           Scenario scenario) {
  require_nonempty(scenario.name, "KV tier decision scenario name");
  require_unique_nonempty_strings(
      scenario.product_profile_ids,
      "KV tier decision product profile ids");
  if (scenario.product_profile_ids.empty()) {
    throw std::invalid_argument(
        "KV tier decision scenario requires product alignment ids");
  }
  for (const auto& id : scenario.product_profile_ids) {
    (void)mature_product_profiles::require_product_profile(id);
  }
  if (scenario.prompt_tokens <= 0 || scenario.decode_tokens < 0 ||
      scenario.prefill_recompute_tokens < 0 ||
      scenario.prefill_recompute_tokens > scenario.prompt_tokens) {
    throw std::invalid_argument(
        "KV tier decision requires positive prompt tokens, non-negative decode tokens, and bounded prefill recompute tokens");
  }
  const int token_count = internal::checked_add_int(
      scenario.prompt_tokens, scenario.decode_tokens,
      "KV tier decision token count");
  if (token_count > assumptions.max_tokens) {
    throw std::invalid_argument(
        "KV tier decision token count exceeds the analytical fixture range");
  }
  scenario.kv_bytes_per_token = require_positive_finite(
      scenario.kv_bytes_per_token,
      "KV tier decision KV bytes per token");
  if (scenario.kv_bytes_per_token >
      assumptions.max_kv_bytes_per_token) {
    throw std::invalid_argument(
        "KV tier decision KV bytes per token exceeds the analytical fixture range");
  }
  scenario.transfer_payload_ratio = require_positive_finite(
      scenario.transfer_payload_ratio,
      "KV tier decision transfer payload ratio");
  if (scenario.transfer_payload_ratio > 1.0) {
    throw std::invalid_argument(
        "KV tier decision transfer payload ratio must not exceed one");
  }
  scenario.hbm_free_gib = require_non_negative_finite(
      scenario.hbm_free_gib, "KV tier decision HBM free GiB");
  scenario.cpu_dram = validate_transfer_path(
      scenario.cpu_dram, "KV tier decision CPU DRAM transfer path");
  scenario.nvme = validate_transfer_path(
      scenario.nvme, "KV tier decision NVMe transfer path");
  scenario.remote = validate_transfer_path(
      scenario.remote, "KV tier decision remote transfer path");
  scenario.prefill_ms_per_token = require_positive_finite(
      scenario.prefill_ms_per_token,
      "KV tier decision prefill latency per token");
  scenario.hit_probability = require_unit_interval(
      scenario.hit_probability,
      "KV tier decision hit probability");
  scenario.correctness_risk = require_unit_interval(
      scenario.correctness_risk,
      "KV tier decision correctness risk");
  return scenario;
}

double transfer_latency_ms(const double payload_gib,
                           const TransferPath& path,
                           double& serialization_ms) {
  serialization_ms = 0.0;
  if (path.bandwidth_gbps <= 0.0) return 0.0;
  serialization_ms =
      runtime_communication_internal::transfer_milliseconds(
          payload_gib, path.bandwidth_gbps,
          "KV tier decision transfer serialization");
  double total = internal::checked_add_finite_double(
      path.setup_ms, path.sender_wait_ms,
      "KV tier decision transfer latency");
  total = internal::checked_add_finite_double(
      total, serialization_ms, "KV tier decision transfer latency");
  total = internal::checked_add_finite_double(
      total, path.receiver_wait_ms,
      "KV tier decision transfer latency");
  return internal::checked_add_finite_double(
      total, path.restore_ms, "KV tier decision transfer latency");
}

double expected_restore_cost_ms(const double hit_probability,
                                const double restore_latency_ms,
                                const double prefill_recompute_ms) {
  const double hit_cost = finite_product(
      hit_probability, restore_latency_ms,
      "KV tier decision hit-weighted restore cost");
  const double miss_cost = finite_product(
      1.0 - hit_probability, prefill_recompute_ms,
      "KV tier decision miss-weighted recompute cost");
  return internal::checked_add_finite_double(
      hit_cost, miss_cost, "KV tier decision expected restore cost");
}

TierCandidate hbm_candidate(const Assumptions& assumptions,
                            const double kv_gib,
                            const double hbm_free_gib) {
  TierCandidate candidate;
  candidate.tier = TierKind::hbm_keep;
  candidate.latency_ms = assumptions.hbm_lookup_ms;
  candidate.expected_cost_ms = assumptions.hbm_lookup_ms;
  candidate.expected_cost_modeled = true;
  candidate.analytically_eligible = kv_gib <= hbm_free_gib;
  candidate.disposition = candidate.analytically_eligible
                              ? CandidateDisposition::
                                    eligible_not_selected_due_to_higher_expected_cost
                              : CandidateDisposition::hbm_capacity_insufficient;
  return candidate;
}

TierCandidate transfer_candidate(
    const Assumptions& assumptions,
    const Scenario& scenario,
    const TierKind tier,
    const TransferPath& path,
    const double kv_gib,
    const double payload_gib,
    const double prefill_recompute_ms) {
  TierCandidate candidate;
  candidate.tier = tier;
  candidate.payload_gib = payload_gib;
  candidate.bandwidth_gbps = path.bandwidth_gbps;
  candidate.setup_ms = path.setup_ms;
  candidate.sender_wait_ms = path.sender_wait_ms;
  candidate.receiver_wait_ms = path.receiver_wait_ms;
  candidate.restore_ms = path.restore_ms;
  candidate.hbm_saved_gib = kv_gib;
  candidate.requires_external_system = true;
  candidate.latency_ms = transfer_latency_ms(
      payload_gib, path, candidate.serialization_ms);
  if (path.bandwidth_gbps <= 0.0) {
    candidate.disposition =
        CandidateDisposition::transfer_path_unavailable;
    return candidate;
  }
  candidate.expected_cost_ms = expected_restore_cost_ms(
      scenario.hit_probability, candidate.latency_ms,
      prefill_recompute_ms);
  candidate.expected_cost_modeled = true;
  if (scenario.correctness_risk >
      assumptions.maximum_reuse_correctness_risk) {
    candidate.disposition =
        CandidateDisposition::correctness_risk_exceeds_policy;
    return candidate;
  }
  candidate.analytically_eligible = true;
  candidate.disposition = CandidateDisposition::
      eligible_not_selected_due_to_higher_expected_cost;
  return candidate;
}

TierCandidate recompute_candidate(const double kv_gib,
                                  const double prefill_recompute_ms) {
  TierCandidate candidate;
  candidate.tier = TierKind::prefill_recompute;
  candidate.latency_ms = prefill_recompute_ms;
  candidate.hbm_saved_gib = kv_gib;
  candidate.expected_cost_ms = prefill_recompute_ms;
  candidate.expected_cost_modeled = true;
  candidate.analytically_eligible = true;
  candidate.disposition = CandidateDisposition::
      eligible_not_selected_due_to_higher_expected_cost;
  return candidate;
}

bool is_external_tier(const TierKind tier) {
  return tier == TierKind::cpu_dram_offload ||
         tier == TierKind::nvme_offload ||
         tier == TierKind::remote_kv_store;
}

int tie_break_priority(const TierKind tier) {
  switch (tier) {
    case TierKind::hbm_keep:
      return 0;
    case TierKind::prefill_recompute:
      return 1;
    case TierKind::cpu_dram_offload:
      return 2;
    case TierKind::nvme_offload:
      return 3;
    case TierKind::remote_kv_store:
      return 4;
  }
  throw std::logic_error("unknown KV tier tie-break priority");
}

bool prefer_candidate(const TierCandidate& candidate,
                      const TierCandidate& current,
                      const double tie_tolerance_ms) {
  const double delta = require_finite(
      candidate.expected_cost_ms - current.expected_cost_ms,
      "KV tier decision candidate expected-cost delta");
  if (delta < -tie_tolerance_ms) return true;
  if (std::fabs(delta) > tie_tolerance_ms) return false;
  return tie_break_priority(candidate.tier) <
         tie_break_priority(current.tier);
}

void validate_result(const Assumptions& assumptions,
                     const Result& result) {
  constexpr std::size_t kExpectedCandidateCount = 5U;
  if (result.candidates.size() != kExpectedCandidateCount ||
      result.selected_candidate_index >= result.candidates.size() ||
      result.kv_token_count <= 0 || result.kv_bytes <= 0.0 ||
      result.kv_gib <= 0.0 || result.transfer_payload_gib <= 0.0) {
    throw std::logic_error(
        "KV tier decision result contains invalid cardinality facts");
  }
  const auto& selected_candidate =
      result.candidates[result.selected_candidate_index];
  std::size_t selected_count = 0U;
  for (const auto& candidate : result.candidates) {
    if (candidate.selected) {
      selected_count = internal::checked_add_size(
          selected_count, 1U,
          "KV tier decision selected candidate count");
      if (!candidate.analytically_eligible ||
          candidate.disposition !=
              CandidateDisposition::selected_modeled_candidate) {
        throw std::logic_error(
            "KV tier decision selected candidate is not eligible");
      }
    }
    require_non_negative_finite(candidate.payload_gib,
                                "KV tier decision candidate payload");
    require_non_negative_finite(candidate.latency_ms,
                                "KV tier decision candidate latency");
    require_non_negative_finite(candidate.expected_cost_ms,
                                "KV tier decision candidate expected cost");
    if (candidate.expected_cost_modeled !=
        (candidate.tier == TierKind::hbm_keep ||
         candidate.tier == TierKind::prefill_recompute ||
         candidate.bandwidth_gbps > 0.0)) {
      throw std::logic_error(
          "KV tier decision expected-cost availability is inconsistent");
    }
    if (candidate.analytically_eligible && !candidate.selected &&
        candidate.disposition != CandidateDisposition::
            eligible_not_selected_due_to_higher_expected_cost &&
        candidate.disposition != CandidateDisposition::
            eligible_not_selected_due_to_tie_break) {
      throw std::logic_error(
          "KV tier decision eligible candidate disposition is inconsistent");
    }
    if (candidate.analytically_eligible && !candidate.selected) {
      const double delta = require_finite(
          candidate.expected_cost_ms -
              selected_candidate.expected_cost_ms,
          "KV tier decision eligible candidate reconciliation delta");
      if (delta < -assumptions.expected_cost_tie_tolerance_ms ||
          (std::fabs(delta) <=
               assumptions.expected_cost_tie_tolerance_ms &&
           candidate.disposition != CandidateDisposition::
               eligible_not_selected_due_to_tie_break) ||
          (delta > assumptions.expected_cost_tie_tolerance_ms &&
           candidate.disposition != CandidateDisposition::
               eligible_not_selected_due_to_higher_expected_cost)) {
        throw std::logic_error(
            "KV tier decision non-selected candidate cost is inconsistent");
      }
    }
  }
  if (selected_count != 1U ||
      !result.candidates[result.selected_candidate_index].selected) {
    throw std::logic_error(
        "KV tier decision selected candidate reconciliation failed");
  }
  require_non_negative_finite(result.hbm_shortfall_gib,
                              "KV tier decision HBM shortfall result");
  require_unit_interval(result.hbm_pressure,
                        "KV tier decision HBM pressure result");
  require_non_negative_finite(result.prefill_recompute_ms,
                              "KV tier decision prefill result");
  require_non_negative_finite(result.selected_latency_ms,
                              "KV tier decision selected latency result");
  require_non_negative_finite(
      result.selected_expected_cost_ms,
      "KV tier decision selected expected cost result");
  require_finite(result.net_latency_delta_ms,
                 "KV tier decision latency delta result");
  require_unit_interval(result.latency_advantage_score,
                        "KV tier decision latency advantage result");
  require_unit_interval(result.deep_dive_score,
                        "KV tier decision score result");
  const auto recompute = std::find_if(
      result.candidates.begin(), result.candidates.end(),
      [](const TierCandidate& candidate) {
        return candidate.tier == TierKind::prefill_recompute;
      });
  if (recompute == result.candidates.end()) {
    throw std::logic_error(
        "KV tier decision recompute candidate is missing");
  }
  if (is_external_tier(selected_candidate.tier)) {
    const double delta = require_finite(
        selected_candidate.expected_cost_ms - recompute->expected_cost_ms,
        "KV tier decision selected external expected-cost delta");
    if (delta >= -assumptions.expected_cost_tie_tolerance_ms) {
      throw std::logic_error(
          "KV tier decision external candidate did not strictly beat recompute");
    }
  }
}

}  // namespace

const char* tier_kind_name(const TierKind tier) {
  switch (tier) {
    case TierKind::hbm_keep:
      return "hbm_keep";
    case TierKind::cpu_dram_offload:
      return "cpu_dram_offload";
    case TierKind::nvme_offload:
      return "nvme_offload";
    case TierKind::remote_kv_store:
      return "remote_kv_store";
    case TierKind::prefill_recompute:
      return "prefill_recompute";
  }
  throw std::logic_error("unknown KV tier kind");
}

const char* candidate_disposition_name(
    const CandidateDisposition disposition) {
  switch (disposition) {
    case CandidateDisposition::selected_modeled_candidate:
      return "selected_modeled_candidate";
    case CandidateDisposition::
        eligible_not_selected_due_to_higher_expected_cost:
      return "eligible_not_selected_due_to_higher_expected_cost";
    case CandidateDisposition::eligible_not_selected_due_to_tie_break:
      return "eligible_not_selected_due_to_conservative_tie_break";
    case CandidateDisposition::hbm_capacity_insufficient:
      return "hbm_capacity_insufficient";
    case CandidateDisposition::transfer_path_unavailable:
      return "transfer_path_unavailable";
    case CandidateDisposition::correctness_risk_exceeds_policy:
      return "correctness_risk_exceeds_policy";
  }
  throw std::logic_error("unknown KV tier candidate disposition");
}

const char* decision_name(const Decision decision) {
  switch (decision) {
    case Decision::prefer_hbm_keep:
      return "prefer_hbm_keep";
    case Decision::prefer_kv_reuse_or_offload_tier:
      return "prefer_kv_reuse_or_offload_tier";
    case Decision::prefer_recompute:
      return "prefer_recompute";
  }
  throw std::logic_error("unknown KV tier decision");
}

const char* recommendation_name(const Recommendation recommendation) {
  switch (recommendation) {
    case Recommendation::ready_to_align_with_product_replay:
      return "ready_to_align_with_lmcache_mooncake_or_vllm_replay";
    case Recommendation::require_reuse_correctness_contract:
      return "require_reuse_correctness_contract_before_tier_claim";
    case Recommendation::collect_real_tier_hit_and_restore_latency:
      return "collect_real_tier_hit_and_restore_latency_before_release_claim";
    case Recommendation::keep_as_dry_run_until_connector_evidence_exists:
      return "keep_as_dry_run_until_product_connector_evidence_exists";
  }
  throw std::logic_error("unknown KV tier recommendation");
}

Assumptions default_assumptions() { return {}; }

std::vector<Scenario> default_scenarios() {
  return {
      {"hot_prefix_hbm_keep",
       {"vllm", "sglang"},
       4096,
       256,
       4096,
       524288.0,
       1.0,
       9.0,
       {42.0, 0.65, 0.20, 0.20, 0.0},
       {6.0, 1.80, 0.50, 0.50, 0.0},
       {90.0, 1.10, 0.50, 0.50, 0.0},
       0.018,
       0.92,
       0.02},
      {"long_context_cpu_offload",
       {"kv_offload"},
       32768,
       384,
       32768,
       524288.0,
       0.25,
       3.0,
       {38.0, 0.65, 0.40, 0.40, 0.80},
       {6.5, 1.80, 0.80, 0.80, 1.50},
       {80.0, 1.10, 0.80, 0.80, 0.90},
       0.020,
       0.74,
       0.04},
      {"archive_prefix_nvme_restore",
       {"kv_offload"},
       65536,
       128,
       65536,
       524288.0,
       0.125,
       1.5,
       {32.0, 0.65, 0.60, 0.60, 1.00},
       {5.0, 1.80, 1.20, 1.20, 2.50},
       {70.0, 1.10, 1.00, 1.00, 1.20},
       0.021,
       0.55,
       0.08},
      {"remote_kv_reuse_candidate",
       {"mooncake", "dynamo"},
       49152,
       512,
       49152,
       524288.0,
       0.25,
       2.0,
       {36.0, 0.65, 0.50, 0.50, 0.90},
       {5.5, 1.80, 1.00, 1.00, 2.00},
       {110.0, 1.10, 0.80, 0.80, 1.10},
       0.019,
       0.81,
       0.06},
      {"low_reuse_recompute_candidate",
       {"vllm"},
       8192,
       64,
       8192,
       524288.0,
       1.0,
       0.8,
       {30.0, 0.65, 0.30, 0.30, 0.50},
       {4.5, 1.80, 0.80, 0.80, 1.50},
       {45.0, 1.10, 0.60, 0.60, 0.80},
       0.016,
       0.18,
       0.03},
  };
}

Result analyze(const Assumptions& assumptions,
               const Scenario& input_scenario) {
  validate_assumptions(assumptions);
  const Scenario scenario = validate_scenario(assumptions, input_scenario);

  Result result;
  result.scenario = scenario;
  result.kv_token_count = internal::checked_add_i64(
      static_cast<std::int64_t>(scenario.prompt_tokens),
      static_cast<std::int64_t>(scenario.decode_tokens),
      "KV tier decision token count");
  result.kv_bytes = finite_product(
      static_cast<double>(result.kv_token_count),
      scenario.kv_bytes_per_token, "KV tier decision KV bytes");
  result.kv_gib = require_finite(
      result.kv_bytes / assumptions.bytes_per_gib,
      "KV tier decision KV GiB");
  result.transfer_payload_gib = finite_product(
      result.kv_gib, scenario.transfer_payload_ratio,
      "KV tier decision transfer payload GiB");
  result.hbm_shortfall_gib = std::max(
      0.0, require_finite(result.kv_gib - scenario.hbm_free_gib,
                          "KV tier decision HBM shortfall"));
  result.hbm_pressure = scenario.hbm_free_gib <= 0.0
                             ? 1.0
                             : std::min(
                                   1.0,
                                   require_finite(
                                       result.kv_gib /
                                           scenario.hbm_free_gib,
                                       "KV tier decision HBM pressure"));
  result.prefill_recompute_ms = finite_product(
      static_cast<double>(scenario.prefill_recompute_tokens),
      scenario.prefill_ms_per_token,
      "KV tier decision prefill recompute latency");

  result.candidates.reserve(5U);
  result.candidates.push_back(hbm_candidate(
      assumptions, result.kv_gib, scenario.hbm_free_gib));
  result.candidates.push_back(transfer_candidate(
      assumptions, scenario, TierKind::cpu_dram_offload,
      scenario.cpu_dram, result.kv_gib, result.transfer_payload_gib,
      result.prefill_recompute_ms));
  result.candidates.push_back(transfer_candidate(
      assumptions, scenario, TierKind::nvme_offload, scenario.nvme,
      result.kv_gib, result.transfer_payload_gib,
      result.prefill_recompute_ms));
  result.candidates.push_back(transfer_candidate(
      assumptions, scenario, TierKind::remote_kv_store, scenario.remote,
      result.kv_gib, result.transfer_payload_gib,
      result.prefill_recompute_ms));
  result.candidates.push_back(recompute_candidate(
      result.kv_gib, result.prefill_recompute_ms));

  bool selected = false;
  for (std::size_t index = 0U; index < result.candidates.size(); ++index) {
    const auto& candidate = result.candidates[index];
    if (!candidate.analytically_eligible) continue;
    if (!selected || prefer_candidate(
                         candidate,
                         result.candidates[result.selected_candidate_index],
                         assumptions.expected_cost_tie_tolerance_ms)) {
      result.selected_candidate_index = index;
      selected = true;
    }
  }
  if (!selected) {
    throw std::logic_error(
        "KV tier decision has no analytically eligible candidate");
  }
  auto& selected_candidate =
      result.candidates[result.selected_candidate_index];
  selected_candidate.selected = true;
  selected_candidate.disposition =
      CandidateDisposition::selected_modeled_candidate;
  for (auto& candidate : result.candidates) {
    if (!candidate.analytically_eligible || candidate.selected) continue;
    const double delta = require_finite(
        candidate.expected_cost_ms - selected_candidate.expected_cost_ms,
        "KV tier decision non-selected expected-cost delta");
    candidate.disposition =
        std::fabs(delta) <= assumptions.expected_cost_tie_tolerance_ms
            ? CandidateDisposition::
                  eligible_not_selected_due_to_tie_break
            : CandidateDisposition::
                  eligible_not_selected_due_to_higher_expected_cost;
  }
  result.selected_latency_ms = selected_candidate.latency_ms;
  result.selected_expected_cost_ms = selected_candidate.expected_cost_ms;
  result.net_latency_delta_ms = require_finite(
      result.selected_expected_cost_ms - result.prefill_recompute_ms,
      "KV tier decision net latency delta");
  result.latency_advantage_score =
      result.prefill_recompute_ms <= 0.0
          ? 0.0
          : std::max(
                0.0, require_finite(
                         (result.prefill_recompute_ms -
                          result.selected_expected_cost_ms) /
                             result.prefill_recompute_ms,
                         "KV tier decision latency advantage score"));

  const double hbm_headroom_score = 1.0 - result.hbm_pressure;
  double score = internal::checked_add_finite_double(
      finite_product(hbm_headroom_score,
                     assumptions.hbm_headroom_weight,
                     "KV tier decision HBM headroom score"),
      finite_product(scenario.hit_probability,
                     assumptions.hit_probability_weight,
                     "KV tier decision hit probability score"),
      "KV tier decision aggregate score");
  score = internal::checked_add_finite_double(
      score,
      finite_product(1.0 - scenario.correctness_risk,
                     assumptions.correctness_weight,
                     "KV tier decision correctness score"),
      "KV tier decision aggregate score");
  result.deep_dive_score = internal::checked_add_finite_double(
      score,
      finite_product(result.latency_advantage_score,
                     assumptions.latency_advantage_weight,
                     "KV tier decision latency advantage contribution"),
      "KV tier decision aggregate score");

  if (selected_candidate.tier == TierKind::hbm_keep) {
    result.decision = Decision::prefer_hbm_keep;
  } else if (is_external_tier(selected_candidate.tier)) {
    result.decision = Decision::prefer_kv_reuse_or_offload_tier;
  } else {
    result.decision = Decision::prefer_recompute;
  }

  if (scenario.correctness_risk >
      assumptions.maximum_reuse_correctness_risk) {
    result.recommendation =
        Recommendation::require_reuse_correctness_contract;
  } else if (is_external_tier(selected_candidate.tier) &&
             result.deep_dive_score >=
                 assumptions.replay_alignment_score_threshold) {
    result.recommendation =
        Recommendation::ready_to_align_with_product_replay;
  } else if (result.hbm_shortfall_gib > 0.0) {
    result.recommendation =
        Recommendation::collect_real_tier_hit_and_restore_latency;
  } else {
    result.recommendation =
        Recommendation::keep_as_dry_run_until_connector_evidence_exists;
  }

  validate_result(assumptions, result);
  return result;
}

Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios) {
  validate_assumptions(assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "KV tier decision scenario inventory must not be empty");
  }

  Plan plan;
  plan.assumptions = assumptions;
  plan.records.reserve(scenarios.size());
  for (std::size_t index = 0U; index < scenarios.size(); ++index) {
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (scenarios[prior].name == scenarios[index].name) {
        throw std::invalid_argument(
            "duplicate KV tier decision scenario name: " +
            scenarios[index].name);
      }
    }
    plan.records.push_back(analyze(assumptions, scenarios[index]));
    const auto& record = plan.records.back();
    const TierKind selected_tier =
        record.candidates[record.selected_candidate_index].tier;
    if (selected_tier == TierKind::hbm_keep) {
      plan.hbm_selected_count = internal::checked_add_size(
          plan.hbm_selected_count, 1U,
          "KV tier decision HBM selected count");
    } else if (is_external_tier(selected_tier)) {
      plan.external_tier_selected_count = internal::checked_add_size(
          plan.external_tier_selected_count, 1U,
          "KV tier decision external selected count");
    } else {
      plan.recompute_selected_count = internal::checked_add_size(
          plan.recompute_selected_count, 1U,
          "KV tier decision recompute selected count");
    }
    if (index == 0U || record.deep_dive_score >
                           plan.records[plan.best_record_index]
                               .deep_dive_score) {
      plan.best_record_index = index;
    }
    if (index == 0U || record.hbm_pressure >
                           plan.records[plan.highest_pressure_record_index]
                               .hbm_pressure) {
      plan.highest_pressure_record_index = index;
    }
  }
  const std::size_t selection_total = internal::checked_sum_size(
      {plan.hbm_selected_count, plan.external_tier_selected_count,
       plan.recompute_selected_count},
      "KV tier decision selection total");
  if (plan.best_record_index >= plan.records.size() ||
      plan.highest_pressure_record_index >= plan.records.size() ||
      selection_total != plan.records.size()) {
    throw std::logic_error(
        "KV tier decision plan summary reconciliation failed");
  }
  return plan;
}

}  // namespace inferlab::kv_tier_decision_deep_dive
