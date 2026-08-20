#pragma once

#include "inferlab/edge_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace inferlab {

inline constexpr std::size_t kMaximumEdgeCloudIdentifierBytes = 256U;
inline constexpr std::size_t kMaximumEdgeCloudEndpointBytes = 2048U;
inline constexpr std::size_t kMaximumEdgeCloudStreamChunkBytes =
    kMaximumEdgeTokenTextBytes;
inline constexpr std::uint64_t kDefaultMinimumCloudDeadlineMs = 5000U;
inline constexpr std::uint64_t kMaximumEdgeCloudObservationAgeMs =
    30U * 24U * 60U * 60U * 1000U;

enum class EdgeCloudPlacement {
  kLocal,
  kCloud,
  kLocalThenCloud,
  kReject,
};

enum class EdgeDataClassification {
  kPublic,
  kInternal,
  kSensitive,
  kDeviceOnly,
};

const char* edge_cloud_placement_name(EdgeCloudPlacement placement) noexcept;
const char* edge_data_classification_name(
    EdgeDataClassification classification) noexcept;
EdgeCloudPlacement parse_edge_cloud_placement(const std::string& value);
EdgeDataClassification parse_edge_data_classification(
    const std::string& value);

struct EdgeCloudSessionRecoveryInput {
  bool recovery_requested = false;
  bool durable_transcript_available = false;
  bool prompt_reconstructed_from_transcript = false;
  bool previous_response_complete = false;
  std::size_t prior_delivered_chunks = 0U;
  std::size_t transcript_turn_count = 0U;
  std::uint64_t state_age_ms = 0U;
  std::uint64_t maximum_state_age_ms = 300000U;
  std::string recovered_session_id = "not_required";
  std::string previous_request_id = "not_required";
  std::string transcript_revision = "not_required";
  std::string provenance = "not_required";
};

struct EdgeCloudSessionRecoveryAssessment {
  bool eligible = true;
  std::string status = "not_required";
  std::string strategy = "not_required";
  std::optional<std::uint64_t> state_age_ms;
  std::vector<std::string> blockers;
};

struct EdgeCloudRequest {
  std::string request_id;
  std::string session_id;
  std::string cloud_model_id;
  EdgeDataClassification data_classification =
      EdgeDataClassification::kInternal;
  EdgeGenerationRequest generation;
  EdgeCloudSessionRecoveryInput session_recovery;
};

struct EdgeCloudNetworkQualityInput {
  bool policy_enabled = false;
  bool observation_available = false;
  double round_trip_time_ms = 0.0;
  double jitter_ms = 0.0;
  double packet_loss_ratio = 0.0;
  double downlink_mbps = 0.0;
  std::uint64_t observation_age_ms = 0U;
  double maximum_round_trip_time_ms = 250.0;
  double maximum_jitter_ms = 100.0;
  double maximum_packet_loss_ratio = 0.05;
  double minimum_downlink_mbps = 0.5;
  std::uint64_t maximum_observation_age_ms = 30000U;
  std::string provenance = "not_required";
};

struct EdgeCloudNetworkQualityAssessment {
  bool eligible = true;
  std::string status = "not_required";
  std::optional<std::uint64_t> observation_age_ms;
  std::vector<std::string> blockers;
};

struct EdgeCloudBudgetPolicyInput {
  bool battery_gate_enabled = false;
  bool battery_observation_available = false;
  double battery_percent = 0.0;
  double minimum_local_battery_percent = 15.0;
  bool local_energy_gate_enabled = false;
  bool local_energy_estimate_available = false;
  std::uint64_t estimated_local_energy_uj = 0U;
  std::uint64_t maximum_local_energy_uj = 10000000U;
  bool cloud_cost_gate_enabled = false;
  bool cloud_cost_estimate_available = false;
  double estimated_cloud_request_cost_usd = 0.0;
  double maximum_cloud_request_cost_usd = 0.10;
  std::uint64_t observation_age_ms = 0U;
  std::uint64_t maximum_observation_age_ms = 30000U;
  std::string provenance = "not_required";
};

struct EdgeCloudBudgetAssessment {
  bool local_eligible = true;
  bool cloud_eligible = true;
  std::string status = "not_required";
  std::optional<std::uint64_t> observation_age_ms;
  std::vector<std::string> local_blockers;
  std::vector<std::string> cloud_blockers;
};

struct EdgeCloudPolicyInput {
  bool local_runtime_ready = false;
  bool local_resource_fit = false;
  bool cloud_transport_ready = false;
  bool network_available = false;
  bool remote_processing_allowed = false;
  bool prefer_local = true;
  std::uint64_t remaining_deadline_ms = 0U;
  std::uint64_t minimum_cloud_deadline_ms =
      kDefaultMinimumCloudDeadlineMs;
  EdgeCloudNetworkQualityInput network_quality;
  EdgeCloudBudgetPolicyInput budget;
};

struct EdgeCloudDecision {
  EdgeCloudPlacement placement = EdgeCloudPlacement::kReject;
  std::string reason;
  bool remote_eligible = false;
  bool fallback_eligible = false;
  EdgeCloudNetworkQualityAssessment network_quality;
  EdgeCloudBudgetAssessment budget;
  EdgeCloudSessionRecoveryAssessment session_recovery;
};

