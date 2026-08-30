#include "inferlab/edge_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

#if defined(INFERLAB_EDGE_LLAMA_CPP_LINKED) && \
    INFERLAB_EDGE_LLAMA_CPP_LINKED
#include "edge_llama_cpp_runtime.hpp"
#include <llama.h>
#endif

#if defined(INFERLAB_EDGE_EXECUTORCH_LINKED) && \
    INFERLAB_EDGE_EXECUTORCH_LINKED
#include "edge_executorch_runtime.hpp"
#endif

#if !defined(INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD)
#define INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD 0
#endif

#if !defined(INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD)
#define INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD 0
#endif

#if !defined(INFERLAB_EDGE_LLAMA_CPP_LINKED)
#define INFERLAB_EDGE_LLAMA_CPP_LINKED 0
#endif

#if !defined(INFERLAB_EDGE_EXECUTORCH_LINKED)
#define INFERLAB_EDGE_EXECUTORCH_LINKED 0
#endif

namespace inferlab {

namespace {

void validate_capability_values(const std::vector<std::string>& values,
                                const char* field) {
  std::unordered_set<std::string> seen;
  for (const auto& value : values) {
    if (value.empty()) {
      throw std::logic_error(std::string("edge runtime ") + field +
                             " contains an empty value");
    }
    if (!seen.insert(value).second) {
      throw std::logic_error(std::string("edge runtime ") + field +
                             " contains a duplicate value: " + value);
    }
  }
}

void erase_capability(std::vector<std::string>* values,
                      const std::string& value) {
  values->erase(std::remove(values->begin(), values->end(), value),
                values->end());
}

}  // namespace

const char* edge_runtime_status_name(const EdgeRuntimeStatus status) noexcept {
  switch (status) {
    case EdgeRuntimeStatus::kReady:
      return "ready";
    case EdgeRuntimeStatus::kDependencyUnavailable:
      return "dependency_unavailable";
    case EdgeRuntimeStatus::kInvalidConfiguration:
      return "invalid_configuration";
    case EdgeRuntimeStatus::kNotLoaded:
      return "not_loaded";
    case EdgeRuntimeStatus::kCancelled:
      return "cancelled";
    case EdgeRuntimeStatus::kFailed:
      return "failed";
  }
  return "unknown";
}

const std::vector<EdgeRuntimeTargetPlatform>& edge_runtime_target_platforms() {
  static const std::vector<EdgeRuntimeTargetPlatform> platforms = {
      {"linux-aarch64", "linux", "aarch64",
       EdgeRuntimeArchitecture::kAarch64},
      {"linux-x86_64", "linux", "x86_64",
       EdgeRuntimeArchitecture::kX86_64},
  };
  static const bool validated = [&] {
    for (std::size_t index = 0U; index < platforms.size(); ++index) {
      const auto& platform = platforms[index];
      if (platform.id.empty() || platform.build_system.empty() ||
          platform.build_architecture.empty() ||
          platform.runtime_architecture == EdgeRuntimeArchitecture::kOther) {
        throw std::logic_error(
            "edge runtime target platform has an invalid required field");
      }
      for (std::size_t prior = 0U; prior < index; ++prior) {
        if (platforms[prior].id == platform.id) {
          throw std::logic_error(
              "duplicate edge runtime target platform: " + platform.id);
        }
      }
    }
    return true;
  }();
  (void)validated;
  return platforms;
}

const EdgeRuntimeTargetPlatform* find_edge_runtime_target_platform(
    const std::string& platform_id) {
  for (const auto& platform : edge_runtime_target_platforms()) {
    if (platform.id == platform_id) return &platform;
  }
  return nullptr;
}

const EdgeRuntimeTargetPlatform* current_edge_runtime_target_platform() {
#if defined(__linux__) && (defined(__aarch64__) || defined(_M_ARM64))
  return find_edge_runtime_target_platform("linux-aarch64");
#elif defined(__linux__) && (defined(__x86_64__) || defined(_M_X64))
  return find_edge_runtime_target_platform("linux-x86_64");
#else
  return nullptr;
#endif
}

const std::vector<EdgeRuntimeRegistration>& edge_runtime_registrations() {
  static const std::vector<EdgeRuntimeRegistration> registrations = {
      {{"llama_cpp",
         "11cd988",
         {"gguf"},
         {"cpu", "cpu-neon"},
         {"scalar", "arm_neon"},
         INFERLAB_EDGE_LLAMA_CPP_LINKED != 0,
         true,
         true,
         true,
         true},
       "INFERLAB_ENABLE_EDGE_LLAMA_CPP",
       INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD != 0,
       INFERLAB_EDGE_LLAMA_CPP_LINKED != 0,
       INFERLAB_EDGE_LLAMA_CPP_LINKED != 0
           ? "linked_llama_cpp_11cd988_adapter"
           : (INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD != 0
                  ? "adapter_requested_dependency_gate_failed"
                  : "adapter_disabled_by_build_option")},
      {{"executorch",
         "v1.2.0",
         {"pte"},
         {"xnnpack"},
         {"scalar", "arm_neon"},
         INFERLAB_EDGE_EXECUTORCH_LINKED != 0,
         true,
         true,
         true,
         false},
       "INFERLAB_ENABLE_EDGE_EXECUTORCH",
       INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD != 0,
       INFERLAB_EDGE_EXECUTORCH_LINKED != 0,
       INFERLAB_EDGE_EXECUTORCH_LINKED != 0
           ? "linked_executorch_v1_2_0_xnnpack_adapter"
           : (INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD != 0
                  ? "adapter_requested_dependency_gate_failed"
                  : "adapter_disabled_by_build_option")},
  };
  static const bool validated = [&] {
    for (std::size_t index = 0U; index < registrations.size(); ++index) {
      const auto& registration = registrations[index];
      const auto& capabilities = registration.declared_capabilities;
      if (capabilities.runtime_id.empty() ||
          capabilities.runtime_version.empty() ||
          capabilities.model_formats.empty() || capabilities.backends.empty() ||
          capabilities.instruction_sets.empty() ||
          registration.cmake_option.empty() ||
          registration.capability_scope.empty()) {
        throw std::logic_error(
            "edge runtime registration contains an empty required field");
      }
      if (registration.adapter_implemented &&
          !capabilities.dependency_linked) {
        throw std::logic_error(
            "implemented edge runtime adapter requires a linked dependency");
      }
      if (capabilities.dependency_linked &&
          !registration.build_option_enabled) {
        throw std::logic_error(
            "linked edge runtime dependency requires an enabled build option");
      }
      if (registration.adapter_implemented !=
          capabilities.dependency_linked) {
        throw std::logic_error(
            "edge runtime factory availability must match dependency linkage");
      }
      validate_capability_values(capabilities.model_formats,
                                 "model_formats");
      validate_capability_values(capabilities.backends, "backends");
      validate_capability_values(capabilities.instruction_sets,
                                 "instruction_sets");
      for (std::size_t prior = 0U; prior < index; ++prior) {
        if (registrations[prior].declared_capabilities.runtime_id ==
            capabilities.runtime_id) {
          throw std::logic_error("duplicate edge runtime registration: " +
                                 capabilities.runtime_id);
        }
      }
    }
    return true;
  }();
  (void)validated;
  return registrations;
}

const EdgeRuntimeRegistration* find_edge_runtime_registration(
    const std::string& runtime_id) {
  for (const auto& registration : edge_runtime_registrations()) {
    if (registration.declared_capabilities.runtime_id == runtime_id) {
      return &registration;
    }
  }
  return nullptr;
}

EdgeRuntimeCapabilities edge_runtime_capabilities_for_current_build(
    const std::string& runtime_id) {
  EdgeRuntimeArchitecture architecture = EdgeRuntimeArchitecture::kOther;
#if defined(__aarch64__) || defined(_M_ARM64)
  architecture = EdgeRuntimeArchitecture::kAarch64;
#elif defined(__x86_64__) || defined(_M_X64)
  architecture = EdgeRuntimeArchitecture::kX86_64;
#endif
  auto capabilities =
      edge_runtime_capabilities_for_architecture(runtime_id, architecture);
#if INFERLAB_EDGE_LLAMA_CPP_LINKED
  if (runtime_id == "llama_cpp") {
    capabilities.supports_mmap = llama_supports_mmap();
  }
#endif
  return capabilities;
}

EdgeRuntimeCapabilities edge_runtime_capabilities_for_architecture(
    const std::string& runtime_id,
    const EdgeRuntimeArchitecture architecture) {
  const auto* registration = find_edge_runtime_registration(runtime_id);
  if (registration == nullptr) {
    throw std::invalid_argument("unknown edge runtime id: " + runtime_id);
  }
  auto capabilities = registration->declared_capabilities;
  switch (architecture) {
    case EdgeRuntimeArchitecture::kAarch64:
      break;
    case EdgeRuntimeArchitecture::kX86_64:
    case EdgeRuntimeArchitecture::kOther:
      erase_capability(&capabilities.backends, "cpu-neon");
      erase_capability(&capabilities.instruction_sets, "arm_neon");
      break;
    default:
      throw std::invalid_argument("unknown edge runtime architecture");
  }
  return capabilities;
}

std::unique_ptr<IEdgeRuntime> create_edge_runtime(
    const std::string& runtime_id) {
#if INFERLAB_EDGE_LLAMA_CPP_LINKED
  if (runtime_id == "llama_cpp") {
    return make_llama_cpp_edge_runtime();
  }
#endif
#if INFERLAB_EDGE_EXECUTORCH_LINKED
  if (runtime_id == "executorch") {
    return make_executorch_edge_runtime();
  }
#endif
  return nullptr;
}

void validate_edge_model_descriptor(const EdgeModelDescriptor& model) {
  if (model.logical_model_id.empty() || model.model_format.empty() ||
      model.quantization.empty() || model.tokenizer_id.empty() ||
      model.checkpoint_revision.empty() ||
      model.conversion_provenance.empty() ||
      model.model_path.empty() || model.model_path_label.empty()) {
    throw std::invalid_argument(
        "edge model identity, checkpoint, conversion, format, quantization, "
        "tokenizer, path and path label must be nonempty");
  }
  if (model.context_limit == 0U) {
    throw std::invalid_argument("edge model context limit must be positive");
  }
}

void validate_edge_generation_request(const EdgeGenerationRequest& request) {
  if (request.prompt.empty()) {
    throw std::invalid_argument("edge generation prompt must not be empty");
  }
  if (request.prompt.size() > kMaximumEdgePromptBytes) {
    throw std::invalid_argument(
        "edge generation prompt exceeds the runtime byte limit");
  }
  if (request.maximum_output_tokens == 0U || request.context_tokens == 0U ||
      request.batch_size == 0U || request.thread_count == 0U) {
    throw std::invalid_argument(
        "edge generation token, context, batch and thread counts must be "
        "positive");
  }
  if (request.maximum_output_tokens > request.context_tokens) {
    throw std::invalid_argument(
        "edge generation maximum output exceeds context capacity");
  }
  if (request.batch_size != 1U) {
    throw std::invalid_argument(
        "edge generation currently supports exactly one prompt per request");
  }
  if (request.maximum_generation_duration_ms == 0U ||
      request.maximum_generation_duration_ms >
          kMaximumEdgeGenerationDurationMs) {
    throw std::invalid_argument(
        "edge generation duration must be positive and within the runtime "
        "cooperative deadline limit");
  }
  if (!std::isfinite(request.temperature) || request.temperature < 0.0F ||
      !std::isfinite(request.top_p) || request.top_p <= 0.0F ||
      request.top_p > 1.0F) {
    throw std::invalid_argument(
        "edge generation sampling parameters are invalid");
  }
}

void validate_edge_runtime_operation_result(
    const EdgeRuntimeOperationResult& result) {
  switch (result.status) {
    case EdgeRuntimeStatus::kReady:
    case EdgeRuntimeStatus::kDependencyUnavailable:
    case EdgeRuntimeStatus::kInvalidConfiguration:
    case EdgeRuntimeStatus::kNotLoaded:
    case EdgeRuntimeStatus::kCancelled:
    case EdgeRuntimeStatus::kFailed:
      break;
    default:
      throw std::invalid_argument("edge runtime result has unknown status");
  }
  if (result.ok() && !result.error_category.empty()) {
    throw std::invalid_argument(
        "ready edge runtime result carries an error category");
  }
  if (!result.ok() && result.error_category.empty()) {
    throw std::invalid_argument(
        "non-ready edge runtime result requires an error category");
  }
}

void validate_edge_generation_result(const EdgeGenerationResult& result) {
  validate_edge_runtime_operation_result(result);
  const auto validate_non_negative_metric =
      [](const std::optional<double>& value, const char* name) {
    if (value.has_value() &&
        (!std::isfinite(*value) || *value < 0.0)) {
      throw std::invalid_argument(std::string("edge runtime metric is invalid: ") +
                                  name);
    }
  };
  const auto validate_positive_metric =
      [](const std::optional<double>& value, const char* name) {
    if (value.has_value() &&
        (!std::isfinite(*value) || *value <= 0.0)) {
      throw std::invalid_argument(std::string("edge runtime metric is invalid: ") +
                                  name);
    }
  };
  validate_non_negative_metric(result.metrics.load_time_ms, "load_time_ms");
  validate_non_negative_metric(result.metrics.cold_ttft_ms, "cold_ttft_ms");
  validate_non_negative_metric(result.metrics.warm_ttft_ms, "warm_ttft_ms");
  if (result.metrics.cold_ttft_ms.has_value() &&
      result.metrics.warm_ttft_ms.has_value()) {
    throw std::invalid_argument(
        "one edge generation cannot carry both cold and warm TTFT");
  }
  validate_positive_metric(result.metrics.tpot_ms, "tpot_ms");
  validate_positive_metric(result.metrics.prompt_tokens_per_second,
                           "prompt_tokens_per_second");
  validate_positive_metric(result.metrics.decode_tokens_per_second,
                           "decode_tokens_per_second");
  if ((result.metrics.peak_rss_bytes.has_value() &&
       *result.metrics.peak_rss_bytes == 0U) ||
      (result.metrics.peak_pss_bytes.has_value() &&
       *result.metrics.peak_pss_bytes == 0U)) {
    throw std::invalid_argument(
        "observed edge process working-set metrics must be positive");
  }
  if (result.metrics.provenance.empty()) {
    throw std::invalid_argument(
        "edge generation metrics require nonempty provenance");
  }
  if (result.ok() && result.finish_reason.empty()) {
    throw std::invalid_argument(
        "ready edge generation result requires a finish reason");
  }
  if (result.ok() && result.finish_reason != "length" &&
      result.finish_reason != "end_of_generation" &&
      result.finish_reason != "runtime_stop_unclassified") {
    throw std::invalid_argument(
        "ready edge generation result has an unknown finish reason");
  }
  if (result.status == EdgeRuntimeStatus::kCancelled &&
      result.finish_reason != "cancelled") {
    throw std::invalid_argument(
        "cancelled edge generation result has inconsistent finish reason");
  }
  if (!result.ok() && result.status != EdgeRuntimeStatus::kCancelled &&
      !result.finish_reason.empty()) {
    throw std::invalid_argument(
        "failed edge generation result carries a completion finish reason");
  }
  if (result.generated_tokens == 0U &&
      (result.metrics.cold_ttft_ms.has_value() ||
       result.metrics.warm_ttft_ms.has_value())) {
    throw std::invalid_argument(
        "first-token latency requires an observed generated token");
  }
}

void validate_edge_generation_result_for_request(
    const EdgeGenerationResult& result,
    const EdgeGenerationRequest& request) {
  validate_edge_generation_request(request);
  validate_edge_generation_result(result);
  if (!result.ok() && result.status != EdgeRuntimeStatus::kCancelled) return;

  if (result.prompt_tokens > request.context_tokens ||
      result.generated_tokens > request.maximum_output_tokens ||
      result.generated_tokens > request.context_tokens - result.prompt_tokens) {
    throw std::invalid_argument(
        "edge generation result exceeds request token limits");
  }
  if (result.ok() && result.prompt_tokens == 0U) {
    throw std::invalid_argument(
        "ready edge generation result requires observed prompt tokens");
  }
  if (result.ok() && result.finish_reason == "length" &&
      result.generated_tokens != request.maximum_output_tokens) {
    throw std::invalid_argument(
        "length finish requires the requested output token count");
  }
  if (result.ok() && result.finish_reason == "runtime_stop_unclassified" &&
      result.generated_tokens == 0U) {
    throw std::invalid_argument(
        "unclassified runtime stop requires an observed generated token");
  }
}

}  // namespace inferlab
