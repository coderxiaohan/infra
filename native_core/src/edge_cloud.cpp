#include "inferlab/edge_cloud.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace inferlab {
namespace {

using SteadyClock = std::chrono::steady_clock;

bool contains_control_byte(const std::string& value) {
  return std::any_of(value.begin(), value.end(), [](const char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte < 0x20U || byte == 0x7fU;
  });
}

void validate_identifier(const std::string& field, const std::string& value) {
  if (value.empty() || value.size() > kMaximumEdgeCloudIdentifierBytes ||
      contains_control_byte(value)) {
    throw std::invalid_argument(field + " is empty, oversized or unsafe");
  }
}

bool cloud_deadline_available(const EdgeCloudPolicyInput& policy) {
  return policy.remaining_deadline_ms >= policy.minimum_cloud_deadline_ms;
}

bool remote_eligible_without_budget(
    const EdgeCloudRequest& request,
    const EdgeCloudPolicyInput& policy,
    const EdgeCloudNetworkQualityAssessment& network_quality) {
  return request.data_classification != EdgeDataClassification::kDeviceOnly &&
         policy.remote_processing_allowed && policy.network_available &&
         network_quality.eligible && policy.cloud_transport_ready &&
         cloud_deadline_available(policy);
}

bool remote_eligible(
    const EdgeCloudRequest& request,
    const EdgeCloudPolicyInput& policy,
    const EdgeCloudNetworkQualityAssessment& network_quality,
    const EdgeCloudBudgetAssessment& budget,
    const EdgeCloudSessionRecoveryAssessment& session_recovery) {
  return remote_eligible_without_budget(request, policy, network_quality) &&
         budget.cloud_eligible && session_recovery.eligible;
}

bool budget_observation_available(const EdgeCloudBudgetPolicyInput& input) {
  return input.battery_observation_available ||
         input.local_energy_estimate_available ||
         input.cloud_cost_estimate_available;
}

std::uint64_t advanced_observation_age(const std::uint64_t age,
                                       const std::uint64_t elapsed) {
  return elapsed > kMaximumEdgeCloudObservationAgeMs - age
             ? kMaximumEdgeCloudObservationAgeMs
             : age + elapsed;
}

bool retryable_local_failure(const EdgeGenerationResult& result) {
  return result.generated_tokens == 0U &&
         (result.status == EdgeRuntimeStatus::kDependencyUnavailable ||
          result.status == EdgeRuntimeStatus::kNotLoaded ||
          result.status == EdgeRuntimeStatus::kFailed);
}

void validate_network_quality_assessment(
    const EdgeCloudNetworkQualityAssessment& assessment) {
  const std::vector<std::string> threshold_blockers = {
      "network_round_trip_time_exceeds_limit",
      "network_jitter_exceeds_limit",
      "network_packet_loss_exceeds_limit",
      "network_downlink_below_limit",
  };
  if (assessment.status == "not_required" ||
      assessment.status == "quality_eligible") {
    const bool age_presence_valid =
        assessment.status == "not_required"
            ? !assessment.observation_age_ms.has_value()
            : assessment.observation_age_ms.has_value();
    if (!assessment.eligible || !assessment.blockers.empty() ||
        !age_presence_valid) {
      throw std::invalid_argument(
          "eligible edge-cloud network assessment cannot have blockers");
    }
    return;
  }
  if (assessment.eligible || assessment.blockers.empty() ||
      assessment.blockers.size() > threshold_blockers.size()) {
    throw std::invalid_argument(
        "ineligible edge-cloud network assessment requires bounded blockers");
  }
  if (assessment.status == "observation_missing") {
    if (assessment.observation_age_ms.has_value() ||
        assessment.blockers !=
        std::vector<std::string>{"network_quality_observation_missing"}) {
      throw std::invalid_argument(
          "missing network assessment has an invalid blocker");
    }
    return;
  }
  if (assessment.status == "observation_stale") {
    if (!assessment.observation_age_ms.has_value() ||
        assessment.blockers !=
        std::vector<std::string>{"network_quality_observation_stale"}) {
      throw std::invalid_argument(
          "stale network assessment has an invalid blocker");
    }
    return;
  }
  if (assessment.status != "quality_below_policy") {
    throw std::invalid_argument(
        "edge-cloud network assessment has an unknown status");
  }
  if (!assessment.observation_age_ms.has_value()) {
    throw std::invalid_argument(
        "quality threshold assessment requires observation age");
  }
  for (std::size_t index = 0U; index < assessment.blockers.size(); ++index) {
    if (std::find(threshold_blockers.begin(), threshold_blockers.end(),
                  assessment.blockers[index]) == threshold_blockers.end()) {
      throw std::invalid_argument(
          "edge-cloud network assessment has an unknown blocker");
    }
    if (std::find(assessment.blockers.begin(),
                  assessment.blockers.begin() + index,
                  assessment.blockers[index]) !=
        assessment.blockers.begin() + index) {
      throw std::invalid_argument(
          "edge-cloud network assessment blockers must be unique");
    }
  }
}

double elapsed_milliseconds(const SteadyClock::time_point start) {
  return std::max(
      0.001,
      std::chrono::duration<double, std::milli>(SteadyClock::now() - start)
          .count());
}

std::uint64_t elapsed_milliseconds_rounded_up(
    const SteadyClock::time_point start) {
  const auto elapsed = SteadyClock::now() - start;
  auto whole = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
  if (whole < elapsed) whole += std::chrono::milliseconds(1);
  const auto milliseconds = whole.count();
  return milliseconds <= 0 ? 0U : static_cast<std::uint64_t>(milliseconds);
}

std::uint64_t remaining_deadline(
    const EdgeCloudPolicyInput& policy,
    const SteadyClock::time_point request_start) {
  const std::uint64_t elapsed = elapsed_milliseconds_rounded_up(request_start);
  return elapsed >= policy.remaining_deadline_ms
             ? 0U
             : policy.remaining_deadline_ms - elapsed;
}

EdgeCloudAttempt local_attempt(const EdgeGenerationResult& result,
                               const double duration_ms) {
  EdgeCloudAttempt attempt;
  attempt.attempt_id = "local_attempt_1";
  attempt.placement = EdgeCloudPlacement::kLocal;
  attempt.status = result.status;
  attempt.error_category = result.error_category;
  attempt.finish_reason = result.finish_reason;
  attempt.delivered_chunks = result.generated_tokens;
  attempt.reported_prompt_tokens = result.prompt_tokens;
  attempt.reported_completion_tokens = result.generated_tokens;
  attempt.duration_ms = duration_ms;
  return attempt;
}

EdgeCloudAttempt cloud_attempt(const CloudGenerationResult& result,
                               const std::size_t attempt_index) {
  EdgeCloudAttempt attempt;
  attempt.attempt_id = "cloud_attempt_" + std::to_string(attempt_index);
  attempt.placement = EdgeCloudPlacement::kCloud;
  attempt.status = result.status;
  attempt.error_category = result.error_category;
  attempt.finish_reason = result.finish_reason;
  attempt.delivered_chunks = result.delivered_chunks;
  attempt.reported_prompt_tokens = result.reported_prompt_tokens;
  attempt.reported_completion_tokens = result.reported_completion_tokens;
  attempt.duration_ms = result.metrics.request_duration_ms;
  return attempt;
}

void project_local_result(const EdgeGenerationResult& source,
                          EdgeCloudGenerationResult* destination) {
  destination->status = source.status;
  destination->error_category = source.error_category;
  destination->detail = source.detail;
  destination->final_placement = EdgeCloudPlacement::kLocal;
  destination->reported_prompt_tokens = source.prompt_tokens;
  destination->reported_completion_tokens = source.generated_tokens;
  destination->finish_reason = source.finish_reason;
}

void project_cloud_result(const CloudGenerationResult& source,
                          EdgeCloudGenerationResult* destination) {
  destination->status = source.status;
  destination->error_category = source.error_category;
  destination->detail = source.detail;
  destination->final_placement = EdgeCloudPlacement::kCloud;
  destination->reported_prompt_tokens = source.reported_prompt_tokens;
  destination->reported_completion_tokens =
      source.reported_completion_tokens;
  destination->finish_reason = source.finish_reason;
}

}  // namespace

