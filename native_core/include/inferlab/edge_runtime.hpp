#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace inferlab {

inline constexpr std::size_t kMaximumEdgePromptBytes = 1024U * 1024U;
inline constexpr std::size_t kMaximumEdgeTokenTextBytes = 1024U * 1024U;
inline constexpr std::uint64_t kDefaultEdgeGenerationDurationMs = 300000U;
inline constexpr std::uint64_t kMaximumEdgeGenerationDurationMs = 3600000U;

enum class EdgeRuntimeStatus {
  kReady,
  kDependencyUnavailable,
  kInvalidConfiguration,
  kNotLoaded,
  kCancelled,
  kFailed,
};

const char* edge_runtime_status_name(EdgeRuntimeStatus status) noexcept;

struct EdgeRuntimeCapabilities {
  std::string runtime_id;
  std::string runtime_version;
  std::vector<std::string> model_formats;
  std::vector<std::string> backends;
  std::vector<std::string> instruction_sets;
  bool dependency_linked = false;
  bool supports_streaming = false;
  bool supports_cancel = false;
  bool supports_mmap = false;
  bool supports_explicit_thread_count = false;
};

struct EdgeModelDescriptor {
  std::string logical_model_id;
  std::string model_format;
  std::string quantization;
  std::string tokenizer_id;
  std::string checkpoint_revision;
  std::string conversion_provenance;
  std::filesystem::path model_path;
  std::filesystem::path tokenizer_path;
  std::string model_path_label;
  std::size_t context_limit = 0U;
  bool prefer_mmap = false;
};

struct EdgeGenerationRequest {
  std::string prompt;
  std::size_t maximum_output_tokens = 0U;
  std::size_t context_tokens = 0U;
  // The current contract carries one prompt. Runtime-internal token batching
  // must not reuse this request-cardinality field.
  std::size_t batch_size = 1U;
  std::size_t thread_count = 1U;
  std::uint64_t maximum_generation_duration_ms =
      kDefaultEdgeGenerationDurationMs;
  std::uint64_t seed = 0U;
  float temperature = 0.0F;
  float top_p = 1.0F;
};

struct EdgeRuntimeMetrics {
  std::optional<double> load_time_ms;
  std::optional<double> cold_ttft_ms;
  std::optional<double> warm_ttft_ms;
  std::optional<double> tpot_ms;
  std::optional<double> prompt_tokens_per_second;
  std::optional<double> decode_tokens_per_second;
  std::optional<std::uint64_t> peak_rss_bytes;
  std::optional<std::uint64_t> peak_pss_bytes;
  std::optional<std::uint64_t> mapped_model_bytes;
  std::optional<std::uint64_t> kv_cache_bytes;
  std::optional<std::uint64_t> runtime_workspace_bytes;
  std::string provenance;
};

struct EdgeRuntimeOperationResult {
  EdgeRuntimeStatus status = EdgeRuntimeStatus::kFailed;
  std::string error_category;
  std::string detail;

  bool ok() const noexcept { return status == EdgeRuntimeStatus::kReady; }
};

struct EdgeGenerationResult : EdgeRuntimeOperationResult {
  std::size_t prompt_tokens = 0U;
  std::size_t generated_tokens = 0U;
  std::string finish_reason;
  EdgeRuntimeMetrics metrics;
};

using EdgeTokenCallback =
    std::function<bool(std::size_t token_index, const std::string& token_text)>;

class IEdgeRuntime {
 public:
  virtual ~IEdgeRuntime() = default;

  virtual EdgeRuntimeCapabilities capabilities() const = 0;
  virtual EdgeRuntimeOperationResult load(
      const EdgeModelDescriptor& model) = 0;
  virtual EdgeGenerationResult generate(
      const EdgeGenerationRequest& request,
      const EdgeTokenCallback& on_token) = 0;

  // load() is fail-closed and replaces any prior model. generate() opens a new
  // cooperative-cancellation window at entry; a later cancel() may be called
  // while it is active and is idempotent. Other lifecycle calls are serialized
  // by the caller. unload() is valid after partial load failure.
  virtual void cancel() noexcept = 0;
  virtual EdgeRuntimeOperationResult reset() = 0;
  virtual void unload() noexcept = 0;
};

struct EdgeRuntimeRegistration {
  EdgeRuntimeCapabilities declared_capabilities;
  std::string cmake_option;
  bool build_option_enabled = false;
  bool adapter_implemented = false;
  std::string capability_scope;
};

enum class EdgeRuntimeArchitecture {
  kAarch64,
  kX86_64,
  kOther,
};

struct EdgeRuntimeTargetPlatform {
  std::string id;
  std::string build_system;
  std::string build_architecture;
  EdgeRuntimeArchitecture runtime_architecture =
      EdgeRuntimeArchitecture::kOther;
};

const std::vector<EdgeRuntimeRegistration>& edge_runtime_registrations();
const std::vector<EdgeRuntimeTargetPlatform>& edge_runtime_target_platforms();
const EdgeRuntimeTargetPlatform* find_edge_runtime_target_platform(
    const std::string& platform_id);
const EdgeRuntimeTargetPlatform* current_edge_runtime_target_platform();
const EdgeRuntimeRegistration* find_edge_runtime_registration(
    const std::string& runtime_id);
EdgeRuntimeCapabilities edge_runtime_capabilities_for_architecture(
    const std::string& runtime_id, EdgeRuntimeArchitecture architecture);
EdgeRuntimeCapabilities edge_runtime_capabilities_for_current_build(
    const std::string& runtime_id);
std::unique_ptr<IEdgeRuntime> create_edge_runtime(
    const std::string& runtime_id);

void validate_edge_model_descriptor(const EdgeModelDescriptor& model);
void validate_edge_generation_request(const EdgeGenerationRequest& request);
void validate_edge_runtime_operation_result(
    const EdgeRuntimeOperationResult& result);
void validate_edge_generation_result(const EdgeGenerationResult& result);
void validate_edge_generation_result_for_request(
    const EdgeGenerationResult& result,
    const EdgeGenerationRequest& request);

}  // namespace inferlab
