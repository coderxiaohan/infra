#include "edge_runtime_matrix_execution.hpp"

#include "edge_process_metrics.hpp"
#include "edge_runtime_matrix_internal.hpp"
#include "edge_runtime_matrix_model.hpp"
#include "inferlab/edge_runtime.hpp"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace inferlab::edge_runtime_matrix {
namespace {

using matrix_detail::append_unique;

bool capability_matches_row(const EdgeRuntimeCapabilities& capabilities,
                            const RuntimeRow& row) {
  const auto* registration = find_edge_runtime_registration(row.runtime_id);
  if (registration == nullptr) return false;
  const auto expected =
      edge_runtime_capabilities_for_current_build(row.runtime_id);
  return capabilities.runtime_id == row.runtime_id &&
         capabilities.runtime_version == row.runtime_version &&
         capabilities.dependency_linked &&
         capabilities.runtime_id == expected.runtime_id &&
         capabilities.runtime_version == expected.runtime_version &&
         capabilities.model_formats == expected.model_formats &&
         capabilities.backends == expected.backends &&
         capabilities.instruction_sets == expected.instruction_sets &&
         capabilities.dependency_linked == expected.dependency_linked &&
         capabilities.supports_streaming == expected.supports_streaming &&
         capabilities.supports_cancel == expected.supports_cancel &&
         capabilities.supports_mmap == expected.supports_mmap &&
         capabilities.supports_explicit_thread_count ==
             expected.supports_explicit_thread_count &&
         std::find(capabilities.model_formats.begin(),
                   capabilities.model_formats.end(),
                   row.model_format) != capabilities.model_formats.end() &&
         std::find(capabilities.backends.begin(), capabilities.backends.end(),
                   row.backend) != capabilities.backends.end();
}

void merge_process_observations(const EdgeRuntimeMetrics& observation,
                                EdgeRuntimeMetrics* primary) {
  if (primary == nullptr) {
    throw std::invalid_argument("edge process metric destination is null");
  }
  const auto merge_max = [](const std::optional<std::uint64_t>& value,
                            std::optional<std::uint64_t>* destination) {
    if (!value.has_value()) return;
    if (!destination->has_value() || *value > **destination) {
      *destination = value;
    }
  };
  merge_max(observation.peak_rss_bytes, &primary->peak_rss_bytes);
  merge_max(observation.peak_pss_bytes, &primary->peak_pss_bytes);
  // Mapping bytes are a point-in-time observation, unlike RSS/PSS maxima.
  // The latest accepted checkpoint is authoritative even when unavailable.
  primary->mapped_model_bytes = observation.mapped_model_bytes;
}

struct GenerationSample {
  EdgeGenerationResult result;
  GeneratedTextCapture generated_text;
  std::size_t callback_count = 0U;
  bool callback_sequence_consistent = true;
};

void observe_generated_token_text(const std::string& token_text,
                                  GeneratedTextCapture* capture) {
  if (!capture->observed_bytes.has_value()) return;
  if (token_text.size() >
      std::numeric_limits<std::size_t>::max() -
          *capture->observed_bytes) {
    capture->observed_bytes.reset();
    capture->text.clear();
    capture->status = "unavailable_generated_text_size_overflow";
    return;
  }
  *capture->observed_bytes += token_text.size();
  if (capture->status == "unavailable_generated_text_limit_exceeded") {
    return;
  }
  if (token_text.size() >
      kMaximumGeneratedTextEvidenceBytes - capture->text.size()) {
    capture->text.clear();
    capture->status = "unavailable_generated_text_limit_exceeded";
    return;
  }
  capture->text.append(token_text);
}

GenerationSample run_generation_sample(
    IEdgeRuntime* runtime, const EdgeGenerationRequest& request,
    const bool capture_generated_text = false) {
  if (runtime == nullptr) {
    throw std::invalid_argument("edge runtime sample requires runtime");
  }
  GenerationSample sample;
  if (capture_generated_text) {
    sample.generated_text.observed_bytes = 0U;
    sample.generated_text.status = "capturing_bounded_callback_text";
  } else {
    sample.generated_text.observed_bytes.reset();
    sample.generated_text.status = "not_captured_performance_probe";
  }
  sample.result = runtime->generate(
      request, [&sample, capture_generated_text](
                   const std::size_t token_index,
                   const std::string& token_text) {
        if (token_index != sample.callback_count) {
          sample.callback_sequence_consistent = false;
        }
        if (capture_generated_text) {
          observe_generated_token_text(token_text, &sample.generated_text);
        }
        ++sample.callback_count;
        return true;
      });
  if (sample.generated_text.status == "capturing_bounded_callback_text") {
    sample.generated_text.status = "captured_bounded_callback_text";
  }
  validate_edge_generation_result_for_request(sample.result, request);
  return sample;
}

bool sample_identity_matches_primary(const GenerationSample& sample,
                                     const RuntimeRow& row) {
  return sample.result.ok() && sample.callback_sequence_consistent &&
         sample.callback_count == sample.result.generated_tokens &&
         sample.result.prompt_tokens == row.observed_prompt_tokens &&
         sample.result.generated_tokens == row.observed_generated_tokens &&
         sample.result.finish_reason == row.finish_reason &&
         sample.result.metrics.provenance == row.metrics.provenance &&
         sample.result.metrics.kv_cache_bytes == row.metrics.kv_cache_bytes &&
         sample.result.metrics.runtime_workspace_bytes ==
             row.metrics.runtime_workspace_bytes;
}

bool sample_matches_primary(const GenerationSample& sample,
                            const RuntimeRow& row) {
  return sample_identity_matches_primary(sample, row) &&
         !sample.result.metrics.cold_ttft_ms.has_value() &&
         (row.observed_generated_tokens == 0U ||
          sample.result.metrics.warm_ttft_ms.has_value());
}

struct OptionalMetricSeries {
  std::vector<double> values;
  std::size_t missing_count = 0U;