const char* edge_cloud_placement_name(
    const EdgeCloudPlacement placement) noexcept {
  switch (placement) {
    case EdgeCloudPlacement::kLocal:
      return "local";
    case EdgeCloudPlacement::kCloud:
      return "cloud";
    case EdgeCloudPlacement::kLocalThenCloud:
      return "local_then_cloud";
    case EdgeCloudPlacement::kReject:
      return "reject";
  }
  return "unknown";
}

const char* edge_data_classification_name(
    const EdgeDataClassification classification) noexcept {
  switch (classification) {
    case EdgeDataClassification::kPublic:
      return "public";
    case EdgeDataClassification::kInternal:
      return "internal";
    case EdgeDataClassification::kSensitive:
      return "sensitive";
    case EdgeDataClassification::kDeviceOnly:
      return "device_only";
  }
  return "unknown";
}

EdgeCloudPlacement parse_edge_cloud_placement(const std::string& value) {
  if (value == "local") return EdgeCloudPlacement::kLocal;
  if (value == "cloud") return EdgeCloudPlacement::kCloud;
  if (value == "local_then_cloud") {
    return EdgeCloudPlacement::kLocalThenCloud;
  }
  if (value == "reject") return EdgeCloudPlacement::kReject;
  throw std::invalid_argument("unknown edge-cloud placement: " + value);
}

EdgeDataClassification parse_edge_data_classification(
    const std::string& value) {
  if (value == "public") return EdgeDataClassification::kPublic;
  if (value == "internal") return EdgeDataClassification::kInternal;
  if (value == "sensitive") return EdgeDataClassification::kSensitive;
  if (value == "device_only") return EdgeDataClassification::kDeviceOnly;
  throw std::invalid_argument("unknown edge-cloud data classification: " +
                              value);
}

void validate_edge_cloud_request(const EdgeCloudRequest& request) {
  validate_identifier("edge-cloud request_id", request.request_id);
  validate_edge_cloud_provenance_label("edge-cloud session_id",
                                       request.session_id);
  if (request.session_id == "not_required") {
    throw std::invalid_argument(
        "edge-cloud session_id cannot use the not-required placeholder");
  }
  validate_identifier("edge-cloud cloud_model_id", request.cloud_model_id);
  validate_edge_generation_request(request.generation);
  validate_edge_cloud_session_recovery_input(request);
}

