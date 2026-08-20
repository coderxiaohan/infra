#pragma once

#include "inferlab/edge_kernels.hpp"
#include "inferlab/edge_runtime.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace inferlab::edge_runtime_matrix {

inline constexpr std::size_t kMaximumConcurrentExecuteSessions = 64U;
inline constexpr std::size_t kMaximumEdgeRuntimeMatrixRows = 64U;
inline constexpr std::size_t kMaximumGeneratedTextEvidenceBytes =
    1024U * 1024U;
inline constexpr std::size_t kMaximumGeneratedTextMatrixEvidenceBytes =
    8U * 1024U * 1024U;

enum class Mode {
  kPlan,
  kExecute,
};

struct ExecutionProtocol {
  std::size_t warmup_iterations = 0U;
  std::size_t minimum_measurement_iterations = 1U;
  std::uint64_t minimum_sustained_duration_ms = 0U;
  std::size_t maximum_measurement_iterations = 1U;
  std::uint64_t maximum_generation_duration_ms =
      kDefaultEdgeGenerationDurationMs;
  std::size_t completed_warmup_iterations = 0U;
  std::size_t completed_measurement_iterations = 0U;
  std::optional<double> observed_measurement_duration_ms;
  std::string status;
  std::string aggregation;
};

struct ConcurrentExecutionObservation {
  std::size_t requested_sessions = 0U;
  std::size_t loaded_sessions = 0U;
  std::size_t warmed_sessions = 0U;
  std::size_t started_sessions = 0U;
  std::size_t completed_sessions = 0U;
  std::size_t total_prompt_tokens = 0U;
  std::size_t total_generated_tokens = 0U;
  std::size_t total_callback_count = 0U;
  std::optional<double> wall_time_ms;
  std::optional<double> median_warm_ttft_ms;
  std::optional<double> median_tpot_ms;
  std::optional<double> aggregate_generated_tokens_per_second;
  std::optional<std::uint64_t> peak_rss_bytes;
  std::optional<std::uint64_t> peak_pss_bytes;
  std::optional<std::uint64_t> mapped_model_bytes;
  std::string finish_alignment_status;
  std::string status;
  std::string provenance;
};

struct RuntimeRow {
  std::string row_id;
  std::string runtime_id;
  std::string runtime_version;
  std::string model_format;
  std::string backend;
  std::string logical_model_id;
  std::string checkpoint_revision;
  std::string tokenizer_id;
  std::string conversion_provenance;
  std::string quantization;
  std::string tokenizer_path_label;
  std::string tokenizer_path_status;
  std::string prompt_fingerprint;
  std::size_t prompt_bytes = 0U;
  std::uint64_t seed = 0U;
  float temperature = 0.0F;
  float top_p = 1.0F;
  std::string model_path_label;
  std::string model_path_status;
  std::string model_input_snapshot_source;
  std::string model_input_stability_status;
  std::string tokenizer_input_snapshot_source;
  std::string tokenizer_input_stability_status;
  std::size_t thread_count = 0U;
  std::size_t target_physical_core_count = 0U;
  std::size_t recommended_thread_count = 0U;
  std::string thread_recommendation_status;
  std::size_t context_tokens = 0U;
  std::size_t batch_size = 0U;
  std::size_t concurrent_sessions = 0U;
  std::size_t maximum_output_tokens = 0U;
  ExecutionProtocol execution_protocol;
  ConcurrentExecutionObservation concurrent_execution;
  int layers = 0;
  int kv_heads = 0;
  int head_dim = 0;
  std::string kv_dtype;
  bool mmap_requested = false;
  std::string mmap_strategy;
  std::uint64_t runtime_workspace_budget_bytes = 0U;
  std::uint64_t system_memory_budget_bytes = 0U;
  std::uint64_t system_reserve_bytes = 0U;
  std::optional<std::uint64_t> model_file_bytes;
  std::optional<std::uint64_t> estimated_model_residency_total_bytes;
  std::optional<std::uint64_t> estimated_kv_cache_bytes;
  std::optional<std::uint64_t> estimated_runtime_workspace_total_bytes;
  std::optional<std::uint64_t> estimated_required_bytes;
  std::optional<bool> estimated_resource_fit;
  std::optional<std::uint64_t> estimated_headroom_bytes;
  std::optional<std::uint64_t> estimated_shortfall_bytes;
  std::string resource_plan_status;
  bool build_option_enabled = false;
  bool dependency_linked = false;
  bool adapter_implemented = false;
  bool target_runtime_contract_supported = false;
  std::string thread_control_status;
  bool executed = false;
  std::size_t observed_prompt_tokens = 0U;
  std::size_t observed_generated_tokens = 0U;
  std::size_t token_callback_count = 0U;
  std::string finish_reason;
  std::string execution_error_category;
  std::optional<double> thermal_before_c;
  std::optional<double> thermal_after_c;
  std::size_t thermal_sensor_count_before = 0U;
  std::size_t thermal_sensor_count_after = 0U;
  std::string thermal_before_status;
  std::string thermal_after_status;
  std::string thermal_provenance;
  std::optional<std::uint64_t> observed_energy_uj;
  std::size_t energy_root_zone_count = 0U;
  std::string energy_before_status;
  std::string energy_after_status;
  std::string energy_status;
  std::string energy_provenance;
  std::string capability_status;
  std::string load_status;
  std::string generate_status;
  std::string cancel_status;
  std::string reset_status;
  std::string unload_status;
  EdgeRuntimeMetrics metrics;
  std::optional<std::uint64_t> observed_working_set_bytes;
  std::string observed_working_set_source;
  std::optional<bool> observed_resource_fit;
  std::optional<std::uint64_t> observed_headroom_bytes;
  std::optional<std::uint64_t> observed_shortfall_bytes;
  std::string observed_resource_status;
  std::vector<std::string> blockers;
};

