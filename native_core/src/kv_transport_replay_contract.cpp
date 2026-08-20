#include "kv_transport_replay_contract.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::kv_transport_replay_contract {
namespace {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;

double effective_bandwidth(const double kv_gib,
                           const double elapsed_ms,
                           const char* name) {
  if (kv_gib <= 0.0) {
    return 0.0;
  }
  require_positive_finite(elapsed_ms, name);
  const double payload_gigabit_ms =
      runtime_communication_internal::
          gib_payload_gigabit_milliseconds(kv_gib, name);
  return require_finite(payload_gigabit_ms / elapsed_ms, name);
}

double transport_score(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayResult& result) {
  if (!result.active_transfer_workload) {
    return 0.0;
  }
  const double normalized_bandwidth =
      clamp_unit_interval(require_finite(
          result.effective_bandwidth_gbps /
              assumptions.bandwidth_score_reference_gbps,
          "KV transport normalized bandwidth"));
  const double overlap_score = finite_product(
      result.overlap_ratio, assumptions.overlap_score_weight,
      "KV transport overlap score");
  const double bandwidth_score = finite_product(
      normalized_bandwidth, assumptions.bandwidth_score_weight,
      "KV transport bandwidth score");
  const double reliability_score = finite_product(
      1.0 - result.scenario.retry_risk,
      assumptions.reliability_score_weight,
      "KV transport reliability score");
  const double blocking_bonus =
      result.decode_blocked_ms <
              assumptions.low_blocking_threshold_ms
          ? assumptions.low_blocking_bonus_weight
          : 0.0;
  double score = internal::checked_add_finite_double(
      overlap_score, bandwidth_score,
      "KV transport aggregate score");
  score = internal::checked_add_finite_double(
      score, reliability_score,
      "KV transport aggregate score");
  score = internal::checked_add_finite_double(
      score, blocking_bonus,
      "KV transport aggregate score");
  return clamp_unit_interval(score);
}

std::string recommendation(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayResult& result) {
  if (!result.active_transfer_workload) {
    return "no_KV_payload_to_transfer";
  }
  if (result.transport_score >=
      assumptions.preferred_transport_score_threshold) {
    return "prefer_transfer_over_recompute_for_warm_kv";
  }
  if (result.decode_blocked_ms >
      assumptions.recompute_blocked_threshold_ms) {
    return "recompute_or_reduce_kv_payload_before_transfer";
  }
  return "enable_overlap_and_profile_transport_on_target_cluster";
}

std::vector<KvTransportTimelineEvent> build_timeline(
    const KvTransportReplayResult& result) {
  const double transfer_start =
      internal::checked_add_finite_double(
          result.scenario.setup_ms,
          result.scenario.sender_wait_ms,
          "KV transport transfer timeline start");
  const double receiver_start =
      internal::checked_add_finite_double(
          transfer_start, result.payload_serialization_ms,
          "KV transport receiver timeline start");
  return {
      {KvTransportTimelinePhase::prepare_descriptor, 0.0,
       result.scenario.setup_ms},
      {KvTransportTimelinePhase::sender_wait,
       result.scenario.setup_ms,
       result.scenario.sender_wait_ms},
      {KvTransportTimelinePhase::kv_transfer, transfer_start,
       result.payload_serialization_ms},
      {KvTransportTimelinePhase::receiver_wait, receiver_start,
       result.scenario.receiver_wait_ms},
      {KvTransportTimelinePhase::decode_blocked,
       result.overlap_hidden_ms, result.decode_blocked_ms},
  };
}

KvTransportReplayResult derive_result(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayScenario& scenario) {
  KvTransportReplayResult result;
  result.scenario = scenario;
  result.active_transfer_workload = scenario.kv_gib > 0.0;
  result.ranking_applicable = result.active_transfer_workload;
  result.kv_bytes = finite_product(
      scenario.kv_gib, assumptions.bytes_per_gib,
      "KV transport payload bytes");
  result.payload_serialization_ms =
      runtime_communication_internal::transfer_milliseconds(
          scenario.kv_gib, scenario.bandwidth_gbps,
          "KV transport payload serialization time");
  result.transfer_ms = internal::checked_add_finite_double(
      scenario.setup_ms, result.payload_serialization_ms,
      "KV transport setup-inclusive transfer time");
  double end_to_end_ms = internal::checked_add_finite_double(
      result.transfer_ms, scenario.sender_wait_ms,
      "KV transport end-to-end time");
  end_to_end_ms = internal::checked_add_finite_double(
      end_to_end_ms, scenario.receiver_wait_ms,
      "KV transport end-to-end time");
  result.end_to_end_ms = end_to_end_ms;
  result.effective_bandwidth_gbps = effective_bandwidth(
      scenario.kv_gib, result.transfer_ms,
      "KV transport setup-inclusive effective bandwidth");
  result.end_to_end_effective_bandwidth_gbps =
      effective_bandwidth(
          scenario.kv_gib, result.end_to_end_ms,
          "KV transport end-to-end effective bandwidth");
  result.sender_wait_ms = scenario.sender_wait_ms;
  result.receiver_wait_ms = scenario.receiver_wait_ms;
  result.overlap_hidden_ms =
      std::min(result.end_to_end_ms,
               scenario.compute_overlap_ms);
  result.overlap_ratio = result.end_to_end_ms > 0.0
      ? require_finite(
            result.overlap_hidden_ms / result.end_to_end_ms,
            "KV transport overlap ratio")
      : 0.0;
  result.decode_blocked_ms = require_finite(
      result.end_to_end_ms - result.overlap_hidden_ms,
      "KV transport decode-blocked time");
  result.retry_or_fallback_count =
      internal::checked_non_negative_round_double_to_int(
          finite_product(
              scenario.retry_risk,
              static_cast<double>(
                  assumptions.retry_count_reference_requests),
              "KV transport retry estimate"),
          "KV transport retry or fallback count");
  result.transport_score = transport_score(assumptions, result);
  result.timeline = build_timeline(result);
  result.recommendation = recommendation(assumptions, result);
  return result;
}

}  // namespace