void validate_edge_cloud_provenance_label(const std::string& field,
                                          const std::string& value) {
  validate_identifier(field, value);
  if (!std::all_of(value.begin(), value.end(), [](const char character) {
        const auto byte = static_cast<unsigned char>(character);
        return std::isalnum(byte) || character == '_' || character == '-' ||
               character == '.' || character == ':';
      })) {
    throw std::invalid_argument(field + " must be a stable label");
  }
}

void validate_edge_cloud_network_quality_input(
    const EdgeCloudNetworkQualityInput& input) {
  constexpr std::uint64_t kMaximumNetworkQualityPolicyAgeMs =
      7U * 24U * 60U * 60U * 1000U;
  constexpr double kMaximumNetworkLatencyMs = 60000.0;
  constexpr double kMaximumNetworkDownlinkMbps = 10000000.0;
  validate_edge_cloud_provenance_label(
      "edge-cloud network quality provenance", input.provenance);
  const auto finite_non_negative = [](const double value) {
    return std::isfinite(value) && value >= 0.0;
  };
  if (!std::isfinite(input.maximum_round_trip_time_ms) ||
      input.maximum_round_trip_time_ms <= 0.0 ||
      input.maximum_round_trip_time_ms > kMaximumNetworkLatencyMs ||
      !std::isfinite(input.maximum_jitter_ms) ||
      input.maximum_jitter_ms <= 0.0 ||
      input.maximum_jitter_ms > kMaximumNetworkLatencyMs ||
      !std::isfinite(input.maximum_packet_loss_ratio) ||
      input.maximum_packet_loss_ratio < 0.0 ||
      input.maximum_packet_loss_ratio > 1.0 ||
      !std::isfinite(input.minimum_downlink_mbps) ||
      input.minimum_downlink_mbps <= 0.0 ||
      input.minimum_downlink_mbps > kMaximumNetworkDownlinkMbps ||
      input.maximum_observation_age_ms == 0U ||
      input.maximum_observation_age_ms >
          kMaximumNetworkQualityPolicyAgeMs ||
      input.observation_age_ms >
          kMaximumEdgeCloudObservationAgeMs) {
    throw std::invalid_argument(
        "edge-cloud network quality thresholds are outside bounds");
  }
  if (!input.policy_enabled) {
    if (input.observation_available || input.round_trip_time_ms != 0.0 ||
        input.jitter_ms != 0.0 || input.packet_loss_ratio != 0.0 ||
        input.downlink_mbps != 0.0 || input.observation_age_ms != 0U) {
      throw std::invalid_argument(
          "disabled edge-cloud network quality policy cannot carry an observation");
    }
    return;
  }
  if (!input.observation_available) {
    if (input.round_trip_time_ms != 0.0 || input.jitter_ms != 0.0 ||
        input.packet_loss_ratio != 0.0 || input.downlink_mbps != 0.0 ||
        input.observation_age_ms != 0U) {
      throw std::invalid_argument(
          "missing edge-cloud network observation must use zero values");
    }
    return;
  }
  if (!finite_non_negative(input.round_trip_time_ms) ||
      input.round_trip_time_ms > kMaximumNetworkLatencyMs ||
      !finite_non_negative(input.jitter_ms) ||
      input.jitter_ms > kMaximumNetworkLatencyMs ||
      !finite_non_negative(input.packet_loss_ratio) ||
      input.packet_loss_ratio > 1.0 ||
      !std::isfinite(input.downlink_mbps) || input.downlink_mbps <= 0.0 ||
      input.downlink_mbps > kMaximumNetworkDownlinkMbps) {
    throw std::invalid_argument(
        "edge-cloud network observation is outside bounds");
  }
}

EdgeCloudNetworkQualityAssessment assess_edge_cloud_network_quality(
    const EdgeCloudNetworkQualityInput& input) {
  validate_edge_cloud_network_quality_input(input);
  EdgeCloudNetworkQualityAssessment assessment;
  if (!input.policy_enabled) {
    validate_network_quality_assessment(assessment);
    return assessment;
  }
  assessment.eligible = false;
  if (!input.observation_available) {
    assessment.status = "observation_missing";
    assessment.blockers.push_back("network_quality_observation_missing");
    validate_network_quality_assessment(assessment);
    return assessment;
  }
  assessment.observation_age_ms = input.observation_age_ms;
  if (input.observation_age_ms > input.maximum_observation_age_ms) {
    assessment.status = "observation_stale";
    assessment.blockers.push_back("network_quality_observation_stale");
    validate_network_quality_assessment(assessment);
    return assessment;
  }
  if (input.round_trip_time_ms > input.maximum_round_trip_time_ms) {
    assessment.blockers.push_back("network_round_trip_time_exceeds_limit");
  }
  if (input.jitter_ms > input.maximum_jitter_ms) {
    assessment.blockers.push_back("network_jitter_exceeds_limit");
  }
  if (input.packet_loss_ratio > input.maximum_packet_loss_ratio) {
    assessment.blockers.push_back("network_packet_loss_exceeds_limit");
  }
  if (input.downlink_mbps < input.minimum_downlink_mbps) {
    assessment.blockers.push_back("network_downlink_below_limit");
  }
  if (!assessment.blockers.empty()) {
    assessment.status = "quality_below_policy";
    validate_network_quality_assessment(assessment);
    return assessment;
  }
  assessment.eligible = true;
  assessment.status = "quality_eligible";
  validate_network_quality_assessment(assessment);
  return assessment;
}