  void add(const std::optional<double>& value) {
    if (value.has_value()) {
      values.push_back(*value);
    } else {
      ++missing_count;
    }
  }

  bool presence_consistent() const noexcept {
    return values.empty() || missing_count == 0U;
  }

  std::optional<double> median() const {
    if (values.empty()) return std::nullopt;
    auto sorted = values;
    std::sort(sorted.begin(), sorted.end());
    const std::size_t middle = sorted.size() / 2U;
    if (sorted.size() % 2U != 0U) return sorted[middle];
    return sorted[middle - 1U] / 2.0 + sorted[middle] / 2.0;
  }
};

struct MeasurementSeries {
  OptionalMetricSeries warm_ttft_ms;
  OptionalMetricSeries tpot_ms;
  OptionalMetricSeries prompt_tokens_per_second;
  OptionalMetricSeries decode_tokens_per_second;

  void add(const EdgeRuntimeMetrics& metrics) {
    warm_ttft_ms.add(metrics.warm_ttft_ms);
    tpot_ms.add(metrics.tpot_ms);
    prompt_tokens_per_second.add(metrics.prompt_tokens_per_second);
    decode_tokens_per_second.add(metrics.decode_tokens_per_second);
  }

  bool presence_consistent() const noexcept {
    return warm_ttft_ms.presence_consistent() &&
           tpot_ms.presence_consistent() &&
           prompt_tokens_per_second.presence_consistent() &&
           decode_tokens_per_second.presence_consistent();
  }