struct CloudTransportCapabilities {
  std::string transport_id;
  std::string transport_version;
  std::string product_id;
  std::string protocol;
  bool dependency_linked = false;
  bool structured_json_parser_linked = false;
  bool supports_streaming = false;
  bool supports_cancel = false;
  bool tls_peer_verification_required = true;
  bool redirects_disabled = true;
};

struct CloudGenerationMetrics {
  std::optional<double> request_duration_ms;
  std::optional<double> first_chunk_latency_ms;
  std::optional<double> stream_chunk_interval_ms;
  std::string provenance;
};

struct CloudGenerationResult : EdgeRuntimeOperationResult {
  std::size_t delivered_chunks = 0U;
  std::optional<std::size_t> reported_prompt_tokens;
  std::optional<std::size_t> reported_completion_tokens;
  std::string finish_reason;
  CloudGenerationMetrics metrics;
};

// Cloud protocol deltas are text chunks, not guaranteed tokenizer tokens.
using EdgeCloudChunkCallback =
    std::function<bool(std::size_t chunk_index,
                       const std::string& chunk_text)>;

class ICloudInferenceTransport {
 public:
  virtual ~ICloudInferenceTransport() = default;

  virtual CloudTransportCapabilities capabilities() const = 0;
  virtual CloudGenerationResult generate(
      const EdgeCloudRequest& request,
      const EdgeCloudChunkCallback& on_chunk) = 0;
  // Must only publish cooperative cancellation. Implementations must not
  // block waiting for generate() or invoke the stream callback here.
  virtual void cancel() noexcept = 0;
};

struct EdgeCloudAttempt {
  std::string attempt_id;
  EdgeCloudPlacement placement = EdgeCloudPlacement::kReject;
  EdgeRuntimeStatus status = EdgeRuntimeStatus::kFailed;
  std::string error_category;
  std::string finish_reason;
  std::size_t delivered_chunks = 0U;
  std::optional<std::size_t> reported_prompt_tokens;
  std::optional<std::size_t> reported_completion_tokens;
  std::optional<double> duration_ms;
};

struct EdgeCloudGenerationResult : EdgeRuntimeOperationResult {
  EdgeCloudDecision decision;
  EdgeCloudPlacement final_placement = EdgeCloudPlacement::kReject;
  std::vector<EdgeCloudAttempt> attempts;
  std::size_t delivered_chunks = 0U;
  std::optional<std::size_t> reported_prompt_tokens;
  std::optional<std::size_t> reported_completion_tokens;
  std::string finish_reason;
  bool fallback_attempted = false;
  bool fallback_performed = false;
  bool fallback_network_quality_present = false;
  EdgeCloudNetworkQualityAssessment fallback_network_quality;
  bool fallback_budget_present = false;
  EdgeCloudBudgetAssessment fallback_budget;
  bool fallback_session_recovery_present = false;
  EdgeCloudSessionRecoveryAssessment fallback_session_recovery;
};

EdgeCloudDecision select_edge_cloud_placement(
    const EdgeCloudRequest& request,
    const EdgeCloudPolicyInput& policy);
EdgeCloudNetworkQualityAssessment assess_edge_cloud_network_quality(
    const EdgeCloudNetworkQualityInput& input);
EdgeCloudBudgetAssessment assess_edge_cloud_budget(
    const EdgeCloudBudgetPolicyInput& input);
EdgeCloudSessionRecoveryAssessment assess_edge_cloud_session_recovery(
    const EdgeCloudRequest& request);
void validate_edge_cloud_request(const EdgeCloudRequest& request);
void validate_edge_cloud_provenance_label(const std::string& field,
                                          const std::string& value);
void validate_edge_cloud_network_quality_input(
    const EdgeCloudNetworkQualityInput& input);
void validate_edge_cloud_budget_policy_input(
    const EdgeCloudBudgetPolicyInput& input);
void validate_edge_cloud_budget_assessment(
    const EdgeCloudBudgetAssessment& assessment);
void validate_edge_cloud_session_recovery_input(
    const EdgeCloudRequest& request);
void validate_edge_cloud_session_recovery_assessment(
    const EdgeCloudSessionRecoveryAssessment& assessment);
void validate_edge_cloud_policy_input(const EdgeCloudPolicyInput& policy);
void validate_cloud_generation_result(const CloudGenerationResult& result);
void validate_edge_cloud_generation_result(
    const EdgeCloudGenerationResult& result);

class EdgeCloudOrchestrator {
 public:
  // Runtime and transport are caller-owned and must outlive active generate
  // and cancel calls.
  EdgeCloudOrchestrator(IEdgeRuntime* local_runtime,
                        ICloudInferenceTransport* cloud_transport);

  EdgeCloudGenerationResult generate(
      const EdgeCloudRequest& request,
      const EdgeCloudPolicyInput& policy,
      const EdgeCloudChunkCallback& on_chunk);
  void cancel() noexcept;

 private:
  enum class ActiveAttempt {
    kNone,
    kLocal,
    kCloud,
  };

  bool activate_attempt_unless_cancelled(ActiveAttempt attempt);

  IEdgeRuntime* local_runtime_ = nullptr;
  ICloudInferenceTransport* cloud_transport_ = nullptr;
  std::mutex mutex_;
  ActiveAttempt active_attempt_ = ActiveAttempt::kNone;
  bool generation_active_ = false;
  bool cancellation_requested_ = false;
};

}  // namespace inferlab
