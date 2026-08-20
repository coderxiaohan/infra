#pragma once

#include "paged_kv_probe_json.hpp"
#include "scheduler_probe_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::probe {

inline std::string scheduler_pressure_profile_json(
    const SchedulerPressureProfile& profile, const int indent) {
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"average_active_requests\": " << std::fixed
      << std::setprecision(6) << profile.average_active_requests << ",\n";
  out << nested << "\"average_cache_utilization\": " << std::fixed
      << std::setprecision(6) << profile.average_cache_utilization << ",\n";
  out << nested << "\"average_decode_steps_per_token\": " << std::fixed
      << std::setprecision(6) << profile.average_decode_steps_per_token
      << ",\n";
  out << nested << "\"average_queue_wait_steps\": " << std::fixed
      << std::setprecision(6) << profile.average_queue_wait_steps << ",\n";
  out << nested << "\"average_ttft_steps\": " << std::fixed
      << std::setprecision(6) << profile.average_ttft_steps << ",\n";
  out << nested << "\"average_used_blocks\": " << std::fixed
      << std::setprecision(6) << profile.average_used_blocks << ",\n";
  out << nested << "\"finished_request_count\": "
      << profile.finished_request_count << ",\n";
  out << nested << "\"peak_active_requests\": "
      << profile.peak_active_requests << ",\n";
  out << nested << "\"peak_cache_utilization\": " << std::fixed
      << std::setprecision(6) << profile.peak_cache_utilization << ",\n";
  out << nested << "\"peak_reserved_bytes\": " << profile.peak_reserved_bytes
      << ",\n";
  out << nested << "\"peak_total_tokens\": " << profile.peak_total_tokens
      << ",\n";
  out << nested << "\"peak_used_blocks\": " << profile.peak_used_blocks
      << ",\n";
  out << nested << "\"rejected_request_count\": "
      << profile.rejected_request_count << ",\n";
  out << nested << "\"request_count\": " << profile.request_count << ",\n";
  out << nested << "\"steps\": " << profile.steps << ",\n";
  out << nested << "\"tokens_per_step\": " << std::fixed
      << std::setprecision(6) << profile.tokens_per_step << ",\n";
  out << nested << "\"total_admitted\": " << profile.total_admitted << ",\n";
  out << nested << "\"total_backpressure_deferred\": "
      << profile.total_backpressure_deferred << ",\n";
  out << nested << "\"total_decoded_tokens\": "
      << profile.total_decoded_tokens << ",\n";
  out << nested << "\"total_finished\": " << profile.total_finished << ",\n";
  out << nested << "\"total_rejected\": " << profile.total_rejected << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string request_lifecycle_metrics_json(
    const std::vector<RequestLifecycleMetrics>& requests, const int indent) {
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  const auto fields = probe_json_indent_offset(indent, 4);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < requests.size(); ++i) {
    const auto& request = requests[i];
    out << nested << "{\n";
    out << fields << "\"decode_steps_per_token\": " << std::fixed
        << std::setprecision(6) << request.decode_steps_per_token << ",\n";
    out << fields << "\"finished_step\": " << request.finished_step << ",\n";
    out << fields << "\"first_decode_step\": " << request.first_decode_step
        << ",\n";
    out << fields << "\"first_scheduled_step\": "
        << request.first_scheduled_step << ",\n";
    out << fields << "\"generated_tokens\": " << request.generated_tokens
        << ",\n";
    out << fields << "\"max_decode_tokens\": " << request.max_decode_tokens
        << ",\n";
    out << fields << "\"prompt_tokens\": " << request.prompt_tokens << ",\n";
    out << fields << "\"queue_wait_steps\": " << request.queue_wait_steps
        << ",\n";
    out << fields << "\"request_id\": " << request.request_id << ",\n";
    out << fields << "\"state\": \"" << request_state_name(request.state)
        << "\",\n";
    out << fields << "\"submitted_step\": " << request.submitted_step
        << ",\n";
    out << fields << "\"time_to_first_token_steps\": "
        << request.time_to_first_token_steps << ",\n";
    out << fields << "\"total_latency_steps\": "
        << request.total_latency_steps << "\n";
    out << nested << "}" << (i + 1 == requests.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

inline std::string inference_workload_json(
    const std::vector<InferenceRequest>& workload, const int indent) {
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  const auto fields = probe_json_indent_offset(indent, 4);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t i = 0; i < workload.size(); ++i) {
    const auto& request = workload[i];
    out << nested << "{\n";
    out << fields << "\"max_decode_tokens\": " << request.max_decode_tokens
        << ",\n";
    out << fields << "\"prompt_tokens\": " << request.prompt_tokens << ",\n";
    out << fields << "\"request_id\": " << request.request_id << ",\n";
    out << fields << "\"total_tokens\": "
        << inference_request_total_tokens(request) << "\n";
    out << nested << "}" << (i + 1 == workload.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

inline std::string scheduler_config_json(const PagedKVCacheConfig& config,
                                         const int max_active_requests,
                                         const int indent) {
  require_paged_kv_probe_capacity(config.num_blocks,
                                  "scheduler probe config blocks");
  if (config.block_size_tokens <= 0 || config.bytes_per_token <= 0 ||
      max_active_requests <= 0) {
    throw std::invalid_argument(
        "scheduler probe config dimensions must be positive");
  }
  const auto pad = probe_json_indent(indent);
  const auto nested = probe_json_indent_offset(indent, 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"block_size_tokens\": " << config.block_size_tokens
      << ",\n";
  out << nested << "\"bytes_per_token\": " << config.bytes_per_token
      << ",\n";
  out << nested << "\"max_active_requests\": " << max_active_requests
      << ",\n";
  out << nested << "\"num_blocks\": " << config.num_blocks << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab::probe