void validate_edge_cloud_policy_input(const EdgeCloudPolicyInput& policy) {
  if (policy.minimum_cloud_deadline_ms == 0U ||
      policy.minimum_cloud_deadline_ms >
          kMaximumEdgeGenerationDurationMs ||
      policy.remaining_deadline_ms > kMaximumEdgeGenerationDurationMs) {
    throw std::invalid_argument(
        "edge-cloud deadline policy is outside the canonical duration bounds");
  }
  validate_edge_cloud_network_quality_input(policy.network_quality);
  validate_edge_cloud_budget_policy_input(policy.budget);
}

EdgeCloudDecision select_edge_cloud_placement(
    const EdgeCloudRequest& request, const EdgeCloudPolicyInput& policy) {
  validate_edge_cloud_request(request);
  validate_edge_cloud_policy_input(policy);

  EdgeCloudDecision decision;
  decision.network_quality =
      assess_edge_cloud_network_quality(policy.network_quality);
  decision.budget = assess_edge_cloud_budget(policy.budget);
  decision.session_recovery = assess_edge_cloud_session_recovery(request);
  const bool remote_ready_without_budget =
      remote_eligible_without_budget(request, policy,
                                     decision.network_quality);
  const bool remote_ready_without_session =
      remote_ready_without_budget && decision.budget.cloud_eligible;
  decision.remote_eligible =
      remote_eligible(request, policy, decision.network_quality,
                      decision.budget, decision.session_recovery);
  decision.fallback_eligible = false;

  const bool local_ready_without_budget =
      policy.local_runtime_ready && policy.local_resource_fit;
  const bool local_ready_without_session =
      local_ready_without_budget && decision.budget.local_eligible;
  const bool local_ready = local_ready_without_session &&
                           decision.session_recovery.eligible;

  if (request.data_classification == EdgeDataClassification::kDeviceOnly) {
    if (local_ready) {
      decision.placement = EdgeCloudPlacement::kLocal;
      decision.reason = "device_only_local_required";
    } else {
      decision.placement = EdgeCloudPlacement::kReject;
      if (local_ready_without_session &&
          !decision.session_recovery.eligible) {
        decision.reason = "device_only_session_recovery_ineligible";
      } else {
        decision.reason = local_ready_without_budget
                              ? "device_only_local_budget_ineligible"
                              : "device_only_local_unavailable";
      }
    }
    decision.remote_eligible = false;
    decision.fallback_eligible = false;
    return decision;
  }

  if (local_ready && decision.remote_eligible) {
    decision.placement = policy.prefer_local
                             ? EdgeCloudPlacement::kLocalThenCloud
                             : EdgeCloudPlacement::kCloud;
    decision.fallback_eligible = policy.prefer_local;
    decision.reason = policy.prefer_local
                          ? "local_preferred_cloud_fallback_eligible"
                          : "cloud_preferred_by_policy";
  } else if (local_ready) {
    decision.placement = EdgeCloudPlacement::kLocal;
    decision.reason =
        remote_ready_without_budget && !decision.budget.cloud_eligible
            ? "cloud_budget_ineligible_local_ready"
            : "local_only_ready";
  } else if (decision.remote_eligible) {
    decision.placement = EdgeCloudPlacement::kCloud;
    decision.reason =
        local_ready_without_budget && !decision.budget.local_eligible
            ? "local_budget_ineligible_cloud_ready"
            : "local_unavailable_cloud_ready";
  } else {
    decision.placement = EdgeCloudPlacement::kReject;
    if (!decision.session_recovery.eligible &&
        (local_ready_without_session || remote_ready_without_session)) {
      decision.reason = "session_recovery_ineligible";
    } else if (local_ready_without_budget && !decision.budget.local_eligible &&
        remote_ready_without_budget && !decision.budget.cloud_eligible) {
      decision.reason = "placement_budget_ineligible";
    } else if (local_ready_without_budget &&
               !decision.budget.local_eligible) {
      decision.reason = "local_budget_ineligible_and_remote_unavailable";
    } else if (remote_ready_without_budget &&
               !decision.budget.cloud_eligible) {
      decision.reason = "cloud_budget_ineligible_and_local_unavailable";
    } else if (!policy.remote_processing_allowed) {
      decision.reason = "remote_processing_disallowed_and_local_unavailable";
    } else if (!policy.network_available) {
      decision.reason = "network_unavailable_and_local_unavailable";
    } else if (!decision.network_quality.eligible) {
      decision.reason = "network_quality_ineligible";
    } else if (!policy.cloud_transport_ready) {
      decision.reason = "cloud_transport_unavailable_and_local_unavailable";
    } else if (!cloud_deadline_available(policy)) {
      decision.reason = "cloud_deadline_budget_insufficient";
    } else {
      decision.reason = "no_eligible_placement";
    }
  }
  return decision;
}

