#include "edge_cloud_routing_model.hpp"

#include "artifact_text_source.hpp"
#include "edge_cloud_transport.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace inferlab::edge_cloud_routing {
namespace {

constexpr std::size_t kMaximumConfigBytes = 2U * 1024U * 1024U;

class ProtectedOutputAlias final : public std::runtime_error {
 public:
  explicit ProtectedOutputAlias(const std::string& message)
      : std::runtime_error(message) {}
};

struct ParsedConfig {
  std::string target_platform;
  EdgeRuntimeArchitecture target_architecture =
      EdgeRuntimeArchitecture::kOther;
  std::string local_runtime_id;
  bool local_resource_fit = false;
  EdgeModelDescriptor local_model;
  OpenAiCompatibleTransportConfig cloud_transport;
  EdgeCloudRequest request;
  EdgeCloudPolicyInput policy;
};

void append_unique(std::vector<std::string>* values, std::string value) {
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(std::move(value));
  }
}

void checked_add_count(const std::size_t value, std::size_t* total,
                       const char* field) {
  if (value > std::numeric_limits<std::size_t>::max() - *total) {
    throw std::overflow_error(std::string("edge-cloud ") + field +
                              " count overflow");
  }
  *total += value;
}

bool path_exists(const std::filesystem::path& path) {
  std::error_code error;
  return !path.empty() && std::filesystem::is_regular_file(path, error) &&
         !error;
}

bool environment_value_present(const std::string& name) {
  const char* value = std::getenv(name.c_str());
  return value != nullptr && *value != '\0';
}

void reject_output_alias(const std::filesystem::path& input,
                         const std::filesystem::path& output,
                         const std::string& role) {
  if (!output.empty() && inferlab_plan::paths_alias(input, output)) {
    throw ProtectedOutputAlias(
        "edge-cloud output aliases protected " + role);
  }
}

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON

const nlohmann::json& required_object(const nlohmann::json& value,
                                      const char* field) {
  if (!value.contains(field) || !value.at(field).is_object()) {
    throw std::invalid_argument(std::string("edge-cloud config requires object: ") +
                                field);
  }
  return value.at(field);
}

std::string required_string(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_string()) {
    throw std::invalid_argument(std::string("edge-cloud config requires string: ") +
                                field);
  }
  const std::string parsed = value.at(field).get<std::string>();
  if (parsed.empty()) {
    throw std::invalid_argument(std::string("edge-cloud config string is empty: ") +
                                field);
  }
  return parsed;
}

bool required_bool(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_boolean()) {
    throw std::invalid_argument(std::string("edge-cloud config requires bool: ") +
                                field);
  }
  return value.at(field).get<bool>();
}

std::uint64_t required_u64(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_number_unsigned()) {
    throw std::invalid_argument(
        std::string("edge-cloud config requires unsigned integer: ") + field);
  }
  return value.at(field).get<std::uint64_t>();
}

std::uint64_t optional_u64(const nlohmann::json& value, const char* field,
                           const std::uint64_t fallback) {
  return value.contains(field) ? required_u64(value, field) : fallback;
}

std::size_t checked_size(const nlohmann::json& value, const char* field) {
  const std::uint64_t parsed = required_u64(value, field);
  if (parsed > std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error(std::string("edge-cloud config size overflow: ") +
                              field);
  }
  return static_cast<std::size_t>(parsed);
}

std::size_t optional_size(const nlohmann::json& value, const char* field,
                          const std::size_t fallback) {
  return value.contains(field) ? checked_size(value, field) : fallback;
}

float required_float(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_number()) {
    throw std::invalid_argument(std::string("edge-cloud config requires number: ") +
                                field);
  }
  const double parsed = value.at(field).get<double>();
  const float narrowed = static_cast<float>(parsed);
  if (!std::isfinite(parsed) || !std::isfinite(narrowed) ||
      (parsed != 0.0 && narrowed == 0.0F)) {
    throw std::invalid_argument(std::string("edge-cloud config float invalid: ") +
                                field);
  }
  return narrowed;
}

double required_double(const nlohmann::json& value, const char* field) {
  if (!value.contains(field) || !value.at(field).is_number()) {
    throw std::invalid_argument(
        std::string("edge-cloud config requires number: ") + field);
  }
  const double parsed = value.at(field).get<double>();
  if (!std::isfinite(parsed)) {
    throw std::invalid_argument(
        std::string("edge-cloud config double invalid: ") + field);
  }
  return parsed;
}

