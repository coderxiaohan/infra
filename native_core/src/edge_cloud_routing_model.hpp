#pragma once

#include "inferlab/edge_cloud.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::edge_cloud_routing {

enum class Mode {
  kPlan,
  kExecute,
};

struct ConfigEvidence {
  std::string path_label;
  std::string status;
  std::uint64_t schema_version = 0U;
  std::string target_platform;
  std::string target_platform_match_status;
  std::string request_fingerprint;
  std::string prompt_fingerprint;
  bool prompt_fingerprint_redacted_by_policy = false;
  bool session_recovery_requested = false;
  std::size_t prompt_bytes = 0U;
  std::string local_model_path_label;
  std::string local_model_path_status;
  std::string endpoint_security;
  std::string api_key_environment_variable;
  std::string product_id;
  std::string cloud_model_id;
  std::string data_classification;
};

struct DependencyEvidence {
  std::string local_runtime_id;
  bool local_dependency_linked = false;
  bool local_streaming_supported = false;
  bool local_model_format_supported = false;
  std::string cloud_transport_id;
  std::string cloud_transport_version;
  bool cloud_dependency_linked = false;
  bool structured_json_parser_linked = false;
  bool cloud_streaming_supported = false;
  bool cloud_cancel_supported = false;
  bool api_key_environment_configured = false;
};

struct LocalLoadEvidence {
  bool attempted = false;
  std::string status = "not_attempted";
  std::string error_category;
};

struct Summary {
  std::string evidence_level;
  std::string decision;
  std::size_t local_decision_count = 0U;
  std::size_t cloud_decision_count = 0U;
  std::size_t local_then_cloud_decision_count = 0U;
  std::size_t reject_decision_count = 0U;
  std::size_t attempt_count = 0U;
  std::size_t local_attempt_count = 0U;
  std::size_t cloud_attempt_count = 0U;
  std::size_t successful_attempt_count = 0U;
  std::size_t failed_attempt_count = 0U;
  std::size_t cancelled_attempt_count = 0U;
  std::size_t delivered_chunk_count = 0U;
  std::size_t pre_output_fallback_count = 0U;
  std::size_t privacy_blocker_count = 0U;
  std::size_t network_blocker_count = 0U;
  std::size_t resource_blocker_count = 0U;
  std::size_t dependency_blocker_count = 0U;
  std::size_t network_quality_issue_count = 0U;
  std::size_t budget_issue_count = 0U;
  std::size_t local_budget_ineligible_count = 0U;
  std::size_t cloud_budget_ineligible_count = 0U;
  std::size_t session_recovery_issue_count = 0U;
  std::size_t session_recovery_ready_count = 0U;
  std::size_t session_recovery_blocked_count = 0U;
  std::size_t blocker_count = 0U;
  bool fallback_attempted = false;
  bool fallback_performed = false;
};

struct Artifact {
  Mode mode = Mode::kPlan;
  bool executed = false;
  ConfigEvidence config;
  DependencyEvidence dependencies;
  EdgeCloudPolicyInput policy;
  EdgeCloudSessionRecoveryInput session_recovery_input;
  EdgeCloudDecision placement;
  LocalLoadEvidence local_load;
  EdgeCloudGenerationResult execution;
  bool execution_result_present = false;
  std::size_t observed_stream_bytes = 0U;
  Summary summary;
  std::vector<std::string> blockers;
  std::vector<std::string> recommended_next_commands;
};

Mode parse_mode(const std::string& value);
const char* mode_name(Mode mode) noexcept;
Artifact build_artifact(const std::filesystem::path& repo_root,
                        const std::filesystem::path& config_path,
                        Mode mode,
                        const std::filesystem::path& output_path);
void validate_artifact(const Artifact& artifact);

}  // namespace inferlab::edge_cloud_routing
