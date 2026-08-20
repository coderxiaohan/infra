#pragma once

#include "plan_json_utils.hpp"

#include <cstddef>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace inferlab_trace_clock {

enum class TimestampUnit {
  milliseconds,
  nanoseconds,
};

struct CalibrationEntry {
  std::size_t source_line_number = 0U;
  std::size_t anchor_timestamp = 0U;
  std::size_t trace_relative_anchor_ns = 0U;
  std::size_t uncertainty_ns = 0U;
  std::string trace_id;
  std::string source_scope;
  std::string clock_domain;
  TimestampUnit timestamp_unit = TimestampUnit::nanoseconds;
};

struct CalibrationSource {
  bool supplied = false;
  bool present = false;
  bool readable = false;
  bool contract_ready = false;
  bool path_redacted = false;
  std::size_t physical_line_count = 0U;
  std::string source_path_label = "not_provided";
  std::string path_probe_status = "not_configured";
  std::string text_read_status = "not_configured";
  std::string contract_fingerprint = "none";
  std::vector<CalibrationEntry> entries;
};

struct CalibrationSourceProjection {
  bool supplied = false;
  bool present = false;
  bool readable = false;
  bool contract_ready = false;
  bool path_redacted = false;
  std::size_t physical_line_count = 0U;
  std::size_t entry_count = 0U;
  std::string source_path_label = "not_provided";
  std::string path_probe_status = "not_configured";
  std::string text_read_status = "not_configured";
  std::string contract_fingerprint = "none";
};

struct NormalizedTimestamp {
  bool calibrated = false;
  std::size_t calibration_entry_ordinal = 0U;
  std::size_t trace_relative_timestamp_ns = 0U;
  std::size_t uncertainty_ns = 0U;
  std::string status = "clock_calibration_not_provided";
};

struct CalibrationUsageProjection {
  CalibrationSourceProjection source;
  std::size_t lookup_count = 0U;
  std::size_t calibrated_timestamp_count = 0U;
  std::size_t uncalibrated_timestamp_count = 0U;
  std::size_t distinct_calibration_entry_count = 0U;
  bool all_lookups_calibrated = false;
  std::string normalized_timestamp_unit = "trace_relative_nanoseconds";
  std::string calibration_scope =
      "trace_id_source_scope_clock_domain_affine_anchor_v1";
  std::string fingerprint_algorithm;
  std::string decision = "clock_calibration_not_provided";
};

class CalibrationUsageTracker {
 public:
  explicit CalibrationUsageTracker(const CalibrationSource& source);

  NormalizedTimestamp normalize(const std::string& trace_id,
                                const std::string& source_scope,
                                const std::string& clock_domain,
                                std::size_t timestamp);

  CalibrationUsageProjection finalize();

 private:
  const CalibrationSource* source_ = nullptr;
  CalibrationUsageProjection projection_;
  std::set<std::size_t> used_entry_ordinals_;
  bool finalized_ = false;
};

const char* timestamp_unit_name(TimestampUnit unit);
const char* clock_calibration_cli_option() noexcept;
const char* product_replay_clock_domain() noexcept;

const std::string& clock_calibration_fingerprint_algorithm();

std::string clock_calibration_usage_decision(
    bool source_supplied, std::size_t lookup_count,
    std::size_t calibrated_timestamp_count);

std::size_t maximum_trace_relative_timestamp_ns() noexcept;

bool valid_clock_domain(const std::string& value) noexcept;
bool valid_clock_source_scope(const std::string& value) noexcept;
bool valid_clock_calibration_fingerprint(
    const std::string& value) noexcept;

TimestampUnit timestamp_unit_for_clock_domain(
    const std::string& clock_domain);

std::string product_replay_clock_source_scope(
    std::size_t source_index, const std::string& product_profile_id);
std::string native_trace_clock_source_scope(
    const std::string& native_trace_domain);

CalibrationSource load_clock_calibration_source(
    const std::filesystem::path& path);

CalibrationSource load_clock_calibration_source_from_cli(
    const std::filesystem::path& repo_root, int argc, char** argv,
    const inferlab_plan::CliOptionSpec& cli_spec);

CalibrationSourceProjection project_clock_calibration_source(
    const CalibrationSource& source);

void validate_clock_calibration_source(
    const CalibrationSource& source);

void validate_clock_calibration_usage(
    const CalibrationUsageProjection& projection);

}  // namespace inferlab_trace_clock