ParsedConfig parse_config(const std::filesystem::path& repo_root,
                          const std::string& text,
                          const std::filesystem::path& output_path,
                          ConfigEvidence* evidence) {
  const auto document = nlohmann::json::parse(text);
  if (!document.is_object() || !document.contains("schema_version") ||
      !document.at("schema_version").is_number_unsigned()) {
    throw std::invalid_argument(
        "edge-cloud config requires unsigned schema_version");
  }
  evidence->schema_version =
      document.at("schema_version").get<std::uint64_t>();
  if (evidence->schema_version != 1U) {
    throw std::invalid_argument("edge-cloud config schema_version must be 1");
  }

  ParsedConfig parsed;
  parsed.target_platform = required_string(document, "target_platform");
  const auto* target_platform =
      find_edge_runtime_target_platform(parsed.target_platform);
  if (target_platform == nullptr) {
    throw std::invalid_argument(
        "edge-cloud target_platform is not a canonical supported value");
  }
  parsed.target_architecture = target_platform->runtime_architecture;
  const auto& local = required_object(document, "local");
  const auto& local_model = required_object(local, "model");
  const auto& cloud = required_object(document, "cloud");
  const auto& policy = required_object(document, "policy");
  const auto& generation = required_object(document, "generation");

  parsed.local_runtime_id = required_string(local, "runtime");
  parsed.local_resource_fit = required_bool(local, "resource_fit");
  parsed.local_model.logical_model_id =
      required_string(local_model, "logical_model_id");
  parsed.local_model.model_format =
      required_string(local_model, "model_format");
  parsed.local_model.quantization =
      required_string(local_model, "quantization");
  parsed.local_model.tokenizer_id =
      required_string(local_model, "tokenizer_id");
  parsed.local_model.checkpoint_revision =
      required_string(local_model, "checkpoint_revision");
  parsed.local_model.conversion_provenance =
      required_string(local_model, "conversion_provenance");
  parsed.local_model.model_path = inferlab_plan::resolve_repo_relative_path(
      repo_root, required_string(local_model, "model_path"));
  parsed.local_model.tokenizer_path =
      inferlab_plan::resolve_optional_repo_relative_path(
          repo_root,
          local_model.contains("tokenizer_path")
              ? required_string(local_model, "tokenizer_path")
              : std::string{});
  parsed.local_model.model_path_label = inferlab_plan::redacted_path_label(
      parsed.local_model.model_path.generic_string());
  parsed.local_model.context_limit = checked_size(local_model, "context_limit");
  parsed.local_model.prefer_mmap = required_bool(local_model, "prefer_mmap");
  reject_output_alias(parsed.local_model.model_path, output_path,
                      "local model input");
  reject_output_alias(parsed.local_model.tokenizer_path, output_path,
                      "local tokenizer input");

  parsed.cloud_transport.product_id = required_string(cloud, "product");
  parsed.cloud_transport.endpoint_url = required_string(cloud, "endpoint_url");
  parsed.cloud_transport.api_key_environment_variable =
      required_string(cloud, "api_key_env");
  parsed.cloud_transport.allow_loopback_http =
      required_bool(cloud, "allow_loopback_http");
  parsed.cloud_transport.connect_timeout_ms = optional_u64(
      cloud, "connect_timeout_ms", parsed.cloud_transport.connect_timeout_ms);
  parsed.cloud_transport.maximum_event_bytes = optional_size(
      cloud, "maximum_event_bytes",
      parsed.cloud_transport.maximum_event_bytes);
  parsed.cloud_transport.maximum_response_bytes = optional_size(
      cloud, "maximum_response_bytes",
      parsed.cloud_transport.maximum_response_bytes);

  parsed.request.request_id = required_string(generation, "request_id");
  parsed.request.session_id = required_string(generation, "session_id");
  parsed.request.cloud_model_id =
      required_string(generation, "cloud_model_id");
  parsed.request.data_classification = parse_edge_data_classification(
      required_string(policy, "data_classification"));
  parsed.request.generation.prompt = required_string(generation, "prompt");
  parsed.request.generation.maximum_output_tokens =
      checked_size(generation, "maximum_output_tokens");
  parsed.request.generation.context_tokens =
      checked_size(generation, "context_tokens");
  parsed.request.generation.batch_size = checked_size(generation, "batch_size");
  parsed.request.generation.thread_count =
      checked_size(generation, "thread_count");
  parsed.request.generation.maximum_generation_duration_ms = required_u64(
      generation, "maximum_generation_duration_ms");
  parsed.request.generation.seed = required_u64(generation, "seed");
  parsed.request.generation.temperature =
      required_float(generation, "temperature");
  parsed.request.generation.top_p = required_float(generation, "top_p");

  parsed.policy.local_resource_fit = parsed.local_resource_fit;
  parsed.policy.network_available =
      required_bool(cloud, "network_available");
  parsed.policy.remote_processing_allowed =
      required_bool(policy, "remote_processing_allowed");
  parsed.policy.prefer_local = required_bool(policy, "prefer_local");
  parsed.policy.remaining_deadline_ms =
      parsed.request.generation.maximum_generation_duration_ms;
  parsed.policy.minimum_cloud_deadline_ms = optional_u64(
      policy, "minimum_cloud_deadline_ms",
      kDefaultMinimumCloudDeadlineMs);
  if (cloud.contains("network_quality")) {
    const auto& network_quality =
        required_object(cloud, "network_quality");
    auto& input = parsed.policy.network_quality;
    input.policy_enabled =
        required_bool(network_quality, "policy_enabled");
    input.observation_available =
        required_bool(network_quality, "observation_available");
    input.round_trip_time_ms =
        required_double(network_quality, "round_trip_time_ms");
    input.jitter_ms = required_double(network_quality, "jitter_ms");
    input.packet_loss_ratio =
        required_double(network_quality, "packet_loss_ratio");
    input.downlink_mbps =
        required_double(network_quality, "downlink_mbps");
    input.observation_age_ms =
        required_u64(network_quality, "observation_age_ms");
    input.maximum_round_trip_time_ms = required_double(
        network_quality, "maximum_round_trip_time_ms");
    input.maximum_jitter_ms =
        required_double(network_quality, "maximum_jitter_ms");
    input.maximum_packet_loss_ratio = required_double(
        network_quality, "maximum_packet_loss_ratio");
    input.minimum_downlink_mbps =
        required_double(network_quality, "minimum_downlink_mbps");
    input.maximum_observation_age_ms =
        required_u64(network_quality, "maximum_observation_age_ms");
    input.provenance = required_string(network_quality, "provenance");
  }
  if (policy.contains("budget")) {
    const auto& budget = required_object(policy, "budget");
    auto& input = parsed.policy.budget;
    input.battery_gate_enabled =
        required_bool(budget, "battery_gate_enabled");
    input.battery_observation_available =
        required_bool(budget, "battery_observation_available");
    input.battery_percent = required_double(budget, "battery_percent");
    input.minimum_local_battery_percent = required_double(
        budget, "minimum_local_battery_percent");
    input.local_energy_gate_enabled =
        required_bool(budget, "local_energy_gate_enabled");
    input.local_energy_estimate_available =
        required_bool(budget, "local_energy_estimate_available");
    input.estimated_local_energy_uj =
        required_u64(budget, "estimated_local_energy_uj");
    input.maximum_local_energy_uj =
        required_u64(budget, "maximum_local_energy_uj");
    input.cloud_cost_gate_enabled =
        required_bool(budget, "cloud_cost_gate_enabled");
    input.cloud_cost_estimate_available =
        required_bool(budget, "cloud_cost_estimate_available");
    input.estimated_cloud_request_cost_usd = required_double(
        budget, "estimated_cloud_request_cost_usd");
    input.maximum_cloud_request_cost_usd = required_double(
        budget, "maximum_cloud_request_cost_usd");
    input.observation_age_ms =
        required_u64(budget, "observation_age_ms");
    input.maximum_observation_age_ms =
        required_u64(budget, "maximum_observation_age_ms");
    input.provenance = required_string(budget, "provenance");
  }
  if (policy.contains("session_recovery")) {
    const auto& recovery = required_object(policy, "session_recovery");
    auto& input = parsed.request.session_recovery;
    input.recovery_requested =
        required_bool(recovery, "recovery_requested");
    input.durable_transcript_available =
        required_bool(recovery, "durable_transcript_available");
    input.prompt_reconstructed_from_transcript = required_bool(
        recovery, "prompt_reconstructed_from_transcript");
    input.previous_response_complete =
        required_bool(recovery, "previous_response_complete");
    input.prior_delivered_chunks =
        checked_size(recovery, "prior_delivered_chunks");
    input.transcript_turn_count =
        checked_size(recovery, "transcript_turn_count");
    input.state_age_ms = required_u64(recovery, "state_age_ms");
    input.maximum_state_age_ms =
        required_u64(recovery, "maximum_state_age_ms");
    input.recovered_session_id =
        required_string(recovery, "recovered_session_id");
    input.previous_request_id =
        required_string(recovery, "previous_request_id");
    input.transcript_revision =
        required_string(recovery, "transcript_revision");
    input.provenance = required_string(recovery, "provenance");
  }

  validate_edge_model_descriptor(parsed.local_model);
  validate_openai_compatible_transport_config(parsed.cloud_transport);
  validate_edge_cloud_request(parsed.request);
  validate_edge_cloud_policy_input(parsed.policy);
  if (parsed.request.generation.context_tokens >
      parsed.local_model.context_limit) {
    throw std::invalid_argument(
        "edge-cloud generation context exceeds the local model limit");
  }

  evidence->target_platform = parsed.target_platform;
  const auto* current_platform = current_edge_runtime_target_platform();
  evidence->target_platform_match_status =
      current_platform != nullptr &&
              current_platform->id == parsed.target_platform
          ? "matches_compiled_platform"
          : "mismatch_compiled_platform";
  evidence->request_fingerprint =
      inferlab_plan::fnv1a64_fingerprint(parsed.request.request_id);
  evidence->session_recovery_requested =
      parsed.request.session_recovery.recovery_requested;
  evidence->prompt_fingerprint_redacted_by_policy =
      parsed.request.data_classification ==
          EdgeDataClassification::kSensitive ||
      parsed.request.data_classification ==
          EdgeDataClassification::kDeviceOnly ||
      parsed.request.session_recovery.recovery_requested;
  evidence->prompt_fingerprint =
      evidence->prompt_fingerprint_redacted_by_policy
          ? std::string{}
          : inferlab_plan::fnv1a64_fingerprint(
                parsed.request.generation.prompt);
  evidence->prompt_bytes = parsed.request.generation.prompt.size();
  evidence->local_model_path_label = parsed.local_model.model_path_label;
  evidence->endpoint_security =
      parsed.cloud_transport.endpoint_url.rfind("https://", 0U) == 0U
          ? "https_tls_peer_verification_required"
          : "explicit_loopback_http_development_only";
  evidence->api_key_environment_variable =
      parsed.cloud_transport.api_key_environment_variable;
  evidence->product_id = parsed.cloud_transport.product_id;
  evidence->cloud_model_id = parsed.request.cloud_model_id;
  evidence->data_classification =
      edge_data_classification_name(parsed.request.data_classification);
  return parsed;
}
#endif