void validate_cloud_generation_result(const CloudGenerationResult& result) {
  validate_edge_runtime_operation_result(result);
  if (result.metrics.provenance.empty()) {
    throw std::invalid_argument(
        "cloud generation metrics require nonempty provenance");
  }
  if (result.delivered_chunks > 0U &&
      (!result.metrics.first_chunk_latency_ms.has_value() ||
       !std::isfinite(*result.metrics.first_chunk_latency_ms) ||
       *result.metrics.first_chunk_latency_ms <= 0.0)) {
    throw std::invalid_argument(
        "cloud result with delivered chunks requires positive first-chunk latency");
  }
  if (result.metrics.request_duration_ms.has_value() &&
      (!std::isfinite(*result.metrics.request_duration_ms) ||
       *result.metrics.request_duration_ms <= 0.0)) {
    throw std::invalid_argument(
        "cloud request duration must be positive when present");
  }
  if (result.metrics.stream_chunk_interval_ms.has_value() &&
      (!std::isfinite(*result.metrics.stream_chunk_interval_ms) ||
       *result.metrics.stream_chunk_interval_ms <= 0.0 ||
       result.delivered_chunks < 2U)) {
    throw std::invalid_argument(
        "cloud chunk interval requires at least two delivered chunks");
  }
  if (result.status == EdgeRuntimeStatus::kReady &&
      result.finish_reason.empty()) {
    throw std::invalid_argument(
        "ready cloud result requires a bounded finish reason");
  }
  if (result.finish_reason.size() > kMaximumEdgeCloudIdentifierBytes ||
      result.error_category.size() > kMaximumEdgeCloudIdentifierBytes ||
      contains_control_byte(result.finish_reason) ||
      contains_control_byte(result.error_category)) {
    throw std::invalid_argument("cloud result metadata is oversized or unsafe");
  }
}