  void project(EdgeRuntimeMetrics* metrics) const {
    if (metrics == nullptr) {
      throw std::invalid_argument("edge measurement projection is null");
    }
    metrics->warm_ttft_ms = warm_ttft_ms.median();
    metrics->tpot_ms = tpot_ms.median();
    metrics->prompt_tokens_per_second =
        prompt_tokens_per_second.median();
    metrics->decode_tokens_per_second =
        decode_tokens_per_second.median();
  }
};

double elapsed_milliseconds(const std::chrono::steady_clock::time_point begin,
                            const std::chrono::steady_clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

void mark_protocol_failure(RuntimeRow* row, const std::string& status,
                           const std::string& blocker,
                           const std::string& error_category) {
  row->execution_protocol.status = status;
  if (row->execution_error_category.empty()) {
    row->execution_error_category = error_category;
  }
  append_unique(&row->blockers, blocker);
}

void mark_factory_failure(RuntimeRow* row, const std::string& blocker,
                          const std::string& load_status,
                          const std::string& remaining_lifecycle_status,
                          const std::string& provenance) {
  append_unique(&row->blockers, blocker);
  row->capability_status = "target_machine_execute_failed";
  row->load_status = load_status;
  row->generate_status = remaining_lifecycle_status;
  row->cancel_status = remaining_lifecycle_status;
  row->reset_status = remaining_lifecycle_status;
  row->unload_status = remaining_lifecycle_status;
  row->metrics.provenance = provenance;
  row->execution_protocol.status = "failed_before_measurement";
  row->concurrent_execution.finish_alignment_status =
      "not_observed_primary_runtime_failed";
  row->concurrent_execution.status = "not_run_primary_runtime_failed";
  row->concurrent_execution.provenance = provenance;
}

EdgeModelDescriptor model_descriptor_for_row(
    const std::filesystem::path& resolved_model_path,
    const std::filesystem::path& resolved_tokenizer_path,
    const RuntimeRow& row) {
  EdgeModelDescriptor descriptor;
  descriptor.logical_model_id = row.logical_model_id;
  descriptor.model_format = row.model_format;
  descriptor.quantization = row.quantization;
  descriptor.tokenizer_id = row.tokenizer_id;
  descriptor.checkpoint_revision = row.checkpoint_revision;
  descriptor.conversion_provenance = row.conversion_provenance;
  descriptor.model_path = resolved_model_path;
  descriptor.tokenizer_path = resolved_tokenizer_path;
  descriptor.model_path_label = row.model_path_label;
  descriptor.context_limit = row.context_tokens;
  descriptor.prefer_mmap = row.mmap_requested;
  return descriptor;
}

EdgeGenerationRequest generation_request_for_row(const std::string& prompt,
                                                  const RuntimeRow& row) {
  EdgeGenerationRequest request;
  request.prompt = prompt;
  request.maximum_output_tokens = row.maximum_output_tokens;
  request.context_tokens = row.context_tokens;
  request.batch_size = row.batch_size;
  request.thread_count = row.recommended_thread_count;
  request.maximum_generation_duration_ms =
      row.execution_protocol.maximum_generation_duration_ms;
  request.seed = row.seed;
  request.temperature = row.temperature;
  request.top_p = row.top_p;
  return request;
}

void mark_concurrent_failure(RuntimeRow* row, const std::string& status,
                             const std::string& blocker,
                             const std::string& error_category) {
  row->concurrent_execution.status = status;
  row->concurrent_execution.finish_alignment_status = "not_aligned_failed";
  row->concurrent_execution.provenance =
      "inferlab_cxx17_independent_runtime_instance_start_gate";
  if (row->execution_error_category.empty()) {
    row->execution_error_category = error_category;
  }
  append_unique(&row->blockers, blocker);
}

std::size_t checked_add_session_count(const std::size_t left,
                                      const std::size_t right,
                                      const char* name) {
  if (right > std::numeric_limits<std::size_t>::max() - left) {
    throw std::overflow_error(std::string(name) + " overflow");
  }
  return left + right;
}

bool run_concurrent_execution_probe(
    IEdgeRuntime* primary_runtime,
    const std::filesystem::path& resolved_model_path,
    const std::filesystem::path& resolved_tokenizer_path,
    const std::string& prompt, RuntimeRow* row) {
  if (primary_runtime == nullptr || row == nullptr) {
    throw std::invalid_argument(
        "edge concurrent probe requires runtime and row");
  }
  if (row->concurrent_sessions == 0U ||
      row->concurrent_sessions > kMaximumConcurrentExecuteSessions) {
    throw std::invalid_argument(
        "edge concurrent probe session count exceeds bounded policy");
  }
  auto& observation = row->concurrent_execution;
  if (row->concurrent_sessions == 1U) {
    observation.status = "not_required_single_session";
    observation.finish_alignment_status = "not_applicable_single_session";
    observation.provenance = "serial_execution_protocol";
    return true;
  }

  observation.status = "loading_sessions";
  observation.finish_alignment_status = "not_observed_loading_sessions";
  observation.provenance =
      "inferlab_cxx17_independent_runtime_instance_start_gate";
  const auto descriptor = model_descriptor_for_row(
      resolved_model_path, resolved_tokenizer_path, *row);
  const auto request = generation_request_for_row(prompt, *row);
  std::vector<std::unique_ptr<IEdgeRuntime>> auxiliary_runtimes;
  std::vector<IEdgeRuntime*> sessions;
  auxiliary_runtimes.reserve(row->concurrent_sessions - 1U);
  sessions.reserve(row->concurrent_sessions);
  sessions.push_back(primary_runtime);
  observation.loaded_sessions = 1U;

  for (std::size_t index = 1U; index < row->concurrent_sessions; ++index) {
    auto runtime = create_edge_runtime(row->runtime_id);
    if (!runtime) {
      mark_concurrent_failure(row, "failed_session_factory",
                              "concurrent_runtime_factory_unavailable",
                              "concurrent_runtime_factory_unavailable");
      return false;
    }
    if (!capability_matches_row(runtime->capabilities(), *row)) {
      mark_concurrent_failure(row, "failed_session_capability",
                              "concurrent_runtime_capability_drift",
                              "concurrent_runtime_capability_drift");
      return false;
    }
    const auto load_result = runtime->load(descriptor);
    validate_edge_runtime_operation_result(load_result);
    if (!load_result.ok()) {
      mark_concurrent_failure(
          row, "failed_session_load", "concurrent_runtime_load_failed",
          load_result.error_category.empty()
              ? "concurrent_runtime_load_failed"
              : load_result.error_category);
      return false;
    }
    sessions.push_back(runtime.get());
    auxiliary_runtimes.push_back(std::move(runtime));
    ++observation.loaded_sessions;
  }

  observation.status = "warming_sessions";
  for (auto* runtime : sessions) {
    const auto sample = run_generation_sample(runtime, request);
    const bool latency_present =
        row->observed_generated_tokens == 0U ||
        sample.result.metrics.cold_ttft_ms.has_value() ||
        sample.result.metrics.warm_ttft_ms.has_value();
    if (!sample_identity_matches_primary(sample, *row) ||
        !latency_present) {
      mark_concurrent_failure(row, "failed_session_warmup",
                              "concurrent_runtime_warmup_mismatch",
                              sample.result.ok()
                                  ? "concurrent_runtime_warmup_mismatch"
                                  : sample.result.error_category);
      return false;
    }
    merge_process_observations(sample.result.metrics, &row->metrics);
    const auto reset_result = runtime->reset();
    validate_edge_runtime_operation_result(reset_result);
    if (!reset_result.ok()) {
      mark_concurrent_failure(
          row, "failed_session_warmup_reset",
          "concurrent_runtime_warmup_reset_failed",
          reset_result.error_category.empty()
              ? "concurrent_runtime_warmup_reset_failed"
              : reset_result.error_category);
      return false;
    }
    ++observation.warmed_sessions;
  }

  struct StartGate {
    std::mutex mutex;
    std::condition_variable condition;
    std::size_t ready_count = 0U;
    std::size_t started_count = 0U;
    std::size_t completion_count = 0U;
    bool released = false;
    bool aborted = false;
  } gate;
  std::vector<GenerationSample> samples(sessions.size());
  std::vector<std::exception_ptr> failures(sessions.size());
  std::vector<std::size_t> completion_order(sessions.size(), 0U);
  std::vector<std::thread> workers;
  workers.reserve(sessions.size());
  try {
    for (std::size_t index = 0U; index < sessions.size(); ++index) {
      workers.emplace_back([&, index] {
        {
          std::unique_lock<std::mutex> lock(gate.mutex);
          ++gate.ready_count;
          gate.condition.notify_all();
          gate.condition.wait(lock, [&gate] { return gate.released; });
          if (gate.aborted) return;
          ++gate.started_count;
        }
        try {
          samples[index] = run_generation_sample(sessions[index], request);
        } catch (...) {
          failures[index] = std::current_exception();
        }
        {
          std::lock_guard<std::mutex> lock(gate.mutex);
          completion_order[index] = ++gate.completion_count;
        }
      });
    }
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(gate.mutex);
      gate.aborted = true;
      gate.released = true;
    }
    gate.condition.notify_all();
    for (auto& worker : workers) {
      if (worker.joinable()) worker.join();
    }
    mark_concurrent_failure(row, "failed_thread_start",
                            "concurrent_thread_start_failed",
                            "concurrent_thread_start_failed");
    return false;
  }

  std::chrono::steady_clock::time_point concurrent_begin;
  {
    std::unique_lock<std::mutex> lock(gate.mutex);
    gate.condition.wait(lock, [&gate, &sessions] {
      return gate.ready_count == sessions.size();
    });
    concurrent_begin = std::chrono::steady_clock::now();
    gate.released = true;
  }
  gate.condition.notify_all();
  for (auto& worker : workers) worker.join();
  {
    std::lock_guard<std::mutex> lock(gate.mutex);
    observation.started_sessions = gate.started_count;
  }
  const auto concurrent_end = std::chrono::steady_clock::now();
  observation.wall_time_ms =
      elapsed_milliseconds(concurrent_begin, concurrent_end);

  MeasurementSeries session_metrics;
  for (std::size_t index = 0U; index < sessions.size(); ++index) {
    if (failures[index] || !sample_matches_primary(samples[index], *row)) {
      const std::string error_category =
          failures[index]
              ? "concurrent_runtime_generation_exception"
              : (samples[index].result.error_category.empty()
                     ? "concurrent_runtime_generation_mismatch"
                     : samples[index].result.error_category);
      mark_concurrent_failure(row, "failed_session_generation",
                              "concurrent_runtime_generation_mismatch",
                              error_category);
      return false;
    }
    session_metrics.add(samples[index].result.metrics);
    observation.total_prompt_tokens = checked_add_session_count(
        observation.total_prompt_tokens,
        samples[index].result.prompt_tokens,
        "concurrent prompt token total");
    observation.total_generated_tokens = checked_add_session_count(
        observation.total_generated_tokens,
        samples[index].result.generated_tokens,
        "concurrent generated token total");
    observation.total_callback_count = checked_add_session_count(
        observation.total_callback_count, samples[index].callback_count,
        "concurrent callback total");
    const auto reset_result = sessions[index]->reset();
    validate_edge_runtime_operation_result(reset_result);
    if (!reset_result.ok()) {
      mark_concurrent_failure(
          row, "failed_session_reset", "concurrent_runtime_reset_failed",
          reset_result.error_category.empty()
              ? "concurrent_runtime_reset_failed"
              : reset_result.error_category);
      return false;
    }
    ++observation.completed_sessions;
  }
  std::vector<std::size_t> completion_indices;
  completion_indices.reserve(sessions.size());
  for (std::size_t index = 0U; index < sessions.size(); ++index) {
    completion_indices.push_back(index);
  }
  std::sort(completion_indices.begin(), completion_indices.end(),
            [&completion_order](const std::size_t left,
                                const std::size_t right) {
              return completion_order[left] < completion_order[right];
            });
  EdgeRuntimeMetrics concurrent_process_metrics;
  for (const auto index : completion_indices) {
    merge_process_observations(samples[index].result.metrics,
                               &concurrent_process_metrics);
    merge_process_observations(samples[index].result.metrics, &row->metrics);
  }
  observation.peak_rss_bytes = concurrent_process_metrics.peak_rss_bytes;
  observation.peak_pss_bytes = concurrent_process_metrics.peak_pss_bytes;
  observation.mapped_model_bytes =
      concurrent_process_metrics.mapped_model_bytes;
  if (!session_metrics.presence_consistent() ||
      !observation.wall_time_ms.has_value() ||
      *observation.wall_time_ms <= 0.0) {
    mark_concurrent_failure(row, "failed_session_metric_projection",
                            "concurrent_runtime_metric_series_inconsistent",
                            "concurrent_runtime_metric_series_inconsistent");
    return false;
  }
  observation.median_warm_ttft_ms = session_metrics.warm_ttft_ms.median();
  observation.median_tpot_ms = session_metrics.tpot_ms.median();
  if (observation.total_generated_tokens > 0U) {
    observation.aggregate_generated_tokens_per_second =
        static_cast<double>(observation.total_generated_tokens) * 1000.0 /
        *observation.wall_time_ms;
  }
  observation.finish_alignment_status =
      "aligned_with_primary_serial_observation";
  observation.status = "observed";
  for (auto& runtime : auxiliary_runtimes) runtime->unload();
  return true;
}

void apply_generation_observation(const EdgeGenerationResult& generation,
                                  const bool callback_sequence_consistent,
                                  RuntimeRow* row) {
  row->generate_status = edge_runtime_status_name(generation.status);
  row->observed_prompt_tokens = generation.prompt_tokens;
  row->observed_generated_tokens = generation.generated_tokens;
  row->finish_reason = generation.finish_reason;
  row->metrics = generation.metrics;
  row->cancel_status = generation.status == EdgeRuntimeStatus::kCancelled
                           ? "observed_cancelled"
                           : "not_invoked_normal_completion";
  if (!generation.ok()) {
    row->execution_error_category = generation.error_category;
    append_unique(&row->blockers, "runtime_generate_failed");
  }
  if (generation.ok() &&
      generation.finish_reason == "runtime_stop_unclassified") {
    append_unique(&row->blockers, "runtime_stop_reason_unclassified");
  }
  if (generation.ok() &&
      (!callback_sequence_consistent ||
       row->token_callback_count != row->observed_generated_tokens)) {
    row->execution_error_category = "token_callback_count_mismatch";
    append_unique(&row->blockers, "token_callback_count_mismatch");
  }
}

void apply_thermal_observation(const EdgeThermalSnapshot& observation,
                               const bool before, RuntimeRow* row) {
  if (before) {
    row->thermal_before_c = observation.maximum_temperature_c;
    row->thermal_sensor_count_before = observation.readable_sensor_count;
    row->thermal_before_status = observation.status;
  } else {
    row->thermal_after_c = observation.maximum_temperature_c;
    row->thermal_sensor_count_after = observation.readable_sensor_count;
    row->thermal_after_status = observation.status;
  }
  if (row->thermal_provenance.empty() ||
      row->thermal_provenance.rfind("unavailable_", 0U) == 0U) {
    row->thermal_provenance = observation.provenance;
  } else if (row->thermal_provenance != observation.provenance) {
    row->thermal_before_c.reset();
    row->thermal_after_c.reset();
    row->thermal_sensor_count_before = 0U;
    row->thermal_sensor_count_after = 0U;
    row->thermal_before_status = "unavailable_mixed_thermal_provenance";
    row->thermal_after_status = "unavailable_mixed_thermal_provenance";
    row->thermal_provenance = "unavailable_mixed_thermal_provenance";
  }
}

void apply_energy_observation(
    const EdgeEnergyIntervalObservation& observation, RuntimeRow* row) {
  if (row == nullptr) return;
  row->observed_energy_uj = observation.consumed_energy_uj;
  row->energy_root_zone_count = observation.root_zone_count;
  row->energy_before_status = observation.before_status;
  row->energy_after_status = observation.after_status;
  row->energy_status = observation.status;
  row->energy_provenance = observation.provenance;
}

}  // namespace