void summarize(Artifact* artifact) {
  artifact->summary.local_decision_count =
      artifact->placement.placement == EdgeCloudPlacement::kLocal ? 1U : 0U;
  artifact->summary.cloud_decision_count =
      artifact->placement.placement == EdgeCloudPlacement::kCloud ? 1U : 0U;
  artifact->summary.local_then_cloud_decision_count =
      artifact->placement.placement == EdgeCloudPlacement::kLocalThenCloud
          ? 1U
          : 0U;
  artifact->summary.reject_decision_count =
      artifact->placement.placement == EdgeCloudPlacement::kReject ? 1U : 0U;
  artifact->summary.attempt_count = artifact->execution_result_present
                                        ? artifact->execution.attempts.size()
                                        : 0U;
  artifact->summary.local_attempt_count = 0U;
  artifact->summary.cloud_attempt_count = 0U;
  artifact->summary.successful_attempt_count = 0U;
  artifact->summary.failed_attempt_count = 0U;
  artifact->summary.cancelled_attempt_count = 0U;
  if (artifact->execution_result_present) {
    for (const auto& attempt : artifact->execution.attempts) {
      if (attempt.placement == EdgeCloudPlacement::kLocal) {
        ++artifact->summary.local_attempt_count;
      } else if (attempt.placement == EdgeCloudPlacement::kCloud) {
        ++artifact->summary.cloud_attempt_count;
      }
      if (attempt.status == EdgeRuntimeStatus::kReady) {
        ++artifact->summary.successful_attempt_count;
      } else if (attempt.status == EdgeRuntimeStatus::kCancelled) {
        ++artifact->summary.cancelled_attempt_count;
      } else {
        ++artifact->summary.failed_attempt_count;
      }
    }
  }
  artifact->summary.delivered_chunk_count =
      artifact->execution_result_present
          ? artifact->execution.delivered_chunks
          : 0U;
  artifact->summary.fallback_attempted =
      artifact->execution_result_present &&
      artifact->execution.fallback_attempted;
  artifact->summary.fallback_performed =
      artifact->execution_result_present &&
      artifact->execution.fallback_performed;
  artifact->summary.pre_output_fallback_count =
      artifact->summary.fallback_performed ? 1U : 0U;
  const bool policy_evaluated = artifact->config.status == "parsed";
  const bool rejected =
      artifact->placement.placement == EdgeCloudPlacement::kReject;
  artifact->summary.privacy_blocker_count =
      policy_evaluated && rejected &&
              (artifact->config.data_classification == "device_only" ||
               !artifact->policy.remote_processing_allowed)
          ? 1U
          : 0U;
  artifact->summary.network_blocker_count =
      policy_evaluated && rejected &&
              artifact->policy.remote_processing_allowed &&
              artifact->config.data_classification != "device_only" &&
              !artifact->policy.network_available
          ? 1U
          : 0U;
  artifact->summary.resource_blocker_count =
      policy_evaluated && rejected && !artifact->policy.local_resource_fit
          ? 1U
          : 0U;
  artifact->summary.dependency_blocker_count =
      policy_evaluated && rejected &&
              !artifact->dependencies.local_dependency_linked &&
              !artifact->dependencies.cloud_dependency_linked
          ? 1U
          : 0U;
  artifact->summary.network_quality_issue_count =
      artifact->placement.network_quality.blockers.size();
  if (artifact->execution_result_present &&
      artifact->execution.fallback_network_quality_present) {
    const std::size_t fallback_issues =
        artifact->execution.fallback_network_quality.blockers.size();
    checked_add_count(fallback_issues,
                      &artifact->summary.network_quality_issue_count,
                      "network quality issue");
  }
  artifact->summary.budget_issue_count = 0U;
  artifact->summary.local_budget_ineligible_count = 0U;
  artifact->summary.cloud_budget_ineligible_count = 0U;
  const auto add_budget_assessment = [&artifact](
      const EdgeCloudBudgetAssessment& assessment) {
    checked_add_count(assessment.local_blockers.size(),
                      &artifact->summary.budget_issue_count,
                      "budget issue");
    checked_add_count(assessment.cloud_blockers.size(),
                      &artifact->summary.budget_issue_count,
                      "budget issue");
    if (!assessment.local_eligible) {
      checked_add_count(1U,
                        &artifact->summary.local_budget_ineligible_count,
                        "local budget ineligible");
    }
    if (!assessment.cloud_eligible) {
      checked_add_count(1U,
                        &artifact->summary.cloud_budget_ineligible_count,
                        "cloud budget ineligible");
    }
  };
  add_budget_assessment(artifact->placement.budget);
  if (artifact->execution_result_present &&
      artifact->execution.fallback_budget_present) {
    add_budget_assessment(artifact->execution.fallback_budget);
  }
  artifact->summary.session_recovery_issue_count = 0U;
  artifact->summary.session_recovery_ready_count = 0U;
  artifact->summary.session_recovery_blocked_count = 0U;
  const auto add_session_recovery_assessment = [&artifact](
      const EdgeCloudSessionRecoveryAssessment& assessment) {
    checked_add_count(assessment.blockers.size(),
                      &artifact->summary.session_recovery_issue_count,
                      "session recovery issue");
    if (assessment.status == "recovery_ready") {
      checked_add_count(1U,
                        &artifact->summary.session_recovery_ready_count,
                        "session recovery ready");
    } else if (assessment.status == "recovery_blocked") {
      checked_add_count(1U,
                        &artifact->summary.session_recovery_blocked_count,
                        "session recovery blocked");
    }
  };
  add_session_recovery_assessment(artifact->placement.session_recovery);
  if (artifact->execution_result_present &&
      artifact->execution.fallback_session_recovery_present) {
    add_session_recovery_assessment(
        artifact->execution.fallback_session_recovery);
  }
  artifact->summary.blocker_count = artifact->blockers.size();
  if (artifact->mode == Mode::kPlan) {
    artifact->summary.evidence_level = "dry_run_ready";
    artifact->summary.decision =
        artifact->placement.placement == EdgeCloudPlacement::kReject
            ? "blocked"
            : "plan_ready";
  } else if (artifact->execution_result_present &&
             artifact->execution.status == EdgeRuntimeStatus::kReady) {
    artifact->summary.evidence_level = "execute_observed";
    artifact->summary.decision = "execute_ready";
  } else {
    artifact->summary.evidence_level = "execute_blocked";
    artifact->summary.decision = "blocked";
  }
}

}  // namespace