void validate_edge_cloud_generation_result(
    const EdgeCloudGenerationResult& result) {
  validate_edge_runtime_operation_result(result);
  switch (result.decision.placement) {
    case EdgeCloudPlacement::kLocal:
    case EdgeCloudPlacement::kCloud:
    case EdgeCloudPlacement::kLocalThenCloud:
    case EdgeCloudPlacement::kReject:
      break;
    default:
      throw std::invalid_argument("edge-cloud decision has unknown placement");
  }
  validate_identifier("edge-cloud decision reason", result.decision.reason);
  validate_network_quality_assessment(result.decision.network_quality);
  validate_edge_cloud_budget_assessment(result.decision.budget);
  validate_edge_cloud_session_recovery_assessment(
      result.decision.session_recovery);
  const bool placement_requires_remote =
      result.decision.placement == EdgeCloudPlacement::kCloud ||
      result.decision.placement == EdgeCloudPlacement::kLocalThenCloud;
  if (result.decision.fallback_eligible !=
          (result.decision.placement ==
           EdgeCloudPlacement::kLocalThenCloud) ||
      result.decision.remote_eligible != placement_requires_remote ||
      (result.decision.remote_eligible &&
       (!result.decision.network_quality.eligible ||
        !result.decision.budget.cloud_eligible ||
        !result.decision.session_recovery.eligible))) {
    throw std::invalid_argument(
        "edge-cloud decision eligibility does not match placement");
  }
  if (!result.decision.budget.local_eligible &&
      (result.decision.placement == EdgeCloudPlacement::kLocal ||
       result.decision.placement == EdgeCloudPlacement::kLocalThenCloud)) {
    throw std::invalid_argument(
        "edge-cloud local placement violates the budget assessment");
  }
  if (!result.decision.session_recovery.eligible &&
      result.decision.placement != EdgeCloudPlacement::kReject) {
    throw std::invalid_argument(
        "edge-cloud placement violates the session recovery assessment");
  }
  if (result.attempts.size() > 2U) {
    throw std::invalid_argument("edge-cloud result exceeds two attempts");
  }
  std::size_t delivered = 0U;
  for (std::size_t index = 0U; index < result.attempts.size(); ++index) {
    const auto& attempt = result.attempts[index];
    if (attempt.placement != EdgeCloudPlacement::kLocal &&
        attempt.placement != EdgeCloudPlacement::kCloud) {
      throw std::invalid_argument(
          "edge-cloud attempt placement must be local or cloud");
    }
    validate_identifier("edge-cloud attempt id", attempt.attempt_id);
    EdgeRuntimeOperationResult attempt_status;
    attempt_status.status = attempt.status;
    attempt_status.error_category = attempt.error_category;
    validate_edge_runtime_operation_result(attempt_status);
    if (attempt.duration_ms.has_value() &&
        (!std::isfinite(*attempt.duration_ms) ||
         *attempt.duration_ms <= 0.0)) {
      throw std::invalid_argument(
          "edge-cloud attempt duration must be positive when present");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (result.attempts[prior].attempt_id == attempt.attempt_id) {
        throw std::invalid_argument("edge-cloud attempt ids must be unique");
      }
    }
    if (attempt.delivered_chunks >
        std::numeric_limits<std::size_t>::max() - delivered) {
      throw std::overflow_error("edge-cloud delivered chunk count overflow");
    }
    delivered += attempt.delivered_chunks;
  }
  if (delivered != result.delivered_chunks) {
    throw std::invalid_argument(
        "edge-cloud attempt chunks do not reconcile with result total");
  }
  if ((result.final_placement == EdgeCloudPlacement::kLocalThenCloud) ||
      (result.decision.placement == EdgeCloudPlacement::kLocal &&
       !result.attempts.empty() &&
       result.attempts.front().placement != EdgeCloudPlacement::kLocal) ||
      (result.decision.placement == EdgeCloudPlacement::kCloud &&
       !result.attempts.empty() &&
       result.attempts.front().placement != EdgeCloudPlacement::kCloud) ||
      (result.decision.placement == EdgeCloudPlacement::kLocalThenCloud &&
       !result.attempts.empty() &&
       result.attempts.front().placement != EdgeCloudPlacement::kLocal)) {
    throw std::invalid_argument(
        "edge-cloud attempt order does not match the placement decision");
  }
  if (result.fallback_performed &&
      (!result.fallback_attempted || result.attempts.size() != 2U ||
       result.decision.placement != EdgeCloudPlacement::kLocalThenCloud ||
       result.attempts.front().placement != EdgeCloudPlacement::kLocal ||
       result.attempts.front().delivered_chunks != 0U ||
       result.attempts.back().placement != EdgeCloudPlacement::kCloud)) {
    throw std::invalid_argument(
        "edge-cloud fallback violates the zero-output local-to-cloud contract");
  }
  if (result.fallback_attempted &&
      (result.decision.placement != EdgeCloudPlacement::kLocalThenCloud ||
       result.attempts.empty() ||
       result.attempts.front().placement != EdgeCloudPlacement::kLocal)) {
    throw std::invalid_argument(
        "edge-cloud fallback attempt requires an initial local attempt");
  }
  if (result.fallback_network_quality_present !=
      result.fallback_attempted) {
    throw std::invalid_argument(
        "edge-cloud fallback network assessment presence is inconsistent");
  }
  if (result.fallback_network_quality_present) {
    validate_network_quality_assessment(
        result.fallback_network_quality);
  }
  if (result.fallback_budget_present != result.fallback_attempted) {
    throw std::invalid_argument(
        "edge-cloud fallback budget assessment presence is inconsistent");
  }
  if (result.fallback_budget_present) {
    validate_edge_cloud_budget_assessment(result.fallback_budget);
  }
  if (result.fallback_performed && !result.fallback_budget.cloud_eligible) {
    throw std::invalid_argument(
        "edge-cloud fallback violates the cloud budget assessment");
  }
  if (result.fallback_session_recovery_present !=
      result.fallback_attempted) {
    throw std::invalid_argument(
        "edge-cloud fallback session assessment presence is inconsistent");
  }
  if (result.fallback_session_recovery_present) {
    validate_edge_cloud_session_recovery_assessment(
        result.fallback_session_recovery);
  }
  if (result.fallback_performed &&
      !result.fallback_session_recovery.eligible) {
    throw std::invalid_argument(
        "edge-cloud fallback violates the session recovery assessment");
  }
  if (result.attempts.size() == 2U && !result.fallback_performed) {
    throw std::invalid_argument(
        "two edge-cloud attempts require an explicit performed fallback");
  }
  if (result.status == EdgeRuntimeStatus::kReady &&
      (result.final_placement == EdgeCloudPlacement::kReject ||
       result.attempts.empty() || result.finish_reason.empty())) {
    throw std::invalid_argument(
        "ready edge-cloud result requires a completed placement and attempt");
  }
  if (result.final_placement == EdgeCloudPlacement::kReject &&
      !result.attempts.empty()) {
    throw std::invalid_argument(
        "rejected edge-cloud result must not contain execution attempts");
  }
  if (!result.attempts.empty() &&
      result.final_placement != result.attempts.back().placement) {
    throw std::invalid_argument(
        "edge-cloud final placement must match the final attempt");
  }
  if (!result.attempts.empty() &&
      result.status != result.attempts.back().status) {
    throw std::invalid_argument(
        "edge-cloud result status must match the final attempt");
  }
}

EdgeCloudOrchestrator::EdgeCloudOrchestrator(
    IEdgeRuntime* local_runtime,
    ICloudInferenceTransport* cloud_transport)
    : local_runtime_(local_runtime), cloud_transport_(cloud_transport) {}

bool EdgeCloudOrchestrator::activate_attempt_unless_cancelled(
    const ActiveAttempt attempt) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!generation_active_ || cancellation_requested_) return false;
  active_attempt_ = attempt;
  return true;
}

