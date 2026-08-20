#include "edge_runtime_matrix_view.hpp"

#include "plan_json_utils.hpp"

#include <sstream>

namespace inferlab::edge_runtime_matrix {
namespace {

std::string bool_json(const bool value) {
  return value ? "true" : "false";
}

std::string optional_u64_json(
    const std::optional<std::uint64_t>& value) {
  return value.has_value() ? std::to_string(*value) : "null";
}

std::string optional_bool_json(const std::optional<bool>& value) {
  return value.has_value() ? bool_json(*value) : "null";
}

std::string optional_double_json(const std::optional<double>& value) {
  return value.has_value() ? inferlab_plan::fixed6(*value) : "null";
}

std::string kernel_capability_json(const KernelCapability& capability,
                                   const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"operator_id\": "
      << inferlab_plan::json_string(capability.operator_id) << ",\n";
  out << nested << "\"scalar_available\": "
      << bool_json(capability.scalar_available) << ",\n";
  out << nested << "\"neon_compiled\": "
      << bool_json(capability.neon_compiled) << ",\n";
  out << nested << "\"neon_runtime_available\": "
      << bool_json(capability.neon_runtime_available) << ",\n";
  out << nested << "\"selected_backend\": "
      << inferlab_plan::json_string(capability.selected_backend) << ",\n";
  out << nested << "\"selection_status\": "
      << inferlab_plan::json_string(capability.selection_status) << "\n";
  out << pad << "}";
  return out.str();
}

std::string runtime_row_json(const RuntimeRow& row, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"row_id\": "
      << inferlab_plan::json_string(row.row_id) << ",\n";
  out << nested << "\"runtime\": "
      << inferlab_plan::json_string(row.runtime_id) << ",\n";
  out << nested << "\"runtime_version\": "
      << inferlab_plan::json_string(row.runtime_version) << ",\n";
  out << nested << "\"model_format\": "
      << inferlab_plan::json_string(row.model_format) << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(row.backend) << ",\n";
  out << nested << "\"logical_model_id\": "
      << inferlab_plan::json_string(row.logical_model_id) << ",\n";
  out << nested << "\"checkpoint_revision\": "
      << inferlab_plan::json_string(row.checkpoint_revision) << ",\n";
  out << nested << "\"tokenizer_id\": "
      << inferlab_plan::json_string(row.tokenizer_id) << ",\n";
  out << nested << "\"conversion_provenance\": "
      << inferlab_plan::json_string(row.conversion_provenance) << ",\n";
  out << nested << "\"quantization\": "
      << inferlab_plan::json_string(row.quantization) << ",\n";
  out << nested << "\"request_identity\": {\n";
  out << nested << "  \"prompt_fingerprint\": "
      << (row.prompt_fingerprint.empty()
              ? "null"
              : inferlab_plan::json_string(row.prompt_fingerprint))
      << ",\n";
  out << nested << "  \"prompt_bytes\": " << row.prompt_bytes << ",\n";
  out << nested << "  \"seed\": " << row.seed << ",\n";
  out << nested << "  \"temperature\": "
      << inferlab_plan::fixed6(static_cast<double>(row.temperature)) << ",\n";
  out << nested << "  \"top_p\": "
      << inferlab_plan::fixed6(static_cast<double>(row.top_p)) << ",\n";
  out << nested << "  \"prompt_text_emitted\": false\n";
  out << nested << "},\n";
  out << nested << "\"model_path_label\": "
      << inferlab_plan::json_string(row.model_path_label) << ",\n";
  out << nested << "\"model_path_redacted\": true,\n";
  out << nested << "\"model_path_status\": "
      << inferlab_plan::json_string(row.model_path_status) << ",\n";
  out << nested << "\"tokenizer_path_label\": "
      << inferlab_plan::json_string(row.tokenizer_path_label) << ",\n";
  out << nested << "\"tokenizer_path_redacted\": true,\n";
  out << nested << "\"tokenizer_path_status\": "
      << inferlab_plan::json_string(row.tokenizer_path_status) << ",\n";
  out << nested << "\"input_artifact_stability\": {\n";
  out << nested << "  \"model_snapshot_source\": "
      << inferlab_plan::json_string(row.model_input_snapshot_source)
      << ",\n";
  out << nested << "  \"model_status\": "
      << inferlab_plan::json_string(row.model_input_stability_status)
      << ",\n";
  out << nested << "  \"tokenizer_snapshot_source\": "
      << inferlab_plan::json_string(row.tokenizer_input_snapshot_source)
      << ",\n";
  out << nested << "  \"tokenizer_status\": "
      << inferlab_plan::json_string(row.tokenizer_input_stability_status)
      << ",\n";
  out << nested << "  \"metadata_identity_emitted\": false,\n";
  out << nested << "  \"content_hash_verified\": false,\n";
  out << nested
      << "  \"claim_boundary\": \"preflight and execution-window metadata reconciliation only; device/inode/path identity is never emitted and stable metadata is not a content digest, signature or immutable-model proof\"\n";
  out << nested << "},\n";
  out << nested << "\"thread_count\": " << row.thread_count << ",\n";
  out << nested << "\"target_physical_core_count\": "
      << (row.target_physical_core_count == 0U
              ? "null"
              : std::to_string(row.target_physical_core_count))
      << ",\n";
  out << nested << "\"recommended_thread_count\": "
      << row.recommended_thread_count << ",\n";
  out << nested << "\"thread_recommendation_status\": "
      << inferlab_plan::json_string(row.thread_recommendation_status)
      << ",\n";
  out << nested << "\"thread_control_status\": "
      << inferlab_plan::json_string(row.thread_control_status) << ",\n";
  out << nested << "\"context_tokens\": " << row.context_tokens << ",\n";
  out << nested << "\"batch_size\": " << row.batch_size << ",\n";
  out << nested << "\"concurrent_sessions\": "
      << row.concurrent_sessions << ",\n";
  out << nested << "\"maximum_output_tokens\": "
      << row.maximum_output_tokens << ",\n";
  out << nested << "\"execution_protocol\": {\n";
  out << nested << "  \"warmup_iterations\": "
      << row.execution_protocol.warmup_iterations << ",\n";
  out << nested << "  \"minimum_measurement_iterations\": "
      << row.execution_protocol.minimum_measurement_iterations << ",\n";
  out << nested << "  \"minimum_sustained_duration_ms\": "
      << row.execution_protocol.minimum_sustained_duration_ms << ",\n";
  out << nested << "  \"maximum_measurement_iterations\": "
      << row.execution_protocol.maximum_measurement_iterations << ",\n";
  out << nested << "  \"maximum_generation_duration_ms\": "
      << row.execution_protocol.maximum_generation_duration_ms << ",\n";
  out << nested
      << "  \"deadline_semantics\": \"cooperative_cancel_request_no_hard_kill\",\n";
  out << nested << "  \"completed_warmup_iterations\": "
      << row.execution_protocol.completed_warmup_iterations << ",\n";
  out << nested << "  \"completed_measurement_iterations\": "
      << row.execution_protocol.completed_measurement_iterations << ",\n";
  out << nested << "  \"observed_measurement_duration_ms\": "
      << optional_double_json(
             row.execution_protocol.observed_measurement_duration_ms)
      << ",\n";
  out << nested << "  \"status\": "
      << inferlab_plan::json_string(row.execution_protocol.status) << ",\n";
  out << nested << "  \"aggregation\": "
      << inferlab_plan::json_string(row.execution_protocol.aggregation)
      << ",\n";
  out << nested
      << "  \"claim_boundary\": \"serial in-process cold/warm protocol; "
         "warm latency and throughput use medians, process memory uses maxima "
         "and model mapping uses the latest accepted-sample checkpoint "
         "including null when its probe is unavailable; cooperative deadline "
         "requests cancellation but cannot hard-kill a stuck third-party call; not "
         "concurrent-session, power, energy, throttling or production "
         "evidence\"\n";
  out << nested << "},\n";
  out << nested << "\"concurrent_execution\": {\n";
  out << nested << "  \"requested_sessions\": "
      << row.concurrent_execution.requested_sessions << ",\n";
  out << nested << "  \"loaded_sessions\": "
      << row.concurrent_execution.loaded_sessions << ",\n";
  out << nested << "  \"warmed_sessions\": "
      << row.concurrent_execution.warmed_sessions << ",\n";
  out << nested << "  \"started_sessions\": "
      << row.concurrent_execution.started_sessions << ",\n";
  out << nested << "  \"completed_sessions\": "
      << row.concurrent_execution.completed_sessions << ",\n";
  out << nested << "  \"total_prompt_tokens\": "
      << row.concurrent_execution.total_prompt_tokens << ",\n";
  out << nested << "  \"total_generated_tokens\": "
      << row.concurrent_execution.total_generated_tokens << ",\n";
  out << nested << "  \"total_callback_count\": "
      << row.concurrent_execution.total_callback_count << ",\n";
  out << nested << "  \"wall_time_ms\": "
      << optional_double_json(row.concurrent_execution.wall_time_ms)
      << ",\n";
  out << nested << "  \"median_warm_ttft_ms\": "
      << optional_double_json(row.concurrent_execution.median_warm_ttft_ms)
      << ",\n";
  out << nested << "  \"median_tpot_ms\": "
      << optional_double_json(row.concurrent_execution.median_tpot_ms)
      << ",\n";
  out << nested << "  \"aggregate_generated_tokens_per_second\": "
      << optional_double_json(
             row.concurrent_execution.aggregate_generated_tokens_per_second)
      << ",\n";
  out << nested << "  \"peak_rss_bytes\": "
      << optional_u64_json(row.concurrent_execution.peak_rss_bytes) << ",\n";
  out << nested << "  \"peak_pss_bytes\": "
      << optional_u64_json(row.concurrent_execution.peak_pss_bytes) << ",\n";
  out << nested << "  \"mapped_model_bytes\": "
      << optional_u64_json(row.concurrent_execution.mapped_model_bytes)
      << ",\n";
  out << nested << "  \"finish_alignment_status\": "
      << inferlab_plan::json_string(
             row.concurrent_execution.finish_alignment_status)
      << ",\n";
  out << nested << "  \"status\": "
      << inferlab_plan::json_string(row.concurrent_execution.status) << ",\n";
  out << nested << "  \"provenance\": "
      << inferlab_plan::json_string(row.concurrent_execution.provenance)
      << ",\n";
  out << nested
      << "  \"claim_boundary\": \"one-process independent-runtime "
         "instances synchronized for one measured generation window after "
         "normalization warmup; not continuous batching, shared-context "
         "scheduling, multi-process saturation, power, energy or production "
         "evidence\"\n";
  out << nested << "},\n";
  out << nested << "\"resource_plan\": {\n";
  out << nested << "  \"layers\": " << row.layers << ",\n";
  out << nested << "  \"kv_heads\": " << row.kv_heads << ",\n";
  out << nested << "  \"head_dim\": " << row.head_dim << ",\n";
  out << nested << "  \"kv_dtype\": "
      << inferlab_plan::json_string(row.kv_dtype) << ",\n";
  out << nested << "  \"mmap_requested\": "
      << bool_json(row.mmap_requested) << ",\n";
  out << nested << "  \"mmap_strategy\": "
      << inferlab_plan::json_string(row.mmap_strategy) << ",\n";
  out << nested << "  \"runtime_workspace_budget_bytes\": "
      << row.runtime_workspace_budget_bytes << ",\n";
  out << nested << "  \"estimated_runtime_workspace_total_bytes\": "
      << optional_u64_json(
             row.estimated_runtime_workspace_total_bytes)
      << ",\n";
  out << nested << "  \"system_memory_budget_bytes\": "
      << row.system_memory_budget_bytes << ",\n";
  out << nested << "  \"system_reserve_bytes\": "
      << row.system_reserve_bytes << ",\n";
  out << nested << "  \"model_file_bytes\": "
      << optional_u64_json(row.model_file_bytes) << ",\n";
  out << nested << "  \"estimated_model_residency_total_bytes\": "
      << optional_u64_json(row.estimated_model_residency_total_bytes)
      << ",\n";
  out << nested << "  \"estimated_kv_cache_bytes\": "
      << optional_u64_json(row.estimated_kv_cache_bytes) << ",\n";
  out << nested << "  \"estimated_required_bytes\": "
      << optional_u64_json(row.estimated_required_bytes) << ",\n";
  out << nested << "  \"estimated_resource_fit\": "
      << optional_bool_json(row.estimated_resource_fit) << ",\n";
  out << nested << "  \"estimated_headroom_bytes\": "
      << optional_u64_json(row.estimated_headroom_bytes) << ",\n";
  out << nested << "  \"estimated_shortfall_bytes\": "
      << optional_u64_json(row.estimated_shortfall_bytes) << ",\n";
  out << nested << "  \"observed_working_set_bytes\": "
      << optional_u64_json(row.observed_working_set_bytes) << ",\n";
  out << nested << "  \"observed_working_set_source\": "
      << (row.observed_working_set_source.empty()
              ? "null"
              : inferlab_plan::json_string(row.observed_working_set_source))
      << ",\n";
  out << nested << "  \"observed_resource_fit\": "
      << optional_bool_json(row.observed_resource_fit) << ",\n";
  out << nested << "  \"observed_headroom_bytes\": "
      << optional_u64_json(row.observed_headroom_bytes) << ",\n";
  out << nested << "  \"observed_shortfall_bytes\": "
      << optional_u64_json(row.observed_shortfall_bytes) << ",\n";
  out << nested << "  \"observed_status\": "
      << inferlab_plan::json_string(row.observed_resource_status) << ",\n";
  out << nested << "  \"status\": "
      << inferlab_plan::json_string(row.resource_plan_status) << ",\n";
  out << nested << "  \"claim_boundary\": \"analytical mmap-shareable or session-scaled buffered model residency/session-scaled KV/per-session workspace/reserve estimate; not observed RSS, PSS, page residency, allocator sharing or OOM evidence\"\n";
  out << nested << "},\n";
  out << nested << "\"build_option_enabled\": "
      << bool_json(row.build_option_enabled) << ",\n";
  out << nested << "\"dependency_linked\": "
      << bool_json(row.dependency_linked) << ",\n";
  out << nested << "\"adapter_implemented\": "
      << bool_json(row.adapter_implemented) << ",\n";
  out << nested << "\"executed\": " << bool_json(row.executed) << ",\n";
  out << nested << "\"execution_observation\": {\n";
  out << nested << "  \"prompt_tokens\": "
      << row.observed_prompt_tokens << ",\n";
  out << nested << "  \"generated_tokens\": "
      << row.observed_generated_tokens << ",\n";
  out << nested << "  \"token_callback_count\": "
      << row.token_callback_count << ",\n";
  out << nested << "  \"finish_reason\": "
      << (row.finish_reason.empty()
              ? "null"
              : inferlab_plan::json_string(row.finish_reason))
      << ",\n";
  out << nested << "  \"error_category\": "
      << (row.execution_error_category.empty()
              ? "null"
              : inferlab_plan::json_string(row.execution_error_category))
      << "\n";
  out << nested << "},\n";
  out << nested << "\"capability_status\": "
      << inferlab_plan::json_string(row.capability_status) << ",\n";
  out << nested << "\"lifecycle\": {\n";
  out << nested << "  \"load\": "
      << inferlab_plan::json_string(row.load_status) << ",\n";
  out << nested << "  \"generate\": "
      << inferlab_plan::json_string(row.generate_status) << ",\n";
  out << nested << "  \"cancel\": "
      << inferlab_plan::json_string(row.cancel_status) << ",\n";
  out << nested << "  \"reset\": "
      << inferlab_plan::json_string(row.reset_status) << ",\n";
  out << nested << "  \"unload\": "
      << inferlab_plan::json_string(row.unload_status) << "\n";
  out << nested << "},\n";
  out << nested << "\"metrics\": {\n";
  out << nested << "  \"load_time_ms\": "
      << optional_double_json(row.metrics.load_time_ms) << ",\n";
  out << nested << "  \"cold_ttft_ms\": "
      << optional_double_json(row.metrics.cold_ttft_ms) << ",\n";
  out << nested << "  \"warm_ttft_ms\": "
      << optional_double_json(row.metrics.warm_ttft_ms) << ",\n";
  out << nested << "  \"tpot_ms\": "
      << optional_double_json(row.metrics.tpot_ms) << ",\n";
  out << nested << "  \"prompt_tokens_per_second\": "
      << optional_double_json(row.metrics.prompt_tokens_per_second) << ",\n";
  out << nested << "  \"decode_tokens_per_second\": "
      << optional_double_json(row.metrics.decode_tokens_per_second) << ",\n";
  out << nested << "  \"peak_rss_bytes\": "
      << optional_u64_json(row.metrics.peak_rss_bytes) << ",\n";
  out << nested << "  \"peak_pss_bytes\": "
      << optional_u64_json(row.metrics.peak_pss_bytes) << ",\n";
  out << nested << "  \"mapped_model_bytes\": "
      << optional_u64_json(row.metrics.mapped_model_bytes) << ",\n";
  out << nested << "  \"kv_cache_bytes\": "
      << optional_u64_json(row.metrics.kv_cache_bytes) << ",\n";
  out << nested << "  \"runtime_workspace_bytes\": "
      << optional_u64_json(row.metrics.runtime_workspace_bytes) << ",\n";
  out << nested << "  \"provenance\": "
      << inferlab_plan::json_string(row.metrics.provenance) << "\n";
  out << nested << "},\n";
  out << nested << "\"thermal_observation\": {\n";
  out << nested << "  \"maximum_temperature_before_c\": "
      << optional_double_json(row.thermal_before_c) << ",\n";
  out << nested << "  \"maximum_temperature_after_c\": "
      << optional_double_json(row.thermal_after_c) << ",\n";
  out << nested << "  \"readable_sensor_count_before\": "
      << row.thermal_sensor_count_before << ",\n";
  out << nested << "  \"readable_sensor_count_after\": "
      << row.thermal_sensor_count_after << ",\n";
  out << nested << "  \"before_status\": "
      << inferlab_plan::json_string(row.thermal_before_status) << ",\n";
  out << nested << "  \"after_status\": "
      << inferlab_plan::json_string(row.thermal_after_status) << ",\n";
  out << nested << "  \"provenance\": "
      << inferlab_plan::json_string(row.thermal_provenance) << ",\n";
  out << nested
      << "  \"claim_boundary\": \"point-in-time maximum readable Linux "
         "thermal-zone temperature before and after the runtime lifecycle; "
         "not a throttling, sustained-performance, power, energy or "
         "device-fitness conclusion\"\n";
  out << nested << "},\n";
  out << nested << "\"energy_observation\": {\n";
  out << nested << "  \"consumed_energy_uj\": "
      << optional_u64_json(row.observed_energy_uj) << ",\n";
  out << nested << "  \"root_zone_count\": "
      << row.energy_root_zone_count << ",\n";
  out << nested << "  \"before_status\": "
      << inferlab_plan::json_string(row.energy_before_status) << ",\n";
  out << nested << "  \"after_status\": "
      << inferlab_plan::json_string(row.energy_after_status) << ",\n";
  out << nested << "  \"status\": "
      << inferlab_plan::json_string(row.energy_status) << ",\n";
  out << nested << "  \"provenance\": "
      << inferlab_plan::json_string(row.energy_provenance) << ",\n";
  out << nested
      << "  \"wrap_policy\": \"monotonic_counter_and_time_power_upper_"
         "bound_below_first_wrap_delta\",\n";
  out << nested
      << "  \"claim_boundary\": \"checked aggregate of Linux powercap "
         "root-zone energy-counter deltas from immediately before runtime "
         "load through unload; whole-system interval observation, not "
         "process-, model- or inference-exclusive energy, instantaneous "
         "power, battery energy or production efficiency evidence\"\n";
  out << nested << "},\n";
  out << nested << "\"blockers\": "
      << inferlab_plan::json_string_array(row.blockers, indent + 2) << "\n";
  out << pad << "}";
  return out.str();
}

template <typename Value, typename Renderer>
std::string array_json(const std::vector<Value>& values, const int indent,
                       const Renderer& renderer) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << "[";
  if (!values.empty()) out << "\n";
  for (std::size_t index = 0U; index < values.size(); ++index) {
    out << renderer(values[index], indent + 2);
    out << (index + 1U == values.size() ? "\n" : ",\n");
  }
  if (!values.empty()) out << pad;
  out << "]";
  return out.str();
}

}  // namespace