Mode parse_mode(const std::string& value) {
  if (value == "plan") return Mode::kPlan;
  if (value == "execute") return Mode::kExecute;
  throw std::invalid_argument("edge-cloud routing mode must be plan or execute");
}

const char* mode_name(const Mode mode) noexcept {
  return mode == Mode::kExecute ? "execute" : "plan";
}

Artifact build_artifact(const std::filesystem::path& repo_root,
                        const std::filesystem::path& config_path,
                        const Mode mode,
                        const std::filesystem::path& output_path) {
  reject_output_alias(config_path, output_path, "configuration input");
  Artifact artifact;
  artifact.mode = mode;
  artifact.config.path_label =
      inferlab_plan::redacted_path_label(config_path.generic_string());
  artifact.recommended_next_commands = {
      "cmake -B build -DCMAKE_BUILD_TYPE=Release -DINFERLAB_ENABLE_EDGE_CLOUD_CURL=ON",
      "cmake --build build --target inferlab_edge_cloud_routing",
      "./build/inferlab_edge_cloud_routing --repo-root . --config configs/edge_cloud_routing.example.json --mode plan --output results/raw/native_edge_cloud_routing.json --format json",
      "Run the same command with --mode execute only after the local runtime, model, network/budget/session-recovery policy and API-key environment are ready",
  };

  const auto read = inferlab_artifact::read_text_with_status(
      config_path, kMaximumConfigBytes);
  artifact.config.status = read.status;
  if (!read.readable || read.empty) {
    append_unique(&artifact.blockers, "config:" + read.status);
    artifact.placement.reason = "configuration_unavailable";
    summarize(&artifact);
    validate_artifact(artifact);
    return artifact;
  }

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
  bool config_parsed = false;
  try {
    ParsedConfig config =
        parse_config(repo_root, read.text, output_path, &artifact.config);
    artifact.config.status = "parsed";
    config_parsed = true;
    artifact.policy = config.policy;
    artifact.session_recovery_input = config.request.session_recovery;
    artifact.placement.session_recovery =
        assess_edge_cloud_session_recovery(config.request);

    const auto local_capabilities =
        edge_runtime_capabilities_for_architecture(
            config.local_runtime_id, config.target_architecture);
    auto cloud_transport =
        create_openai_compatible_cloud_transport(config.cloud_transport);
    const auto cloud_capabilities = cloud_transport->capabilities();
    artifact.config.local_model_path_status =
        path_exists(config.local_model.model_path) ? "present" : "missing";
    artifact.dependencies.local_runtime_id = config.local_runtime_id;
    artifact.dependencies.local_dependency_linked =
        local_capabilities.dependency_linked;
    artifact.dependencies.local_streaming_supported =
        local_capabilities.supports_streaming;
    artifact.dependencies.local_model_format_supported =
        std::find(local_capabilities.model_formats.begin(),
                  local_capabilities.model_formats.end(),
                  config.local_model.model_format) !=
        local_capabilities.model_formats.end();
    artifact.dependencies.cloud_transport_id =
        cloud_capabilities.transport_id;
    artifact.dependencies.cloud_transport_version =
        cloud_capabilities.transport_version;
    artifact.dependencies.cloud_dependency_linked =
        cloud_capabilities.dependency_linked;
    artifact.dependencies.structured_json_parser_linked =
        cloud_capabilities.structured_json_parser_linked;
    artifact.dependencies.cloud_streaming_supported =
        cloud_capabilities.supports_streaming;
    artifact.dependencies.cloud_cancel_supported =
        cloud_capabilities.supports_cancel;
    artifact.dependencies.api_key_environment_configured =
        environment_value_present(
            config.cloud_transport.api_key_environment_variable);

    const bool target_platform_matches =
        artifact.config.target_platform_match_status ==
        "matches_compiled_platform";
    config.policy.local_runtime_ready = target_platform_matches &&
        local_capabilities.dependency_linked &&
        artifact.dependencies.local_streaming_supported &&
        artifact.dependencies.local_model_format_supported &&
        artifact.config.local_model_path_status == "present";
    config.policy.cloud_transport_ready = target_platform_matches &&
        cloud_capabilities.dependency_linked &&
        cloud_capabilities.structured_json_parser_linked &&
        artifact.dependencies.api_key_environment_configured;
    artifact.policy = config.policy;
    artifact.placement =
        select_edge_cloud_placement(config.request, artifact.policy);
    if (!target_platform_matches) {
      append_unique(&artifact.blockers,
                    "target_platform:mismatch_compiled_platform");
    }

    if (mode == Mode::kExecute) {
      std::unique_ptr<IEdgeRuntime> local_runtime;
      const bool local_candidate =
          artifact.placement.placement == EdgeCloudPlacement::kLocal ||
          artifact.placement.placement ==
              EdgeCloudPlacement::kLocalThenCloud;
      if (local_candidate) {
        local_runtime = create_edge_runtime(config.local_runtime_id);
        artifact.local_load.attempted = true;
        EdgeRuntimeOperationResult load;
        if (local_runtime == nullptr) {
          load.status = EdgeRuntimeStatus::kDependencyUnavailable;
          load.error_category = "local_runtime_factory_unavailable";
          load.detail =
              "local runtime factory was unavailable after capability selection";
        } else {
          try {
            load = local_runtime->load(config.local_model);
            validate_edge_runtime_operation_result(load);
          } catch (const std::exception&) {
            load.status = EdgeRuntimeStatus::kFailed;
            load.error_category = "local_load_exception";
            load.detail =
                "local runtime load raised or returned an invalid result";
          }
        }
        artifact.local_load.status = edge_runtime_status_name(load.status);
        artifact.local_load.error_category = load.error_category;
        artifact.policy.local_runtime_ready = load.ok();
        artifact.placement =
            select_edge_cloud_placement(config.request, artifact.policy);
      }

      EdgeCloudOrchestrator orchestrator(local_runtime.get(),
                                         cloud_transport.get());
      try {
        artifact.execution = orchestrator.generate(
            config.request, artifact.policy,
            [&artifact](const std::size_t, const std::string& chunk) {
              if (chunk.size() >
                  std::numeric_limits<std::size_t>::max() -
                      artifact.observed_stream_bytes) {
                return false;
              }
              artifact.observed_stream_bytes += chunk.size();
              return true;
            });
      } catch (...) {
        if (local_runtime) local_runtime->unload();
        throw;
      }
      if (local_runtime) local_runtime->unload();
      artifact.execution_result_present = true;
      artifact.executed = !artifact.execution.attempts.empty();
      if (artifact.execution.status != EdgeRuntimeStatus::kReady) {
        append_unique(&artifact.blockers,
                      "execution:" + artifact.execution.error_category);
      }
      if (artifact.execution.fallback_network_quality_present &&
          !artifact.execution.fallback_network_quality.eligible) {
        for (const auto& blocker :
             artifact.execution.fallback_network_quality.blockers) {
          append_unique(&artifact.blockers,
                        "fallback_network_quality:" + blocker);
        }
      }
      if (artifact.execution.fallback_budget_present &&
          !artifact.execution.fallback_budget.cloud_eligible) {
        for (const auto& blocker :
             artifact.execution.fallback_budget.cloud_blockers) {
          append_unique(&artifact.blockers,
                        "fallback_cloud_budget:" + blocker);
        }
      }
      if (artifact.execution.fallback_session_recovery_present &&
          !artifact.execution.fallback_session_recovery.eligible) {
        for (const auto& blocker :
             artifact.execution.fallback_session_recovery.blockers) {
          append_unique(&artifact.blockers,
                        "fallback_session_recovery:" + blocker);
        }
      }
    }
  } catch (const ProtectedOutputAlias&) {
    throw;
  } catch (const std::exception&) {
    if (artifact.observed_stream_bytes != 0U) {
      throw;
    }
    if (!config_parsed) {
      artifact.config.status = "malformed";
      artifact.config.schema_version = 0U;
      artifact.placement = {};
      artifact.placement.reason = "configuration_malformed";
      append_unique(&artifact.blockers, "config:malformed");
    } else if (mode == Mode::kExecute) {
      if (artifact.placement.reason.empty()) {
        artifact.placement.placement = EdgeCloudPlacement::kReject;
        artifact.placement.reason = "execution_setup_failed";
      }
      artifact.execution = {};
      artifact.execution.decision = artifact.placement;
      artifact.execution.status = EdgeRuntimeStatus::kFailed;
      artifact.execution.error_category = "execution_exception";
      artifact.execution.detail =
          "edge-cloud execution raised an internal exception";
      artifact.execution_result_present = true;
      artifact.executed = false;
      append_unique(&artifact.blockers, "execution:execution_exception");
    } else {
      const EdgeCloudSessionRecoveryAssessment session_recovery =
          artifact.placement.session_recovery;
      artifact.placement = {};
      artifact.placement.reason = "readiness_evaluation_failed";
      artifact.placement.session_recovery = session_recovery;
      append_unique(&artifact.blockers, "readiness:evaluation_failed");
    }
  }
#else
  artifact.config.status = "structured_json_parser_unavailable";
  artifact.placement.reason = "structured_json_parser_unavailable";
  append_unique(&artifact.blockers,
                "config:structured_json_parser_unavailable");
#endif

  if (artifact.placement.placement == EdgeCloudPlacement::kReject) {
    for (const auto& blocker :
         artifact.placement.network_quality.blockers) {
      append_unique(&artifact.blockers,
                    "network_quality:" + blocker);
    }
    for (const auto& blocker : artifact.placement.budget.local_blockers) {
      append_unique(&artifact.blockers, "local_budget:" + blocker);
    }
    for (const auto& blocker : artifact.placement.budget.cloud_blockers) {
      append_unique(&artifact.blockers, "cloud_budget:" + blocker);
    }
    for (const auto& blocker :
         artifact.placement.session_recovery.blockers) {
      append_unique(&artifact.blockers, "session_recovery:" + blocker);
    }
    append_unique(&artifact.blockers,
                  "placement:" + artifact.placement.reason);
  }
  summarize(&artifact);
  validate_artifact(artifact);
  return artifact;
}