EdgeCloudGenerationResult EdgeCloudOrchestrator::generate(
    const EdgeCloudRequest& request, const EdgeCloudPolicyInput& policy,
    const EdgeCloudChunkCallback& on_chunk) {
  validate_edge_cloud_request(request);
  validate_edge_cloud_policy_input(policy);
  if (!on_chunk) {
    throw std::invalid_argument("edge-cloud stream callback is required");
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (generation_active_) {
      throw std::logic_error(
          "edge-cloud orchestrator permits one active generation");
    }
    generation_active_ = true;
    cancellation_requested_ = false;
    active_attempt_ = ActiveAttempt::kNone;
  }

  const auto finish_generation = [this]() {
    std::lock_guard<std::mutex> lock(mutex_);
    active_attempt_ = ActiveAttempt::kNone;
    generation_active_ = false;
  };

  try {
    EdgeCloudGenerationResult result;
    result.decision = select_edge_cloud_placement(request, policy);
    result.final_placement = EdgeCloudPlacement::kReject;
    if (result.decision.placement == EdgeCloudPlacement::kReject) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "placement_rejected";
      result.detail = result.decision.reason;
      validate_edge_cloud_generation_result(result);
      finish_generation();
      return result;
    }

    std::size_t delivered_chunks = 0U;
    std::optional<SteadyClock::time_point> first_delivery_time;
    const auto is_cancelled = [this]() {
      std::lock_guard<std::mutex> lock(mutex_);
      return cancellation_requested_;
    };
    const auto forward_chunk = [&](const std::size_t,
                                   const std::string& text) {
      if (text.size() > kMaximumEdgeCloudStreamChunkBytes) {
        return false;
      }
      if (is_cancelled()) return false;
      const std::size_t next_index = delivered_chunks;
      if (delivered_chunks == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("edge-cloud delivered chunk count overflow");
      }
      if (!first_delivery_time.has_value()) {
        first_delivery_time = SteadyClock::now();
      }
      ++delivered_chunks;
      return on_chunk(next_index, text);
    };

    const auto run_cloud = [&]() {
      if (is_cancelled()) {
        CloudGenerationResult cancelled;
        cancelled.status = EdgeRuntimeStatus::kCancelled;
        cancelled.error_category = "cancelled_before_cloud_attempt";
        cancelled.detail = "edge-cloud request was cancelled before cloud I/O";
        cancelled.metrics.provenance = "orchestrator_pre_attempt_gate";
        return cancelled;
      }
      if (cloud_transport_ == nullptr) {
        CloudGenerationResult unavailable;
        unavailable.status = EdgeRuntimeStatus::kDependencyUnavailable;
        unavailable.error_category = "cloud_transport_missing";
        unavailable.detail = "cloud transport was not provided";
        unavailable.metrics.provenance = "orchestrator_capability_gate";
        return unavailable;
      }
      if (!activate_attempt_unless_cancelled(ActiveAttempt::kCloud)) {
        CloudGenerationResult cancelled;
        cancelled.status = EdgeRuntimeStatus::kCancelled;
        cancelled.error_category = "cancelled_before_cloud_attempt";
        cancelled.detail = "edge-cloud request was cancelled before cloud I/O";
        cancelled.metrics.provenance = "orchestrator_pre_attempt_gate";
        return cancelled;
      }
      const auto cloud_start = SteadyClock::now();
      const std::size_t chunks_before = delivered_chunks;
      try {
        auto cloud = cloud_transport_->generate(request, forward_chunk);
        if (cloud.delivered_chunks != delivered_chunks - chunks_before) {
          throw std::invalid_argument(
              "cloud transport result does not reconcile callback chunks");
        }
        validate_cloud_generation_result(cloud);
        if (cloud.reported_prompt_tokens.has_value() &&
            *cloud.reported_prompt_tokens >
                request.generation.context_tokens) {
          throw std::invalid_argument(
              "cloud prompt usage exceeds the request context limit");
        }
        if (cloud.reported_completion_tokens.has_value() &&
            *cloud.reported_completion_tokens >
                request.generation.maximum_output_tokens) {
          throw std::invalid_argument(
              "cloud completion usage exceeds the request output limit");
        }
        if (cloud.reported_prompt_tokens.has_value() &&
            cloud.reported_completion_tokens.has_value() &&
            *cloud.reported_completion_tokens >
                request.generation.context_tokens -
                    *cloud.reported_prompt_tokens) {
          throw std::invalid_argument(
              "cloud usage exceeds the request context limit");
        }
        return cloud;
      } catch (const std::exception&) {
        CloudGenerationResult failed;
        failed.status = EdgeRuntimeStatus::kFailed;
        failed.error_category = "cloud_transport_exception";
        failed.detail =
            "cloud transport raised or returned an invalid result";
        failed.delivered_chunks = delivered_chunks - chunks_before;
        failed.metrics.request_duration_ms =
            elapsed_milliseconds(cloud_start);
        if (failed.delivered_chunks > 0U &&
            first_delivery_time.has_value()) {
          failed.metrics.first_chunk_latency_ms = std::max(
              0.001,
              std::chrono::duration<double, std::milli>(
                  *first_delivery_time - cloud_start)
                  .count());
        }
        failed.metrics.provenance = "orchestrator_exception_projection";
        return failed;
      }
    };

    const auto request_start = SteadyClock::now();
    if (result.decision.placement == EdgeCloudPlacement::kCloud) {
      auto cloud = run_cloud();
      result.attempts.push_back(cloud_attempt(cloud, 1U));
      project_cloud_result(cloud, &result);
      result.delivered_chunks = delivered_chunks;
      validate_edge_cloud_generation_result(result);
      finish_generation();
      return result;
    }

    if (local_runtime_ == nullptr) {
      throw std::logic_error(
          "local placement selected without a local runtime");
    }
    const bool local_attempt_activated =
        activate_attempt_unless_cancelled(ActiveAttempt::kLocal);
    const auto local_start = SteadyClock::now();
    EdgeGenerationResult local;
    if (!local_attempt_activated) {
      local.status = EdgeRuntimeStatus::kCancelled;
      local.error_category = "cancelled_before_local_attempt";
      local.detail = "edge-cloud request was cancelled before local generate";
      local.finish_reason = "cancelled";
      local.metrics.provenance = "orchestrator_pre_attempt_gate";
    } else {
      const std::size_t chunks_before = delivered_chunks;
      try {
        local = local_runtime_->generate(request.generation, forward_chunk);
        if (local.generated_tokens != delivered_chunks - chunks_before) {
          throw std::invalid_argument(
              "local runtime result does not reconcile callback deliveries");
        }
        validate_edge_generation_result_for_request(local,
                                                    request.generation);
      } catch (const std::exception&) {
        local = {};
        local.status = EdgeRuntimeStatus::kFailed;
        local.error_category = "local_runtime_exception";
        local.detail =
            "local runtime raised or returned an invalid result";
        local.generated_tokens = delivered_chunks - chunks_before;
        local.metrics.provenance = "orchestrator_exception_projection";
      }
    }
    validate_edge_generation_result_for_request(local, request.generation);
    result.attempts.push_back(
        local_attempt(local, elapsed_milliseconds(local_start)));
    project_local_result(local, &result);
    result.delivered_chunks = delivered_chunks;

    if (result.decision.placement != EdgeCloudPlacement::kLocalThenCloud ||
        local.status == EdgeRuntimeStatus::kReady) {
      validate_edge_cloud_generation_result(result);
      finish_generation();
      return result;
    }

    result.fallback_attempted = true;
    EdgeCloudPolicyInput fallback_policy = policy;
    EdgeCloudRequest fallback_request = request;
    fallback_policy.remaining_deadline_ms =
        remaining_deadline(policy, request_start);
    const std::uint64_t elapsed =
        policy.remaining_deadline_ms -
        fallback_policy.remaining_deadline_ms;
    if (fallback_policy.network_quality.policy_enabled &&
        fallback_policy.network_quality.observation_available) {
      auto& age = fallback_policy.network_quality.observation_age_ms;
      age = advanced_observation_age(age, elapsed);
    }
    if (budget_observation_available(fallback_policy.budget)) {
      auto& age = fallback_policy.budget.observation_age_ms;
      age = advanced_observation_age(age, elapsed);
    }
    if (fallback_request.session_recovery.recovery_requested) {
      auto& age = fallback_request.session_recovery.state_age_ms;
      age = advanced_observation_age(age, elapsed);
    }
    result.fallback_network_quality =
        assess_edge_cloud_network_quality(
            fallback_policy.network_quality);
    result.fallback_network_quality_present = true;
    result.fallback_budget = assess_edge_cloud_budget(fallback_policy.budget);
    result.fallback_budget_present = true;
    result.fallback_session_recovery =
        assess_edge_cloud_session_recovery(fallback_request);
    result.fallback_session_recovery_present = true;
    const bool cancelled = is_cancelled();
    bool cloud_capability_ready_now = false;
    if (cloud_transport_ != nullptr) {
      try {
        const auto capabilities = cloud_transport_->capabilities();
        cloud_capability_ready_now = capabilities.dependency_linked &&
                                     capabilities.supports_streaming &&
                                     capabilities.supports_cancel;
      } catch (const std::exception&) {
        cloud_capability_ready_now = false;
      }
    }
    const bool fallback_allowed =
        !cancelled && retryable_local_failure(local) &&
        delivered_chunks == 0U && cloud_capability_ready_now &&
        remote_eligible(fallback_request, fallback_policy,
                        result.fallback_network_quality,
                        result.fallback_budget,
                        result.fallback_session_recovery);
    if (!fallback_allowed) {
      if (cancelled || local.status == EdgeRuntimeStatus::kCancelled) {
        result.error_category = "fallback_blocked_cancelled";
      } else if (delivered_chunks != 0U || local.generated_tokens != 0U) {
        result.error_category = "fallback_blocked_output_already_delivered";
      } else if (!retryable_local_failure(local)) {
        result.error_category = "fallback_blocked_non_retryable_local_failure";
      } else if (!result.fallback_network_quality.eligible) {
        result.error_category = "fallback_blocked_network_quality";
      } else if (!result.fallback_budget.cloud_eligible) {
        result.error_category = "fallback_blocked_cloud_budget";
      } else if (!result.fallback_session_recovery.eligible) {
        result.error_category = "fallback_blocked_session_recovery";
      } else {
        result.error_category = "fallback_blocked_policy_or_deadline";
      }
      validate_edge_cloud_generation_result(result);
      finish_generation();
      return result;
    }

    auto cloud = run_cloud();
    result.attempts.push_back(cloud_attempt(cloud, 2U));
    result.fallback_performed = true;
    project_cloud_result(cloud, &result);
    result.delivered_chunks = delivered_chunks;
    validate_edge_cloud_generation_result(result);
    finish_generation();
    return result;
  } catch (...) {
    finish_generation();
    throw;
  }
}

void EdgeCloudOrchestrator::cancel() noexcept {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!generation_active_) return;
  cancellation_requested_ = true;
  if (active_attempt_ == ActiveAttempt::kLocal && local_runtime_ != nullptr) {
    local_runtime_->cancel();
  } else if (active_attempt_ == ActiveAttempt::kCloud &&
             cloud_transport_ != nullptr) {
    cloud_transport_->cancel();
  }
}

}  // namespace inferlab