std::string artifact_json(const Matrix& matrix) {
  validate_matrix(matrix);
  const bool executed = !matrix.runtime_rows.empty() &&
                        matrix.summary.executed_row_count ==
                            matrix.summary.runtime_row_count;
  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_edge_runtime_matrix\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": "
      << inferlab_plan::json_string(mode_name(matrix.mode)) << ",\n";
  out << "  \"executed\": " << bool_json(executed) << ",\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ edge runtime contract, scalar/ARM-NEON capability, checked resource planning, execution-window model/tokenizer metadata stability, linked-runtime readiness, explicit pinned-adapter execute observations, bounded single-request generated-text observation and optional whole-system Linux powercap lifecycle energy delta; plan, blocked, input-drifted, partial or null metrics cannot select a runtime, metadata stability is not a content hash, text match is not model-quality or token/logit/quantization equivalence, and powercap energy is not process/model-exclusive energy, instantaneous power, battery, production or release evidence\",\n";
  out << "  \"generated_at\": "
      << inferlab_plan::json_string(inferlab_plan::current_utc_timestamp())
      << ",\n";
  out << "  \"target_platform\": "
      << inferlab_plan::json_string(matrix.target_platform) << ",\n";
  out << "  \"build_environment\": {\n";
  out << "    \"compiled_system\": "
      << inferlab_plan::json_string(matrix.build_system) << ",\n";
  out << "    \"compiled_architecture\": "
      << inferlab_plan::json_string(matrix.build_architecture) << ",\n";
  out << "    \"target_platform_match_status\": "
      << inferlab_plan::json_string(matrix.target_platform_match_status)
      << "\n";
  out << "  },\n";
  out << "  \"config\": {\n";
  out << "    \"path_label\": "
      << inferlab_plan::json_string(matrix.config_path_label) << ",\n";
  out << "    \"path_redacted\": true,\n";
  out << "    \"schema_version\": "
      << (matrix.config_schema_version == 0U
              ? "null"
              : std::to_string(matrix.config_schema_version))
      << ",\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(matrix.config_status) << "\n";
  out << "  },\n";
  out << "  \"isa_capabilities\": {\n";
  out << "    \"aarch64_build\": "
      << bool_json(matrix.isa_capabilities.aarch64_build) << ",\n";
  out << "    \"neon_compiled\": "
      << bool_json(matrix.isa_capabilities.neon_compiled) << ",\n";
  out << "    \"neon_runtime_available\": "
      << bool_json(matrix.isa_capabilities.neon_runtime_available) << ",\n";
  out << "    \"sve_requested\": "
      << bool_json(matrix.isa_capabilities.sve_requested) << ",\n";
  out << "    \"sve_compiled\": "
      << bool_json(matrix.isa_capabilities.sve_compiled) << ",\n";
  out << "    \"sve_runtime_available\": "
      << bool_json(matrix.isa_capabilities.sve_runtime_available) << ",\n";
  out << "    \"runtime_probe_status\": "
      << inferlab_plan::json_string(
             matrix.isa_capabilities.runtime_probe_status)
      << "\n";
  out << "  },\n";
  out << "  \"kernel_capabilities\": "
      << array_json(matrix.kernel_capabilities, 2, kernel_capability_json)
      << ",\n";
  out << "  \"dependency_status\": "
      << array_json(matrix.runtime_rows, 2,
                    [](const RuntimeRow& row, const int indent) {
                      const std::string pad = inferlab_plan::json_indent(indent);
                      const std::string nested =
                          inferlab_plan::json_indent(indent + 2);
                      std::ostringstream dependency;
                      dependency << pad << "{\n";
                      dependency << nested << "\"runtime\": "
                                 << inferlab_plan::json_string(row.runtime_id)
                                 << ",\n";
                      dependency << nested << "\"build_option_enabled\": "
                                 << bool_json(row.build_option_enabled) << ",\n";
                      dependency << nested << "\"dependency_linked\": "
                                 << bool_json(row.dependency_linked) << ",\n";
                      dependency << nested << "\"adapter_implemented\": "
                                 << bool_json(row.adapter_implemented) << "\n";
                      dependency << pad << "}";
                      return dependency.str();
                    })
      << ",\n";
  out << "  \"runtime_rows\": "
      << array_json(matrix.runtime_rows, 2, runtime_row_json) << ",\n";
  out << "  \"comparison_identity\": {\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(matrix.comparison_identity_status)
      << ",\n";
  out << "    \"gaps\": "
      << inferlab_plan::json_string_array(matrix.comparison_identity_gaps, 4)
      << ",\n";
  out << "    \"quality_observation\": {\n";
  out << "      \"status\": "
      << inferlab_plan::json_string(matrix.quality_comparison.status)
      << ",\n";
  out << "      \"reference_row_id\": "
      << (matrix.quality_comparison.reference_row_id.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.quality_comparison.reference_row_id))
      << ",\n";
  out << "      \"observations\": "
      << array_json(
             matrix.quality_comparison.observations, 6,
             [](const GeneratedTextQualityObservation& observation,
                const int indent) {
               const std::string pad = inferlab_plan::json_indent(indent);
               const std::string nested =
                   inferlab_plan::json_indent(indent + 2);
               std::ostringstream value;
               value << pad << "{\n";
               value << nested << "\"row_id\": "
                     << inferlab_plan::json_string(observation.row_id)
                     << ",\n";
               value << nested << "\"generated_text_bytes\": "
                     << (observation.generated_text_bytes.has_value()
                             ? std::to_string(
                                   *observation.generated_text_bytes)
                             : "null")
                     << ",\n";
               value << nested << "\"generated_text_fingerprint\": "
                     << (observation.generated_text_fingerprint.empty()
                             ? "null"
                             : inferlab_plan::json_string(
                                   observation.generated_text_fingerprint))
                     << ",\n";
               value << nested << "\"capture_status\": "
                     << inferlab_plan::json_string(
                            observation.capture_status)
                     << ",\n";
               value << nested << "\"exact_match_with_reference\": "
                     << bool_json(observation.exact_match_with_reference)
                     << ",\n";
               value << nested << "\"first_mismatch_byte_offset\": "
                     << (observation.first_mismatch_byte_offset.has_value()
                             ? std::to_string(
                                   *observation.first_mismatch_byte_offset)
                             : "null")
                     << ",\n";
               value << nested << "\"generated_text_emitted\": false\n";
               value << pad << "}";
               return value.str();
             })
      << ",\n";
  out << "      \"matching_row_ids\": "
      << inferlab_plan::json_string_array(
             matrix.quality_comparison.matching_row_ids, 6)
      << ",\n";
  out << "      \"mismatching_row_ids\": "
      << inferlab_plan::json_string_array(
             matrix.quality_comparison.mismatching_row_ids, 6)
      << ",\n";
  out << "      \"unavailable_row_ids\": "
      << inferlab_plan::json_string_array(
             matrix.quality_comparison.unavailable_row_ids, 6)
      << ",\n";
  out << "      \"criterion\": "
      << inferlab_plan::json_string(matrix.quality_comparison.criterion)
      << ",\n";
  out << "      \"fingerprint_algorithm\": \"fnv1a64_noncryptographic\",\n";
  out << "      \"capture_limit_bytes\": "
      << kMaximumGeneratedTextEvidenceBytes << ",\n";
  out << "      \"matrix_capture_limit_bytes\": "
      << kMaximumGeneratedTextMatrixEvidenceBytes << ",\n";
  out << "      \"claim_boundary\": \"exact match requires nonempty bounded callback text for every row and compares one aligned deterministic request only; unavailable comparison classifies every row, mismatch offset reveals position but not text, and match is not a model-quality, logit, token-id or quantization-equivalence claim\"\n";
  out << "    },\n";
  out << "    \"claim_boundary\": \"aligned logical identity permits bounded quality observation only; it does not imply token, logit, quantization, timing or resource equivalence\"\n";
  out << "  },\n";
  out << "  \"deployment_recommendation\": {\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(
             matrix.deployment_recommendation.status)
      << ",\n";
  out << "    \"candidate_row_ids\": "
      << inferlab_plan::json_string_array(
             matrix.deployment_recommendation.candidate_row_ids, 4)
      << ",\n";
  out << "    \"observation_alignment_status\": "
      << inferlab_plan::json_string(
             matrix.deployment_recommendation.observation_alignment_status)
      << ",\n";
  out << "    \"observation_alignment_gaps\": "
      << inferlab_plan::json_string_array(
             matrix.deployment_recommendation.observation_alignment_gaps, 4)
      << ",\n";
  out << "    \"selected_row_id\": "
      << (matrix.deployment_recommendation.selected_row_id.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.selected_row_id))
      << ",\n";
  out << "    \"runtime\": "
      << (matrix.deployment_recommendation.runtime_id.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.runtime_id))
      << ",\n";
  out << "    \"backend\": "
      << (matrix.deployment_recommendation.backend.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.backend))
      << ",\n";
  out << "    \"model_format\": "
      << (matrix.deployment_recommendation.model_format.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.model_format))
      << ",\n";
  out << "    \"thread_count\": "
      << (matrix.deployment_recommendation.thread_count == 0U
              ? "null"
              : std::to_string(
                    matrix.deployment_recommendation.thread_count))
      << ",\n";
  out << "    \"thread_count_status\": "
      << (matrix.deployment_recommendation.thread_count_status.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.thread_count_status))
      << ",\n";
  out << "    \"mmap_strategy\": "
      << (matrix.deployment_recommendation.mmap_strategy.empty()
              ? "null"
              : inferlab_plan::json_string(
                    matrix.deployment_recommendation.mmap_strategy))
      << ",\n";
  out << "    \"criterion\": "
      << inferlab_plan::json_string(
             matrix.deployment_recommendation.criterion)
      << ",\n";
  out << "    \"claim_boundary\": \"a selected row is an execute-observed performance candidate under the configured workload, not a quality, power, thermal, production or release recommendation\"\n";
  out << "  },\n";
  out << "  \"delivery_contract\": {\n";
  out << "    \"package_root\": "
      << inferlab_plan::json_string(matrix.delivery_contract.package_root)
      << ",\n";
  out << "    \"required_directories\": "
      << inferlab_plan::json_string_array(
             matrix.delivery_contract.required_directories, 4)
      << ",\n";
  out << "    \"manifest_required_fields\": "
      << inferlab_plan::json_string_array(
             matrix.delivery_contract.manifest_required_fields, 4)
      << ",\n";
  out << "    \"status\": "
      << inferlab_plan::json_string(matrix.delivery_contract.status) << ",\n";
  out << "    \"claim_boundary\": \"delivery layout contract only; no package was built or inspected\"\n";
  out << "  },\n";
  out << "  \"summary\": {\n";
  out << "    \"runtime_row_count\": " << matrix.summary.runtime_row_count
      << ",\n";
  out << "    \"dependency_linked_count\": "
      << matrix.summary.dependency_linked_count << ",\n";
  out << "    \"adapter_implemented_count\": "
      << matrix.summary.adapter_implemented_count << ",\n";
  out << "    \"model_present_count\": "
      << matrix.summary.model_present_count << ",\n";
  out << "    \"executed_row_count\": "
      << matrix.summary.executed_row_count << ",\n";
  out << "    \"blocker_count\": " << matrix.summary.blocker_count << ",\n";
  out << "    \"evidence_level\": "
      << inferlab_plan::json_string(matrix.summary.evidence_level) << ",\n";
  out << "    \"decision\": "
      << inferlab_plan::json_string(matrix.summary.decision) << "\n";
  out << "  },\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(matrix.blockers, 2) << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(
             matrix.recommended_next_commands, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::edge_runtime_matrix
