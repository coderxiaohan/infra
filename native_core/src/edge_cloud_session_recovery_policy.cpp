#include "inferlab/edge_cloud.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab {
namespace {

constexpr std::size_t kMaximumRecoveredTranscriptTurns = 1000000U;
constexpr std::size_t kMaximumPriorDeliveredChunks = 1000000U;

bool is_not_required_identity(const std::string& value) {
  return value == "not_required";
}

void require_unique_known_blockers(
    const std::vector<std::string>& blockers) {
  const std::vector<std::string> vocabulary = {
      "durable_transcript_missing",
      "prompt_not_reconstructed_from_transcript",
      "transcript_turns_missing",
      "recovery_state_stale",
      "session_identity_mismatch",
      "previous_request_identity_reused",
      "partial_output_resume_unsupported",
  };
  for (std::size_t index = 0U; index < blockers.size(); ++index) {
    if (std::find(vocabulary.begin(), vocabulary.end(), blockers[index]) ==
        vocabulary.end()) {
      throw std::invalid_argument(
          "edge-cloud session recovery has an unknown blocker");
    }
    if (std::find(blockers.begin(), blockers.begin() + index,
                  blockers[index]) != blockers.begin() + index) {
      throw std::invalid_argument(
          "edge-cloud session recovery blockers must be unique");
    }
  }
}

}  // namespace

void validate_edge_cloud_session_recovery_input(
    const EdgeCloudRequest& request) {
  const auto& input = request.session_recovery;
  if (input.maximum_state_age_ms == 0U ||
      input.maximum_state_age_ms > kMaximumEdgeCloudObservationAgeMs ||
      input.state_age_ms > kMaximumEdgeCloudObservationAgeMs ||
      input.prior_delivered_chunks > kMaximumPriorDeliveredChunks ||
      input.transcript_turn_count > kMaximumRecoveredTranscriptTurns) {
    throw std::invalid_argument(
        "edge-cloud session recovery input is outside bounds");
  }
  if (!input.recovery_requested) {
    if (input.durable_transcript_available ||
        input.prompt_reconstructed_from_transcript ||
        input.previous_response_complete ||
        input.prior_delivered_chunks != 0U ||
        input.transcript_turn_count != 0U || input.state_age_ms != 0U ||
        !is_not_required_identity(input.recovered_session_id) ||
        !is_not_required_identity(input.previous_request_id) ||
        !is_not_required_identity(input.transcript_revision) ||
        !is_not_required_identity(input.provenance)) {
      throw std::invalid_argument(
          "disabled edge-cloud session recovery cannot carry evidence");
    }
    return;
  }

  validate_edge_cloud_provenance_label(
      "edge-cloud recovered session id", input.recovered_session_id);
  validate_edge_cloud_provenance_label(
      "edge-cloud previous request id", input.previous_request_id);
  validate_edge_cloud_provenance_label(
      "edge-cloud transcript revision", input.transcript_revision);
  validate_edge_cloud_provenance_label(
      "edge-cloud session recovery provenance", input.provenance);
  if (is_not_required_identity(input.recovered_session_id) ||
      is_not_required_identity(input.previous_request_id) ||
      is_not_required_identity(input.transcript_revision) ||
      is_not_required_identity(input.provenance)) {
    throw std::invalid_argument(
        "enabled edge-cloud session recovery requires concrete identities "
        "and provenance");
  }
}

void validate_edge_cloud_session_recovery_assessment(
    const EdgeCloudSessionRecoveryAssessment& assessment) {
  require_unique_known_blockers(assessment.blockers);
  if (assessment.state_age_ms.has_value() &&
      *assessment.state_age_ms > kMaximumEdgeCloudObservationAgeMs) {
    throw std::invalid_argument(
        "edge-cloud session recovery assessment age is outside bounds");
  }
  if (assessment.status == "not_required") {
    if (!assessment.eligible || assessment.strategy != "not_required" ||
        assessment.state_age_ms.has_value() || !assessment.blockers.empty()) {
      throw std::invalid_argument(
          "not-required edge-cloud session recovery is inconsistent");
    }
    return;
  }
  if (assessment.status == "recovery_ready") {
    if (!assessment.eligible || !assessment.state_age_ms.has_value() ||
        !assessment.blockers.empty() ||
        (assessment.strategy != "continue_after_completed_turn" &&
         assessment.strategy !=
             "restart_incomplete_zero_output_turn")) {
      throw std::invalid_argument(
          "ready edge-cloud session recovery is inconsistent");
    }
    return;
  }
  if (assessment.status != "recovery_blocked" || assessment.eligible ||
      assessment.strategy != "none" ||
      !assessment.state_age_ms.has_value() || assessment.blockers.empty()) {
    throw std::invalid_argument(
        "blocked edge-cloud session recovery is inconsistent");
  }
}

EdgeCloudSessionRecoveryAssessment assess_edge_cloud_session_recovery(
    const EdgeCloudRequest& request) {
  validate_edge_cloud_session_recovery_input(request);
  const auto& input = request.session_recovery;
  EdgeCloudSessionRecoveryAssessment assessment;
  if (!input.recovery_requested) {
    validate_edge_cloud_session_recovery_assessment(assessment);
    return assessment;
  }

  assessment.eligible = false;
  assessment.status = "recovery_blocked";
  assessment.strategy = "none";
  assessment.state_age_ms = input.state_age_ms;
  if (!input.durable_transcript_available) {
    assessment.blockers.push_back("durable_transcript_missing");
  }
  if (!input.prompt_reconstructed_from_transcript) {
    assessment.blockers.push_back(
        "prompt_not_reconstructed_from_transcript");
  }
  if (input.transcript_turn_count == 0U) {
    assessment.blockers.push_back("transcript_turns_missing");
  }
  if (input.state_age_ms > input.maximum_state_age_ms) {
    assessment.blockers.push_back("recovery_state_stale");
  }
  if (input.recovered_session_id != request.session_id) {
    assessment.blockers.push_back("session_identity_mismatch");
  }
  if (input.previous_request_id == request.request_id) {
    assessment.blockers.push_back("previous_request_identity_reused");
  }
  if (!input.previous_response_complete &&
      input.prior_delivered_chunks != 0U) {
    assessment.blockers.push_back("partial_output_resume_unsupported");
  }
  if (assessment.blockers.empty()) {
    assessment.eligible = true;
    assessment.status = "recovery_ready";
    assessment.strategy = input.previous_response_complete
                              ? "continue_after_completed_turn"
                              : "restart_incomplete_zero_output_turn";
  }
  validate_edge_cloud_session_recovery_assessment(assessment);
  return assessment;
}

}  // namespace inferlab