void validate_artifact(const Artifact& artifact) {
  validate_edge_cloud_budget_assessment(artifact.placement.budget);
  if (artifact.execution_result_present &&
      artifact.execution.fallback_budget_present) {
    validate_edge_cloud_budget_assessment(
        artifact.execution.fallback_budget);
  }
  validate_edge_cloud_session_recovery_assessment(
      artifact.placement.session_recovery);
  if (artifact.execution_result_present &&
      artifact.execution.fallback_session_recovery_present) {
    validate_edge_cloud_session_recovery_assessment(
        artifact.execution.fallback_session_recovery);
  }
  const std::size_t fallback_budget_issue_count =
      artifact.execution_result_present &&
              artifact.execution.fallback_budget_present
          ? artifact.execution.fallback_budget.local_blockers.size() +
                artifact.execution.fallback_budget.cloud_blockers.size()
          : 0U;
  const std::size_t expected_budget_issue_count =
      artifact.placement.budget.local_blockers.size() +
      artifact.placement.budget.cloud_blockers.size() +
      fallback_budget_issue_count;
  const std::size_t expected_local_budget_ineligible_count =
      (artifact.placement.budget.local_eligible ? 0U : 1U) +
      (artifact.execution_result_present &&
               artifact.execution.fallback_budget_present &&
               !artifact.execution.fallback_budget.local_eligible
           ? 1U
           : 0U);
  const std::size_t expected_cloud_budget_ineligible_count =
      (artifact.placement.budget.cloud_eligible ? 0U : 1U) +
      (artifact.execution_result_present &&
               artifact.execution.fallback_budget_present &&
               !artifact.execution.fallback_budget.cloud_eligible
           ? 1U
           : 0U);
  const std::size_t expected_session_recovery_issue_count =
      artifact.placement.session_recovery.blockers.size() +
      (artifact.execution_result_present &&
               artifact.execution.fallback_session_recovery_present
           ? artifact.execution.fallback_session_recovery.blockers.size()
           : 0U);
  const std::size_t expected_session_recovery_ready_count =
      (artifact.placement.session_recovery.status == "recovery_ready" ? 1U
                                                                      : 0U) +
      (artifact.execution_result_present &&
               artifact.execution.fallback_session_recovery_present &&
               artifact.execution.fallback_session_recovery.status ==
                   "recovery_ready"
           ? 1U
           : 0U);
  const std::size_t expected_session_recovery_blocked_count =
      (artifact.placement.session_recovery.status == "recovery_blocked" ? 1U
                                                                        : 0U) +
      (artifact.execution_result_present &&
               artifact.execution.fallback_session_recovery_present &&
               artifact.execution.fallback_session_recovery.status ==
                   "recovery_blocked"
           ? 1U
           : 0U);
  if (artifact.config.path_label.empty() || artifact.config.status.empty() ||
      artifact.placement.reason.empty() ||
      artifact.summary.evidence_level.empty() ||
      artifact.summary.decision.empty() ||
      artifact.summary.blocker_count != artifact.blockers.size() ||
      artifact.summary.attempt_count !=
          (artifact.execution_result_present
               ? artifact.execution.attempts.size()
               : 0U) ||
      artifact.summary.local_decision_count +
              artifact.summary.cloud_decision_count +
              artifact.summary.local_then_cloud_decision_count +
              artifact.summary.reject_decision_count !=
          1U ||
      artifact.summary.local_attempt_count +
              artifact.summary.cloud_attempt_count !=
          artifact.summary.attempt_count ||
      artifact.summary.successful_attempt_count +
              artifact.summary.failed_attempt_count +
              artifact.summary.cancelled_attempt_count !=
          artifact.summary.attempt_count ||
      artifact.summary.pre_output_fallback_count !=
          (artifact.summary.fallback_performed ? 1U : 0U) ||
      artifact.summary.network_quality_issue_count !=
          artifact.placement.network_quality.blockers.size() +
              (artifact.execution_result_present &&
                       artifact.execution.fallback_network_quality_present
                   ? artifact.execution.fallback_network_quality.blockers
                         .size()
                   : 0U) ||
      artifact.summary.budget_issue_count !=
          expected_budget_issue_count ||
      artifact.summary.local_budget_ineligible_count !=
          expected_local_budget_ineligible_count ||
      artifact.summary.cloud_budget_ineligible_count !=
          expected_cloud_budget_ineligible_count ||
      artifact.summary.session_recovery_issue_count !=
          expected_session_recovery_issue_count ||
      artifact.summary.session_recovery_ready_count !=
          expected_session_recovery_ready_count ||
      artifact.summary.session_recovery_blocked_count !=
          expected_session_recovery_blocked_count ||
      artifact.config.session_recovery_requested !=
          artifact.session_recovery_input.recovery_requested ||
      artifact.executed != (artifact.execution_result_present &&
                            !artifact.execution.attempts.empty())) {
    throw std::logic_error("edge-cloud routing artifact invariant failed");
  }
  if ((!artifact.local_load.attempted &&
       (artifact.local_load.status != "not_attempted" ||
        !artifact.local_load.error_category.empty())) ||
      (artifact.local_load.attempted &&
       (artifact.local_load.status == "not_attempted" ||
        ((artifact.local_load.status == "ready") !=
         artifact.local_load.error_category.empty())))) {
    throw std::logic_error("edge-cloud local-load evidence is inconsistent");
  }
  if (artifact.config.status == "parsed" &&
      ((artifact.config.prompt_fingerprint_redacted_by_policy &&
        !artifact.config.prompt_fingerprint.empty()) ||
       (!artifact.config.prompt_fingerprint_redacted_by_policy &&
        artifact.config.prompt_fingerprint.empty()))) {
    throw std::logic_error(
        "edge-cloud prompt fingerprint redaction is inconsistent");
  }
  if (artifact.execution_result_present) {
    validate_edge_cloud_generation_result(artifact.execution);
    if (artifact.summary.delivered_chunk_count !=
            artifact.execution.delivered_chunks ||
        artifact.summary.fallback_attempted !=
            artifact.execution.fallback_attempted ||
        artifact.summary.fallback_performed !=
            artifact.execution.fallback_performed) {
      throw std::logic_error(
          "edge-cloud execution summary does not reconcile");
    }
  } else if (artifact.executed || artifact.observed_stream_bytes != 0U) {
    throw std::logic_error(
        "edge-cloud plan artifact must not contain execution observations");
  }
  if (artifact.mode == Mode::kPlan && artifact.executed) {
    throw std::logic_error("edge-cloud plan mode cannot be executed");
  }
}

}  // namespace inferlab::edge_cloud_routing