struct DeploymentRecommendation {
  std::string status;
  std::vector<std::string> candidate_row_ids;
  std::string observation_alignment_status;
  std::vector<std::string> observation_alignment_gaps;
  std::string selected_row_id;
  std::string runtime_id;
  std::string backend;
  std::string model_format;
  std::size_t thread_count = 0U;
  std::string thread_count_status;
  std::string mmap_strategy;
  std::string criterion;
};

struct GeneratedTextQualityObservation {
  std::string row_id;
  std::optional<std::size_t> generated_text_bytes;
  std::string generated_text_fingerprint;
  std::string capture_status;
  bool exact_match_with_reference = false;
  std::optional<std::size_t> first_mismatch_byte_offset;
  // Request-local comparison material. summarize() clears this before the
  // Matrix may be validated or rendered.
  std::string retained_text_for_comparison;
};

struct QualityComparison {
  std::string status;
  std::string reference_row_id;
  std::vector<GeneratedTextQualityObservation> observations;
  std::vector<std::string> matching_row_ids;
  std::vector<std::string> mismatching_row_ids;
  std::vector<std::string> unavailable_row_ids;
  std::string criterion;
};

struct DeliveryContract {
  std::vector<std::string> required_directories;
  std::vector<std::string> manifest_required_fields;
  std::string package_root;
  std::string status;
};

struct KernelCapability {
  std::string operator_id;
  bool scalar_available = true;
  bool neon_compiled = false;
  bool neon_runtime_available = false;
  std::string selected_backend;
  std::string selection_status;
};

struct Summary {
  std::size_t runtime_row_count = 0U;
  std::size_t dependency_linked_count = 0U;
  std::size_t adapter_implemented_count = 0U;
  std::size_t model_present_count = 0U;
  std::size_t executed_row_count = 0U;
  std::size_t blocker_count = 0U;
  std::string evidence_level;
  std::string decision;
};

struct Matrix {
  Mode mode = Mode::kPlan;
  std::string target_platform;
  std::string build_system;
  std::string build_architecture;
  std::string target_platform_match_status;
  std::string config_path_label;
  std::string config_status;
  std::uint64_t config_schema_version = 0U;
  EdgeIsaCapabilities isa_capabilities;
  std::vector<KernelCapability> kernel_capabilities;
  std::vector<RuntimeRow> runtime_rows;
  std::string comparison_identity_status;
  std::vector<std::string> comparison_identity_gaps;
  QualityComparison quality_comparison;
  std::vector<std::string> blockers;
  std::vector<std::string> recommended_next_commands;
  DeploymentRecommendation deployment_recommendation;
  DeliveryContract delivery_contract;
  Summary summary;
};

Mode parse_mode(const std::string& value);
const char* mode_name(Mode mode) noexcept;
Matrix build_matrix(const std::filesystem::path& repo_root,
                    const std::filesystem::path& config_path, Mode mode,
                    const std::filesystem::path& output_path);
void validate_matrix(const Matrix& matrix);

}  // namespace inferlab::edge_runtime_matrix
