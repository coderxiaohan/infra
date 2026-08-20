#include "trace_clock_calibration.hpp"

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "trace_correlation_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace inferlab_trace_clock {
namespace {

constexpr std::size_t kMaximumSourceBytes = 2U * 1024U * 1024U;
constexpr std::size_t kMaximumPhysicalLineCount = 8192U;
constexpr std::size_t kMaximumLineBytes = 16U * 1024U;
constexpr std::size_t kMaximumSourceScopeBytes = 192U;
constexpr std::size_t kMaximumExactJsonInteger =
    9007199254740991ULL;
constexpr std::size_t kMillisecondsToNanoseconds = 1000000U;
constexpr char kFingerprintPrefix[] = "clock-fnv1a64x2:";
constexpr std::size_t kFingerprintPrefixLength =
    sizeof(kFingerprintPrefix) - 1U;
constexpr std::size_t kFingerprintHexLength = 32U;

using EntryKey = std::tuple<std::string, std::string, std::string>;

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

bool lowercase_hex(const char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f');
}

bool safe_supplied_source_path_label(
    const CalibrationSource& source) {
  return !source.source_path_label.empty() &&
         source.source_path_label != "not_provided" &&
         !inferlab_plan::looks_like_path_token(source.source_path_label) &&
         (source.path_redacted ||
          inferlab_plan::is_non_sensitive_path_placeholder(
              source.source_path_label));
}

bool safe_supplied_source_path_label(
    const CalibrationSourceProjection& source) {
  return !source.source_path_label.empty() &&
         source.source_path_label != "not_provided" &&
         !inferlab_plan::looks_like_path_token(source.source_path_label) &&
         (source.path_redacted ||
          inferlab_plan::is_non_sensitive_path_placeholder(
              source.source_path_label));
}

EntryKey entry_key(const CalibrationEntry& entry) {
  return {entry.trace_id, entry.source_scope, entry.clock_domain};
}

std::string canonical_fingerprint_payload(
    const std::vector<CalibrationEntry>& entries) {
  std::vector<const CalibrationEntry*> ordered;
  ordered.reserve(entries.size());
  for (const auto& entry : entries) ordered.push_back(&entry);
  std::sort(ordered.begin(), ordered.end(), [](const auto* lhs,
                                               const auto* rhs) {
    return entry_key(*lhs) < entry_key(*rhs);
  });

  std::string payload =
      "inferlab.trace-clock-calibration.v1|count=" +
      std::to_string(ordered.size()) + "|";
  for (const auto* entry : ordered) {
    payload += "trace=" + std::to_string(entry->trace_id.size()) + ":" +
               entry->trace_id;
    payload += "|scope=" + std::to_string(entry->source_scope.size()) +
               ":" + entry->source_scope;
    payload += "|domain=" + std::to_string(entry->clock_domain.size()) +
               ":" + entry->clock_domain;
    payload += "|unit=" +
               std::string(timestamp_unit_name(entry->timestamp_unit));
    payload += "|anchor=" + std::to_string(entry->anchor_timestamp);
    payload += "|relative_anchor_ns=" +
               std::to_string(entry->trace_relative_anchor_ns);
    payload += "|uncertainty_ns=" +
               std::to_string(entry->uncertainty_ns) + "|";
  }
  return payload;
}

std::string calibration_fingerprint(
    const std::vector<CalibrationEntry>& entries) {
  if (entries.empty()) return "none";
  const std::string payload = canonical_fingerprint_payload(entries);
  const std::string primary =
      "inferlab.trace-clock-calibration.primary|" + payload;
  const std::string secondary =
      "inferlab.trace-clock-calibration.secondary|" + payload;
  return std::string(kFingerprintPrefix) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(primary)) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(secondary));
}