void execute_runtime_row(
    const std::filesystem::path& resolved_model_path,
    const std::filesystem::path& resolved_tokenizer_path,
    const std::string& prompt, GeneratedTextCapture* generated_text,
    RuntimeRow* row) {
  if (generated_text == nullptr || row == nullptr) {
    throw std::invalid_argument(
        "edge runtime execute row and text capture must not be null");
  }
  *generated_text = {};
  generated_text->observed_bytes.reset();
  generated_text->status = "not_observed_execute_not_started";
  auto runtime = create_edge_runtime(row->runtime_id);
  if (!runtime) {
    mark_factory_failure(row, "runtime_factory_unavailable",
                         "factory_unavailable", "not_run_factory_unavailable",
                         "unavailable_runtime_factory_missing");
    generated_text->status = "unavailable_runtime_factory_missing";
    return;
  }
  const auto runtime_capabilities = runtime->capabilities();
  if (!capability_matches_row(runtime_capabilities, *row)) {
    mark_factory_failure(row, "runtime_factory_capability_drift",
                         "not_run_capability_drift",
                         "not_run_capability_drift",
                         "unavailable_runtime_capability_drift");
    generated_text->status = "unavailable_runtime_capability_drift";
    return;
  }
  row->thread_control_status =
      runtime_capabilities.supports_explicit_thread_count
          ? "explicit_request_supported_execute_pending"
          : "runtime_managed_not_explicitly_controlled";

  const auto energy_before = edge_energy_snapshot();
  const auto load_result = runtime->load(model_descriptor_for_row(
      resolved_model_path, resolved_tokenizer_path, *row));
  validate_edge_runtime_operation_result(load_result);
  row->load_status = edge_runtime_status_name(load_result.status);
  if (!load_result.ok()) {
    row->execution_error_category = load_result.error_category;
    append_unique(&row->blockers, "runtime_load_failed");
    row->capability_status = "target_machine_execute_failed";
    runtime->unload();
    row->generate_status = "not_run_load_failed";
    row->cancel_status = "not_run_load_failed";
    row->reset_status = "not_run_load_failed";
    row->unload_status = "completed_after_load_failure";
    row->metrics.provenance = "unavailable_runtime_load_failed";
    row->execution_protocol.status = "failed_before_measurement";
    row->concurrent_execution.finish_alignment_status =
        "not_observed_primary_runtime_failed";
    row->concurrent_execution.status = "not_run_primary_runtime_failed";
    row->concurrent_execution.provenance =
        "unavailable_runtime_load_failed";
    generated_text->status = "unavailable_runtime_load_failed";
    apply_energy_observation(
        edge_energy_interval_observation(energy_before,
                                         edge_energy_snapshot()),
        row);
    return;
  }

  apply_thermal_observation(edge_thermal_snapshot(), true, row);

  if (runtime_capabilities.supports_explicit_thread_count) {
    row->thread_control_status = "explicit_request_applied_to_generate_call";
  }
  const auto generation_request = generation_request_for_row(prompt, *row);
  row->execution_protocol.status = "cold_probe_running";
  const auto primary_sample =
      run_generation_sample(runtime.get(), generation_request, true);
  *generated_text = primary_sample.generated_text;
  row->token_callback_count = primary_sample.callback_count;
  apply_generation_observation(primary_sample.result,
                               primary_sample.callback_sequence_consistent,
                               row);

  const auto reset_result = runtime->reset();
  validate_edge_runtime_operation_result(reset_result);
  row->reset_status = edge_runtime_status_name(reset_result.status);
  if (!reset_result.ok()) {
    if (row->execution_error_category.empty()) {
      row->execution_error_category = reset_result.error_category;
    }
    append_unique(&row->blockers, "runtime_reset_failed");
  }

  const bool primary_callback_consistent =
      primary_sample.callback_sequence_consistent &&
      row->token_callback_count == row->observed_generated_tokens;
  const bool primary_latency_consistent =
      row->observed_generated_tokens == 0U ||
      primary_sample.result.metrics.cold_ttft_ms.has_value();
  bool protocol_ok = primary_sample.result.ok() && reset_result.ok() &&
                     primary_callback_consistent &&
                     primary_latency_consistent;
  if (!protocol_ok) {
    mark_protocol_failure(row, "failed_cold_probe",
                          "runtime_cold_probe_failed",
                          "runtime_cold_probe_failed");
  }

  const auto reset_after_sample = [&runtime, row]() {
    const auto result = runtime->reset();
    validate_edge_runtime_operation_result(result);
    if (!result.ok()) {
      row->reset_status = edge_runtime_status_name(result.status);
      if (row->execution_error_category.empty()) {
        row->execution_error_category = result.error_category;
      }
      return false;
    }
    row->reset_status = "ready_between_measurement_iterations";
    return true;
  };

  if (protocol_ok) {
    row->execution_protocol.status = "warmup_running";
    for (std::size_t index = 0U;
         index < row->execution_protocol.warmup_iterations; ++index) {
      const auto sample =
          run_generation_sample(runtime.get(), generation_request);
      if (!sample_matches_primary(sample, *row)) {
        mark_protocol_failure(row, "failed_warmup",
                              "runtime_warmup_observation_mismatch",
                              sample.result.ok()
                                  ? "runtime_warmup_observation_mismatch"
                                  : sample.result.error_category);
        protocol_ok = false;
        break;
      }
      merge_process_observations(sample.result.metrics, &row->metrics);
      if (!reset_after_sample()) {
        mark_protocol_failure(row, "failed_warmup",
                              "runtime_warmup_reset_failed",
                              "runtime_warmup_reset_failed");
        protocol_ok = false;
        break;
      }
      ++row->execution_protocol.completed_warmup_iterations;
    }
  }

  MeasurementSeries measurement_series;
  if (protocol_ok) {
    row->execution_protocol.status = "measurement_running";
    const auto measurement_begin = std::chrono::steady_clock::now();
    while (row->execution_protocol.completed_measurement_iterations <
           row->execution_protocol.maximum_measurement_iterations) {
      const auto now = std::chrono::steady_clock::now();
      const double elapsed_ms = elapsed_milliseconds(measurement_begin, now);
      if (row->execution_protocol.completed_measurement_iterations >=
              row->execution_protocol.minimum_measurement_iterations &&
          elapsed_ms >= static_cast<double>(
                            row->execution_protocol
                                .minimum_sustained_duration_ms)) {
        break;
      }
      const auto sample =
          run_generation_sample(runtime.get(), generation_request);
      if (!sample_matches_primary(sample, *row)) {
        mark_protocol_failure(row, "failed_measurement",
                              "runtime_measurement_observation_mismatch",
                              sample.result.ok()
                                  ? "runtime_measurement_observation_mismatch"
                                  : sample.result.error_category);
        protocol_ok = false;
        break;
      }
      measurement_series.add(sample.result.metrics);
      merge_process_observations(sample.result.metrics, &row->metrics);
      if (!reset_after_sample()) {
        mark_protocol_failure(row, "failed_measurement",
                              "runtime_measurement_reset_failed",
                              "runtime_measurement_reset_failed");
        protocol_ok = false;
        break;
      }
      ++row->execution_protocol.completed_measurement_iterations;
    }
    row->execution_protocol.observed_measurement_duration_ms =
        elapsed_milliseconds(measurement_begin,
                             std::chrono::steady_clock::now());
    if (protocol_ok &&
        (!measurement_series.presence_consistent() ||
         row->execution_protocol.completed_measurement_iterations <
             row->execution_protocol.minimum_measurement_iterations)) {
      mark_protocol_failure(row, "failed_measurement",
                            "runtime_measurement_series_inconsistent",
                            "runtime_measurement_series_inconsistent");
      protocol_ok = false;
    }
    if (protocol_ok &&
        *row->execution_protocol.observed_measurement_duration_ms <
            static_cast<double>(row->execution_protocol
                                    .minimum_sustained_duration_ms)) {
      mark_protocol_failure(row, "failed_sustained_duration",
                            "runtime_sustained_duration_not_met",
                            "runtime_sustained_duration_not_met");
      protocol_ok = false;
    }
    if (protocol_ok) {
      measurement_series.project(&row->metrics);
      row->execution_protocol.status = "observed";
    }
  }

  bool concurrent_probe_ok = false;
  if (protocol_ok) {
    concurrent_probe_ok = run_concurrent_execution_probe(
        runtime.get(), resolved_model_path, resolved_tokenizer_path, prompt,
        row);
  } else {
    row->concurrent_execution.finish_alignment_status =
        "not_observed_serial_protocol_failed";
    row->concurrent_execution.status = "not_run_serial_protocol_failed";
    row->concurrent_execution.provenance =
        "unavailable_serial_protocol_failed";
  }

  bool cancel_probe_ok = false;
  bool cancel_reset_ok = false;
  if (protocol_ok && concurrent_probe_ok &&
      runtime_capabilities.supports_cancel) {
    auto cancel_request = generation_request_for_row(prompt, *row);
    cancel_request.maximum_output_tokens = 1U;
    std::size_t cancel_callback_count = 0U;
    bool cancel_callback_sequence_consistent = true;
    bool cancel_invoked = false;
    const auto cancellation = runtime->generate(
        cancel_request,
        [&runtime, &cancel_callback_count,
         &cancel_callback_sequence_consistent,
         &cancel_invoked](const std::size_t token_index,
                          const std::string&) {
          if (token_index != cancel_callback_count) {
            cancel_callback_sequence_consistent = false;
          }
          ++cancel_callback_count;
          if (!cancel_invoked) {
            runtime->cancel();
            cancel_invoked = true;
          }
          return true;
        });
    validate_edge_generation_result_for_request(cancellation, cancel_request);
    const bool cancel_provenance_consistent =
        cancellation.metrics.provenance == row->metrics.provenance;
    cancel_probe_ok =
        cancel_invoked && cancel_callback_sequence_consistent &&
        cancel_callback_count > 0U &&
        cancellation.status == EdgeRuntimeStatus::kCancelled &&
        cancellation.generated_tokens == cancel_callback_count &&
        cancel_provenance_consistent;
    if (cancel_probe_ok) {
      // The adapter timestamps the first token before the callback invokes
      // cancel; expose all later observations only after the public-cancel
      // contract and metric provenance reconcile.
      merge_process_observations(cancellation.metrics, &row->metrics);
    }
    row->cancel_status = cancel_probe_ok
                             ? "observed_public_cancel_during_generation"
                             : "cancel_probe_failed";
    if (!cancel_probe_ok) {
      if (row->execution_error_category.empty()) {
        row->execution_error_category =
            cancel_provenance_consistent
                ? "runtime_cancel_probe_failed"
                : "runtime_cancel_probe_provenance_mismatch";
      }
      append_unique(&row->blockers, "runtime_cancel_probe_failed");
    }
    const auto cancel_reset_result = runtime->reset();
    validate_edge_runtime_operation_result(cancel_reset_result);
    cancel_reset_ok = cancel_reset_result.ok();
    if (!cancel_reset_ok) {
      if (row->execution_error_category.empty()) {
        row->execution_error_category = cancel_reset_result.error_category;
      }
      append_unique(&row->blockers, "runtime_post_cancel_reset_failed");
    } else {
      row->reset_status = "ready_after_generation_and_cancel_probe";
    }
  } else if (!runtime_capabilities.supports_cancel) {
    row->cancel_status = "runtime_cancel_capability_missing";
    append_unique(&row->blockers, "runtime_cancel_capability_missing");
  } else {
    row->cancel_status = concurrent_probe_ok
                             ? "not_run_execution_protocol_failed"
                             : "not_run_concurrent_protocol_failed";
  }
  apply_thermal_observation(edge_thermal_snapshot(), false, row);
  runtime->unload();
  apply_energy_observation(
      edge_energy_interval_observation(energy_before, edge_energy_snapshot()),
      row);
  row->unload_status = "completed";
  row->executed = protocol_ok && concurrent_probe_ok && cancel_probe_ok &&
                  cancel_reset_ok;
  row->capability_status = row->executed
                               ? "target_machine_execute_observed"
                               : "target_machine_execute_failed";
  if (row->executed &&
      generated_text->status == "captured_bounded_callback_text") {
    generated_text->status = "observed_bounded_callback_text";
  } else if (!row->executed) {
    generated_text->text.clear();
    generated_text->status = "unavailable_runtime_protocol_failed";
  }
}

}  // namespace inferlab::edge_runtime_matrix
