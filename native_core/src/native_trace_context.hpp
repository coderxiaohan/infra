#pragma once

#include "plan_json_utils.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_native_trace {

enum class Domain {
  kv_paged_attention,
  scheduler_batching,
  scheduler_root_cause,
  cuda_quant_microkernel,
  pd_split_crossover,
  kv_aware_routing,
};

struct InputRecord {
  std::size_t source_line_number = 0U;
  std::size_t timestamp_ns = 0U;
  std::size_t span_start_timestamp_ns = 0U;
  std::size_t span_end_timestamp_ns = 0U;
  std::string record_key;
  std::string trace_id;
  std::string span_id;
  std::string parent_span_id;
  std::string causal_predecessor_span_id;
  std::string clock_domain;
  bool parent_span_present = false;
  bool span_lifecycle_supplied = false;
  bool causal_predecessor_span_present = false;
  std::string span_terminal_status = "none";
};

struct Source {
  bool supplied = false;
  bool present = false;
  bool readable = false;
  bool contract_ready = false;
  bool path_redacted = false;
  std::size_t physical_line_count = 0U;
  std::string source_path_label = "not_provided";
  std::string path_probe_status = "not_configured";
  std::string text_read_status = "not_configured";
  std::vector<InputRecord> records;
  inferlab_trace_clock::CalibrationSource clock_calibration;
};

struct RecordProjection {
  std::size_t record_ordinal = 0U;
  std::size_t source_line_number = 0U;
  std::string record_key;
  std::string span_role;
  std::string trace_join_token = "none";
  std::string span_join_token = "none";
  std::string parent_span_join_token = "none";
  std::string causal_predecessor_span_join_token = "none";
  std::string clock_domain = "not_provided";
  std::string clock_calibration_status = "not_applicable";
  std::size_t trace_relative_timestamp_ns = 0U;
  std::size_t clock_uncertainty_ns = 0U;
  std::size_t trace_relative_span_start_ns = 0U;
  std::size_t trace_relative_span_end_ns = 0U;
  std::size_t span_lifecycle_clock_uncertainty_ns = 0U;
  bool context_present = false;
  bool parent_span_present = false;
  bool timestamp_present = false;
  bool relative_timestamp_present = false;
  bool correlation_eligible = false;
  bool span_lifecycle_supplied = false;
  bool span_lifecycle_contract_ready = false;
  bool relative_span_lifecycle_present = false;
  bool causal_predecessor_span_present = false;
  std::string span_terminal_status = "none";
  std::string span_lifecycle_status = "not_provided";
};

struct Summary {
  std::size_t record_count = 0U;
  std::size_t source_record_count = 0U;
  std::size_t context_present_count = 0U;
  std::size_t context_missing_count = 0U;
  std::size_t trace_join_token_count = 0U;
  std::size_t distinct_trace_join_token_count = 0U;
  std::size_t span_join_token_count = 0U;
  std::size_t parent_span_join_token_count = 0U;
  std::size_t clock_domain_count = 0U;
  std::size_t clock_calibrated_record_count = 0U;
  std::size_t clock_uncalibrated_record_count = 0U;
  std::size_t span_lifecycle_supplied_record_count = 0U;
  std::size_t span_lifecycle_contract_ready_record_count = 0U;
  std::size_t span_lifecycle_calibrated_record_count = 0U;
  std::size_t span_lifecycle_uncalibrated_record_count = 0U;
  std::size_t causal_predecessor_span_count = 0U;
  bool source_supplied = false;
  bool source_contract_ready = false;
  bool all_contexts_bound = false;
  bool clock_normalization_performed = false;
  bool span_lifecycle_normalization_performed = false;
  std::string decision = "not_provided";
  std::string span_lifecycle_decision = "not_provided";
};

struct Projection {
  Domain domain = Domain::kv_paged_attention;
  Source source;
  std::vector<RecordProjection> records;
  Summary summary;
  inferlab_trace_clock::CalibrationUsageProjection clock_calibration;
  inferlab_trace_clock::CalibrationUsageProjection
      span_lifecycle_clock_calibration;
  std::string join_token_algorithm;
  std::string claim_boundary;
};

const char* domain_name(Domain domain);
const char* span_role_name(Domain domain);

bool valid_trace_record_key(const std::string& value) noexcept;
bool valid_trace_domain_name(const std::string& value) noexcept;
bool trace_domain_span_role_matches(const std::string& domain,
                                    const std::string& span_role) noexcept;

const inferlab_plan::CliOptionSpec& trace_context_cli_spec();

Source load_trace_context_source(const std::filesystem::path& path);

Source load_trace_context_source_from_cli(
    const std::filesystem::path& repo_root, int argc, char** argv);

Source load_trace_context_source_from_cli(
    const std::filesystem::path& repo_root, int argc, char** argv,
    const inferlab_plan::CliOptionSpec& cli_spec);

Projection bind_trace_context(
    Domain domain, const std::vector<std::string>& expected_record_keys,
    Source source);

void validate_trace_context_projection(const Projection& projection);

void validate_trace_context_alignment(
    const Projection& projection, Domain expected_domain,
    const std::vector<std::string>& expected_record_keys);

}  // namespace inferlab_native_trace