CalibrationEntry parse_entry(const std::string& line,
                             const std::size_t line_number) {
  if (!inferlab_artifact::json_document_is_valid_object(line)) {
    throw std::invalid_argument(
        "trace clock calibration line is malformed or has duplicate keys at line " +
        std::to_string(line_number));
  }
  const auto trace_id =
      inferlab_artifact::project_string_field(line, {"trace_id"});
  const auto source_scope =
      inferlab_artifact::project_string_field(line, {"source_scope"});
  const auto clock_domain =
      inferlab_artifact::project_string_field(line, {"clock_domain"});
  const auto timestamp_unit =
      inferlab_artifact::project_string_field(line, {"timestamp_unit"});
  const auto anchor_timestamp =
      inferlab_artifact::project_unsigned_field(line, {"anchor_timestamp"});
  const auto relative_anchor = inferlab_artifact::project_unsigned_field(
      line, {"trace_relative_anchor_ns"});
  const auto uncertainty =
      inferlab_artifact::project_unsigned_field(line, {"uncertainty_ns"});

  if (!trace_id.present || trace_id.invalid ||
      !inferlab_trace::valid_w3c_trace_id(trace_id.value) ||
      !source_scope.present || source_scope.invalid ||
      !valid_clock_source_scope(source_scope.value) ||
      !clock_domain.present || clock_domain.invalid ||
      !valid_clock_domain(clock_domain.value) ||
      !timestamp_unit.present || timestamp_unit.invalid ||
      !anchor_timestamp.present || anchor_timestamp.invalid ||
      !relative_anchor.present || relative_anchor.invalid ||
      !uncertainty.present || uncertainty.invalid ||
      relative_anchor.value > maximum_trace_relative_timestamp_ns() ||
      uncertainty.value > maximum_trace_relative_timestamp_ns()) {
    throw std::invalid_argument(
        "trace clock calibration field contract is invalid at line " +
        std::to_string(line_number));
  }

  const TimestampUnit expected_unit =
      timestamp_unit_for_clock_domain(clock_domain.value);
  if (timestamp_unit.value != timestamp_unit_name(expected_unit)) {
    throw std::invalid_argument(
        "trace clock calibration timestamp unit disagrees with clock domain at line " +
        std::to_string(line_number));
  }

  CalibrationEntry entry;
  entry.source_line_number = line_number;
  entry.anchor_timestamp = anchor_timestamp.value;
  entry.trace_relative_anchor_ns = relative_anchor.value;
  entry.uncertainty_ns = uncertainty.value;
  entry.trace_id = trace_id.value;
  entry.source_scope = source_scope.value;
  entry.clock_domain = clock_domain.value;
  entry.timestamp_unit = expected_unit;
  return entry;
}

}  // namespace

const char* timestamp_unit_name(const TimestampUnit unit) {
  switch (unit) {
    case TimestampUnit::milliseconds:
      return "milliseconds";
    case TimestampUnit::nanoseconds:
      return "nanoseconds";
  }
  throw std::invalid_argument("unknown trace clock timestamp unit");
}

const char* clock_calibration_cli_option() noexcept {
  return "--trace-clock-calibration-file";
}

const char* product_replay_clock_domain() noexcept {
  return "product_log_ms";
}

const std::string& clock_calibration_fingerprint_algorithm() {
  static const std::string algorithm =
      "fnv1a64x2_length_framed_clock_calibration_v1_noncryptographic";
  return algorithm;
}

std::string clock_calibration_usage_decision(
    const bool source_supplied, const std::size_t lookup_count,
    const std::size_t calibrated_timestamp_count) {
  if (calibrated_timestamp_count > lookup_count) {
    throw std::invalid_argument(
        "clock calibration calibrated count exceeds lookup count");
  }
  if (!source_supplied) return "clock_calibration_not_provided";
  if (lookup_count == 0U) {
    return "clock_calibration_provided_without_lookups";
  }
  if (calibrated_timestamp_count == lookup_count) {
    return "trace_clock_calibration_ready";
  }
  if (calibrated_timestamp_count > 0U) {
    return "partial_trace_clock_calibration";
  }
  return "clock_calibration_provided_without_matches";
}

std::size_t maximum_trace_relative_timestamp_ns() noexcept {
  return kMaximumExactJsonInteger;
}

bool valid_clock_domain(const std::string& value) noexcept {
  return value == product_replay_clock_domain() ||
         value == "unix_epoch_ns" || value == "steady_clock_ns" ||
         value == "cuda_event_ns";
}

bool valid_clock_source_scope(const std::string& value) noexcept {
  if (value.empty() || value.size() > kMaximumSourceScopeBytes) return false;
  return std::all_of(value.begin(), value.end(), [](const char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
           ch == '.' || ch == ':';
  });
}

bool valid_clock_calibration_fingerprint(
    const std::string& value) noexcept {
  if (value == "none") return true;
  if (value.size() != kFingerprintPrefixLength + kFingerprintHexLength ||
      value.compare(0U, kFingerprintPrefixLength, kFingerprintPrefix,
                    kFingerprintPrefixLength) != 0) {
    return false;
  }
  return std::all_of(
      value.begin() + static_cast<std::ptrdiff_t>(kFingerprintPrefixLength),
      value.end(), lowercase_hex);
}

