#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace inferlab::kv_tier_decision_deep_dive {

enum class TierKind {
  hbm_keep,
  cpu_dram_offload,
  nvme_offload,
  remote_kv_store,
  prefill_recompute,
};

enum class CandidateDisposition {
  selected_modeled_candidate,
  eligible_not_selected_due_to_higher_expected_cost,
  eligible_not_selected_due_to_tie_break,
  hbm_capacity_insufficient,
  transfer_path_unavailable,
  correctness_risk_exceeds_policy,
};

enum class Decision {
  prefer_hbm_keep,
  prefer_kv_reuse_or_offload_tier,
  prefer_recompute,
};

enum class Recommendation {
  ready_to_align_with_product_replay,
  require_reuse_correctness_contract,
  collect_real_tier_hit_and_restore_latency,
  keep_as_dry_run_until_connector_evidence_exists,
};

struct TransferPath {
  double bandwidth_gbps = 0.0;
  double setup_ms = 0.0;
  double sender_wait_ms = 0.0;
  double receiver_wait_ms = 0.0;
  double restore_ms = 0.0;
};

struct Assumptions {
  std::string fixture_name =
      "compiled_kv_tier_decision_analytical_fixture_v2";
  double bytes_per_gib = 1024.0 * 1024.0 * 1024.0;
  int max_tokens = 16777216;
  double max_kv_bytes_per_token = 16777216.0;
  double hbm_lookup_ms = 0.15;
  double expected_cost_tie_tolerance_ms = 1e-6;
  double maximum_reuse_correctness_risk = 0.15;
  double replay_alignment_score_threshold = 0.72;
  double hbm_headroom_weight = 0.20;
  double hit_probability_weight = 0.30;
  double correctness_weight = 0.20;
  double latency_advantage_weight = 0.30;
};

struct Scenario {
  std::string name;
  std::vector<std::string> product_profile_ids;
  int prompt_tokens = 0;
  int decode_tokens = 0;
  int prefill_recompute_tokens = 0;
  double kv_bytes_per_token = 0.0;
  double transfer_payload_ratio = 1.0;
  double hbm_free_gib = 0.0;
  TransferPath cpu_dram;
  TransferPath nvme;
  TransferPath remote;
  double prefill_ms_per_token = 0.0;
  double hit_probability = 0.0;
  double correctness_risk = 0.0;
};

struct TierCandidate {
  TierKind tier = TierKind::prefill_recompute;
  CandidateDisposition disposition =
      CandidateDisposition::
          eligible_not_selected_due_to_higher_expected_cost;
  double payload_gib = 0.0;
  double bandwidth_gbps = 0.0;
  double setup_ms = 0.0;
  double sender_wait_ms = 0.0;
  double receiver_wait_ms = 0.0;
  double restore_ms = 0.0;
  double serialization_ms = 0.0;
  double latency_ms = 0.0;
  double hbm_saved_gib = 0.0;
  double expected_cost_ms = 0.0;
  bool expected_cost_modeled = false;
  bool requires_external_system = false;
  bool analytically_eligible = false;
  bool selected = false;
};

struct Result {
  Scenario scenario;
  std::int64_t kv_token_count = 0;
  double kv_bytes = 0.0;
  double kv_gib = 0.0;
  double transfer_payload_gib = 0.0;
  double hbm_shortfall_gib = 0.0;
  double hbm_pressure = 0.0;
  double prefill_recompute_ms = 0.0;
  std::size_t selected_candidate_index = 0U;
  double selected_latency_ms = 0.0;
  double selected_expected_cost_ms = 0.0;
  double net_latency_delta_ms = 0.0;
  double latency_advantage_score = 0.0;
  double deep_dive_score = 0.0;
  Decision decision = Decision::prefer_recompute;
  Recommendation recommendation =
      Recommendation::keep_as_dry_run_until_connector_evidence_exists;
  std::vector<TierCandidate> candidates;
};

struct Plan {
  Assumptions assumptions;
  std::vector<Result> records;
  std::size_t best_record_index = 0U;
  std::size_t highest_pressure_record_index = 0U;
  std::size_t hbm_selected_count = 0U;
  std::size_t external_tier_selected_count = 0U;
  std::size_t recompute_selected_count = 0U;
};

const char* tier_kind_name(TierKind tier);
const char* candidate_disposition_name(CandidateDisposition disposition);
const char* decision_name(Decision decision);
const char* recommendation_name(Recommendation recommendation);

Assumptions default_assumptions();
std::vector<Scenario> default_scenarios();
Result analyze(const Assumptions& assumptions, const Scenario& scenario);
Plan build_plan(const Assumptions& assumptions,
                const std::vector<Scenario>& scenarios);

}  // namespace inferlab::kv_tier_decision_deep_dive