void validate_assumptions(
    const KvTransportReplayAssumptions& assumptions) {
  require_nonempty(assumptions.profile_name,
                   "KV transport assumption profile");
  require_positive_finite(assumptions.bytes_per_gib,
                          "KV transport bytes per GiB");
  if (std::fabs(
          assumptions.bytes_per_gib -
          runtime_communication_internal::kBytesPerGib) >
      kComparisonTolerance) {
    throw std::invalid_argument(
        "KV transport bytes-per-GiB assumption must match the canonical unit owner");
  }
  if (assumptions.retry_count_reference_requests <= 0) {
    throw std::invalid_argument(
        "KV transport retry reference requests must be positive");
  }
  require_positive_finite(
      assumptions.bandwidth_score_reference_gbps,
      "KV transport bandwidth score reference");
  require_unit_interval(assumptions.overlap_score_weight,
                        "KV transport overlap score weight");
  require_unit_interval(assumptions.bandwidth_score_weight,
                        "KV transport bandwidth score weight");
  require_unit_interval(assumptions.reliability_score_weight,
                        "KV transport reliability score weight");
  require_unit_interval(assumptions.low_blocking_bonus_weight,
                        "KV transport low-blocking bonus weight");
  double score_weight_total =
      internal::checked_add_finite_double(
          assumptions.overlap_score_weight,
          assumptions.bandwidth_score_weight,
          "KV transport score weight total");
  score_weight_total = internal::checked_add_finite_double(
      score_weight_total,
      assumptions.reliability_score_weight,
      "KV transport score weight total");
  score_weight_total = internal::checked_add_finite_double(
      score_weight_total,
      assumptions.low_blocking_bonus_weight,
      "KV transport score weight total");
  if (score_weight_total < 1.0 - kComparisonTolerance ||
      score_weight_total > 1.0 + kComparisonTolerance) {
    throw std::invalid_argument(
        "KV transport score weights must sum to one");
  }
  require_non_negative_finite(
      assumptions.low_blocking_threshold_ms,
      "KV transport low-blocking threshold");
  require_unit_interval(
      assumptions.preferred_transport_score_threshold,
      "KV transport preferred score threshold");
  require_non_negative_finite(
      assumptions.recompute_blocked_threshold_ms,
      "KV transport recompute threshold");
  if (assumptions.recompute_blocked_threshold_ms <
      assumptions.low_blocking_threshold_ms) {
    throw std::invalid_argument(
        "KV transport recompute threshold must not be below the low-blocking threshold");
  }
}

static KvTransportReplayScenario validate_scenario(
    KvTransportReplayScenario scenario) {
  require_nonempty(scenario.name, "KV transport scenario name");
  (void)kv_transport_replay_backend_name(scenario.transport);
  scenario.kv_gib = require_non_negative_finite(
      scenario.kv_gib, "KV transport payload GiB");
  scenario.bandwidth_gbps = require_non_negative_finite(
      scenario.bandwidth_gbps, "KV transport bandwidth");
  scenario.setup_ms = require_non_negative_finite(
      scenario.setup_ms, "KV transport setup time");
  scenario.sender_wait_ms = require_non_negative_finite(
      scenario.sender_wait_ms, "KV transport sender wait");
  scenario.receiver_wait_ms = require_non_negative_finite(
      scenario.receiver_wait_ms, "KV transport receiver wait");
  scenario.compute_overlap_ms = require_non_negative_finite(
      scenario.compute_overlap_ms, "KV transport compute overlap");
  scenario.retry_risk = require_unit_interval(
      scenario.retry_risk, "KV transport retry risk");
  if (scenario.kv_gib > 0.0) {
    require_positive_finite(
        scenario.bandwidth_gbps,
        "KV transport active bandwidth");
  } else if (scenario.setup_ms != 0.0 ||
             scenario.sender_wait_ms != 0.0 ||
             scenario.receiver_wait_ms != 0.0 ||
             scenario.compute_overlap_ms != 0.0 ||
             scenario.retry_risk != 0.0) {
    throw std::invalid_argument(
        "KV transport zero-payload scenarios cannot carry transport work or risk");
  }
  return scenario;
}

KvTransportReplayResult build_result(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayScenario& input_scenario) {
  validate_assumptions(assumptions);
  const KvTransportReplayScenario scenario =
      validate_scenario(input_scenario);
  return derive_result(assumptions, scenario);
}

}  // namespace inferlab::kv_transport_replay_contract