TimestampUnit timestamp_unit_for_clock_domain(
    const std::string& clock_domain) {
  if (clock_domain == product_replay_clock_domain()) {
    return TimestampUnit::milliseconds;
  }
  if (clock_domain == "unix_epoch_ns" ||
      clock_domain == "steady_clock_ns" ||
      clock_domain == "cuda_event_ns") {
    return TimestampUnit::nanoseconds;
  }
  throw std::invalid_argument("unsupported trace clock domain");
}

std::string product_replay_clock_source_scope(
    const std::size_t source_index,
    const std::string& product_profile_id) {
  if (product_profile_id.empty() ||
      !std::all_of(product_profile_id.begin(), product_profile_id.end(),
                   [](const char ch) {
                     return (ch >= 'a' && ch <= 'z') ||
                            (ch >= 'A' && ch <= 'Z') ||
                            (ch >= '0' && ch <= '9') || ch == '_' ||
                            ch == '-' || ch == '.';
                   })) {
    throw std::invalid_argument(
        "product replay clock scope requires a canonical profile id");
  }
  const std::string scope = "product_replay:" + product_profile_id +
                            ":source:" + std::to_string(source_index);
  if (!valid_clock_source_scope(scope)) {
    throw std::invalid_argument("product replay clock scope is too long");
  }
  return scope;
}

std::string native_trace_clock_source_scope(
    const std::string& native_trace_domain) {
  const std::string scope = "native_trace:" + native_trace_domain;
  if (!valid_clock_source_scope(scope)) {
    throw std::invalid_argument(
        "native trace clock scope requires a canonical domain");
  }
  return scope;
}

CalibrationSource load_clock_calibration_source(
    const std::filesystem::path& path) {
  CalibrationSource source;
  if (path.empty()) return source;

  source.supplied = true;
  source.source_path_label =
      inferlab_plan::redacted_path_label(path.generic_string());
  source.path_redacted = inferlab_plan::path_was_redacted(
      path.generic_string(), source.source_path_label);
  source.path_probe_status = inferlab_artifact::path_probe_status(path);
  source.present = source.path_probe_status != "missing";
  if (source.path_probe_status != "file") {
    throw std::invalid_argument(
        "trace clock calibration source is not a readable regular file: " +
        source.source_path_label);
  }

  const auto read = inferlab_artifact::read_text_with_status(
      path, kMaximumSourceBytes);
  source.readable = read.readable;
  source.text_read_status = read.status;
  if (!read.readable || read.empty) {
    throw std::invalid_argument(
        "trace clock calibration bounded read failed: " +
        source.source_path_label + ":" + read.status);
  }

  std::set<EntryKey> keys;
  std::istringstream input(read.text);
  std::string line;
  std::size_t line_number = 0U;
  while (std::getline(input, line)) {
    increment(line_number, "trace clock calibration physical line count");
    if (line_number > kMaximumPhysicalLineCount) {
      throw std::invalid_argument(
          "trace clock calibration exceeds the physical line limit");
    }
    if (line.size() > kMaximumLineBytes) {
      throw std::invalid_argument(
          "trace clock calibration line exceeds the byte limit at line " +
          std::to_string(line_number));
    }
    if (!inferlab_plan::has_non_whitespace(line)) continue;
    CalibrationEntry entry = parse_entry(line, line_number);
    if (!keys.insert(entry_key(entry)).second) {
      throw std::invalid_argument(
          "trace clock calibration contains a duplicate trace/scope/domain key at line " +
          std::to_string(line_number));
    }
    source.entries.push_back(std::move(entry));
  }
  source.physical_line_count = line_number;
  if (source.entries.empty()) {
    throw std::invalid_argument(
        "trace clock calibration contains no entries");
  }
  source.contract_fingerprint = calibration_fingerprint(source.entries);
  source.contract_ready = true;
  validate_clock_calibration_source(source);
  return source;
}

CalibrationSource load_clock_calibration_source_from_cli(
    const std::filesystem::path& repo_root, const int argc, char** argv,
    const inferlab_plan::CliOptionSpec& cli_spec) {
  const std::filesystem::path argument =
      inferlab_plan::arg_value_with_spec(
          argc, argv, clock_calibration_cli_option(), "", cli_spec);
  return load_clock_calibration_source(
      inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                         argument));
}

CalibrationSourceProjection project_clock_calibration_source(
    const CalibrationSource& source) {
  validate_clock_calibration_source(source);
  CalibrationSourceProjection projection;
  projection.supplied = source.supplied;
  projection.present = source.present;
  projection.readable = source.readable;
  projection.contract_ready = source.contract_ready;
  projection.path_redacted = source.path_redacted;
  projection.physical_line_count = source.physical_line_count;
  projection.entry_count = source.entries.size();
  projection.source_path_label = source.source_path_label;
  projection.path_probe_status = source.path_probe_status;
  projection.text_read_status = source.text_read_status;
  projection.contract_fingerprint = source.contract_fingerprint;
  return projection;
}

void validate_clock_calibration_source(
    const CalibrationSource& source) {
  if (!source.supplied) {
    if (source.present || source.readable || source.contract_ready ||
        source.path_redacted || source.physical_line_count != 0U ||
        !source.entries.empty() ||
        source.source_path_label != "not_provided" ||
        source.path_probe_status != "not_configured" ||
        source.text_read_status != "not_configured" ||
        source.contract_fingerprint != "none") {
      throw std::invalid_argument(
          "trace clock calibration has data without a supplied source");
    }
    return;
  }
  if (!source.present || !source.readable || !source.contract_ready ||
      !safe_supplied_source_path_label(source) ||
      source.path_probe_status != "file" ||
      source.text_read_status != "ok" || source.entries.empty() ||
      source.entries.size() > kMaximumPhysicalLineCount ||
      source.physical_line_count < source.entries.size() ||
      source.physical_line_count > kMaximumPhysicalLineCount ||
      !valid_clock_calibration_fingerprint(source.contract_fingerprint) ||
      source.contract_fingerprint == "none" ||
      calibration_fingerprint(source.entries) !=
          source.contract_fingerprint) {
    throw std::invalid_argument(
        "trace clock calibration source metadata is inconsistent");
  }

  std::set<EntryKey> keys;
  for (const auto& entry : source.entries) {
    if (entry.source_line_number == 0U ||
        entry.source_line_number > source.physical_line_count ||
        !inferlab_trace::valid_w3c_trace_id(entry.trace_id) ||
        !valid_clock_source_scope(entry.source_scope) ||
        !valid_clock_domain(entry.clock_domain) ||
        entry.timestamp_unit !=
            timestamp_unit_for_clock_domain(entry.clock_domain) ||
        entry.trace_relative_anchor_ns >
            maximum_trace_relative_timestamp_ns() ||
        entry.uncertainty_ns > maximum_trace_relative_timestamp_ns() ||
        !keys.insert(entry_key(entry)).second) {
      throw std::invalid_argument(
          "trace clock calibration entry contract is inconsistent");
    }
  }
}

CalibrationUsageTracker::CalibrationUsageTracker(
    const CalibrationSource& source)
    : source_(&source) {
  validate_clock_calibration_source(source);
  projection_.source = project_clock_calibration_source(source);
  projection_.fingerprint_algorithm =
      clock_calibration_fingerprint_algorithm();
}

NormalizedTimestamp CalibrationUsageTracker::normalize(
    const std::string& trace_id, const std::string& source_scope,
    const std::string& clock_domain, const std::size_t timestamp) {
  if (finalized_) {
    throw std::logic_error(
        "trace clock calibration usage is already finalized");
  }
  if (!inferlab_trace::valid_w3c_trace_id(trace_id) ||
      !valid_clock_source_scope(source_scope) ||
      !valid_clock_domain(clock_domain)) {
    throw std::invalid_argument(
        "trace clock calibration lookup key is invalid");
  }
  increment(projection_.lookup_count,
            "trace clock calibration lookup count");

  NormalizedTimestamp result;
  if (!source_->supplied) {
    increment(projection_.uncalibrated_timestamp_count,
              "uncalibrated trace timestamp count");
    return result;
  }

  const CalibrationEntry* match = nullptr;
  std::size_t match_ordinal = 0U;
  for (std::size_t index = 0U; index < source_->entries.size(); ++index) {
    const auto& entry = source_->entries[index];
    if (entry.trace_id != trace_id || entry.source_scope != source_scope ||
        entry.clock_domain != clock_domain) {
      continue;
    }
    if (match != nullptr) {
      throw std::logic_error(
          "trace clock calibration lookup resolved duplicate entries");
    }
    match = &entry;
    match_ordinal = inferlab::internal::checked_add_size(
        index, 1U, "trace clock calibration entry ordinal");
  }
  if (match == nullptr) {
    result.status = "clock_calibration_entry_missing";
    increment(projection_.uncalibrated_timestamp_count,
              "uncalibrated trace timestamp count");
    return result;
  }
  if (timestamp < match->anchor_timestamp) {
    throw std::invalid_argument(
        "trace timestamp precedes its calibration anchor");
  }
  const std::size_t delta = timestamp - match->anchor_timestamp;
  const std::size_t multiplier =
      match->timestamp_unit == TimestampUnit::milliseconds
          ? kMillisecondsToNanoseconds
          : 1U;
  const std::size_t delta_ns = inferlab::internal::checked_product_size(
      delta, multiplier, "trace clock calibrated timestamp delta");
  const std::size_t relative_ns = inferlab::internal::checked_add_size(
      match->trace_relative_anchor_ns, delta_ns,
      "trace clock relative timestamp");
  if (relative_ns > maximum_trace_relative_timestamp_ns()) {
    throw std::invalid_argument(
        "trace clock relative timestamp exceeds exact JSON range");
  }

  result.calibrated = true;
  result.calibration_entry_ordinal = match_ordinal;
  result.trace_relative_timestamp_ns = relative_ns;
  result.uncertainty_ns = match->uncertainty_ns;
  result.status = "trace_relative_timestamp_ready";
  increment(projection_.calibrated_timestamp_count,
            "calibrated trace timestamp count");
  used_entry_ordinals_.insert(match_ordinal);
  return result;
}

CalibrationUsageProjection CalibrationUsageTracker::finalize() {
  if (finalized_) {
    throw std::logic_error(
        "trace clock calibration usage was finalized twice");
  }
  finalized_ = true;
  projection_.distinct_calibration_entry_count =
      used_entry_ordinals_.size();
  projection_.all_lookups_calibrated =
      projection_.lookup_count > 0U &&
      projection_.calibrated_timestamp_count == projection_.lookup_count;
  projection_.decision = clock_calibration_usage_decision(
      projection_.source.supplied, projection_.lookup_count,
      projection_.calibrated_timestamp_count);
  validate_clock_calibration_usage(projection_);
  return projection_;
}

void validate_clock_calibration_usage(
    const CalibrationUsageProjection& projection) {
  const auto& source = projection.source;
  if (source.supplied != source.present ||
      source.supplied != source.readable ||
      source.supplied != source.contract_ready ||
      source.entry_count > source.physical_line_count ||
      projection.calibrated_timestamp_count > projection.lookup_count ||
      inferlab::internal::checked_add_size(
          projection.calibrated_timestamp_count,
          projection.uncalibrated_timestamp_count,
          "trace clock usage partition") != projection.lookup_count ||
      projection.distinct_calibration_entry_count >
          projection.calibrated_timestamp_count ||
      projection.distinct_calibration_entry_count > source.entry_count ||
      projection.all_lookups_calibrated !=
          (projection.lookup_count > 0U &&
           projection.calibrated_timestamp_count ==
               projection.lookup_count) ||
      projection.normalized_timestamp_unit !=
          "trace_relative_nanoseconds" ||
      projection.calibration_scope !=
          "trace_id_source_scope_clock_domain_affine_anchor_v1" ||
      projection.fingerprint_algorithm !=
          clock_calibration_fingerprint_algorithm() ||
      projection.decision != clock_calibration_usage_decision(
          source.supplied, projection.lookup_count,
          projection.calibrated_timestamp_count) ||
      !valid_clock_calibration_fingerprint(source.contract_fingerprint)) {
    throw std::logic_error(
        "trace clock calibration usage summary is inconsistent");
  }
  if (!source.supplied) {
    if (source.path_redacted || source.physical_line_count != 0U ||
        source.entry_count != 0U ||
        source.source_path_label != "not_provided" ||
        source.path_probe_status != "not_configured" ||
        source.text_read_status != "not_configured" ||
        source.contract_fingerprint != "none" ||
        projection.calibrated_timestamp_count != 0U ||
        projection.distinct_calibration_entry_count != 0U) {
      throw std::logic_error(
          "absent trace clock calibration exposes source data");
    }
  } else if (!safe_supplied_source_path_label(source) ||
             source.path_probe_status != "file" ||
             source.text_read_status != "ok" ||
             source.entry_count == 0U ||
             source.contract_fingerprint == "none") {
    throw std::logic_error(
        "supplied trace clock calibration source projection is invalid");
  }
}

}  // namespace inferlab_trace_clock
