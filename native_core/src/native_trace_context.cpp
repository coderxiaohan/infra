#include "native_trace_context.hpp"

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "trace_correlation_contract.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inferlab_native_trace {
namespace {

constexpr std::size_t kMaximumSourceBytes = 1024U * 1024U;
constexpr std::size_t kMaximumPhysicalLineCount = 4096U;
constexpr std::size_t kMaximumLineBytes = 16U * 1024U;
constexpr std::size_t kMaximumRecordKeyBytes = 128U;

struct DomainSpec {
  Domain domain;
  std::string_view name;
  std::string_view span_role;
};

constexpr std::array<DomainSpec, 6U> kDomainSpecs{{
    {Domain::kv_paged_attention, "kv_paged_attention",
     "kv.paged_attention"},
    {Domain::scheduler_batching, "scheduler_batching",
     "scheduler.batch"},
    {Domain::scheduler_root_cause, "scheduler_root_cause",
     "scheduler.root_cause"},
    {Domain::cuda_quant_microkernel, "cuda_quant_microkernel",
     "cuda.microkernel"},
    {Domain::pd_split_crossover, "pd_split_crossover",
     "serving.pd_split_crossover"},
    {Domain::kv_aware_routing, "kv_aware_routing",
     "routing.kv_aware"},
}};

constexpr bool valid_domain_specs() noexcept {
  for (std::size_t index = 0U; index < kDomainSpecs.size(); ++index) {
    const auto& current = kDomainSpecs[index];
    if (current.name.empty() || current.span_role.empty()) return false;
    for (std::size_t prior = 0U; prior < index; ++prior) {
      const auto& existing = kDomainSpecs[prior];
      if (current.domain == existing.domain ||
          current.name == existing.name ||
          current.span_role == existing.span_role) {
        return false;
      }
    }
  }
  return true;
}

static_assert(
    kDomainSpecs.size() ==
        static_cast<std::size_t>(Domain::kv_aware_routing) + 1U,
    "native trace domain registry must cover every enum value");
static_assert(valid_domain_specs(),
              "native trace domain registry must be nonempty and unique");

const DomainSpec* find_domain_spec(const Domain domain) noexcept {
  const auto found = std::find_if(
      kDomainSpecs.begin(), kDomainSpecs.end(),
      [domain](const DomainSpec& spec) { return spec.domain == domain; });
  return found == kDomainSpecs.end() ? nullptr : &*found;
}

const DomainSpec* find_domain_spec(const std::string& name) noexcept {
  const auto found = std::find_if(
      kDomainSpecs.begin(), kDomainSpecs.end(),
      [&name](const DomainSpec& spec) {
        return std::string_view(name) == spec.name;
      });
  return found == kDomainSpecs.end() ? nullptr : &*found;
}

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

bool supported_clock_domain(const std::string& value) {
  return value != inferlab_trace_clock::product_replay_clock_domain() &&
         inferlab_trace_clock::valid_clock_domain(value);
}

bool safe_supplied_source_path_label(const Source& source) {
  return !source.source_path_label.empty() &&
         source.source_path_label != "not_provided" &&
         !inferlab_plan::looks_like_path_token(source.source_path_label) &&
         (source.path_redacted ||
          inferlab_plan::is_non_sensitive_path_placeholder(
              source.source_path_label));
}

InputRecord parse_record(const std::string& line,
                         const std::size_t line_number) {
  if (!inferlab_artifact::json_document_is_valid_object(line)) {
    throw std::invalid_argument(
        "native trace sidecar line is malformed or has duplicate keys at line " +
        std::to_string(line_number));
  }
  const auto record_key =
      inferlab_artifact::project_string_field(line, {"record_key"});
  const auto trace_id =
      inferlab_artifact::project_string_field(line, {"trace_id"});
  const auto span_id =
      inferlab_artifact::project_string_field(line, {"span_id"});
  const auto clock_domain =
      inferlab_artifact::project_string_field(line, {"clock_domain"});
  const auto timestamp_ns =
      inferlab_artifact::project_unsigned_field(line, {"timestamp_ns"});
  const bool span_start_key_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "span_start_timestamp_ns");
  const bool span_end_key_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "span_end_timestamp_ns");
  const bool span_terminal_status_key_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "span_terminal_status");
  const auto span_start_timestamp_ns =
      inferlab_artifact::project_unsigned_field(
          line, {"span_start_timestamp_ns"});
  const auto span_end_timestamp_ns =
      inferlab_artifact::project_unsigned_field(
          line, {"span_end_timestamp_ns"});
  const auto span_terminal_status =
      inferlab_artifact::project_string_field(
          line, {"span_terminal_status"});
  const bool parent_key_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "parent_span_id");
  const auto parent_span_id =
      inferlab_artifact::project_string_field(line, {"parent_span_id"});
  const bool causal_predecessor_key_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "causal_predecessor_span_id");
  const auto causal_predecessor_span_id =
      inferlab_artifact::project_string_field(
          line, {"causal_predecessor_span_id"});
  const bool any_lifecycle_field =
      span_start_key_present || span_end_key_present ||
      span_terminal_status_key_present;
  const bool complete_lifecycle_bundle =
      span_start_key_present && span_end_key_present &&
      span_terminal_status_key_present;

  if (!record_key.present || record_key.invalid ||
      !valid_trace_record_key(record_key.value) || !trace_id.present ||
      trace_id.invalid ||
      !inferlab_trace::valid_w3c_trace_id(trace_id.value) ||
      !span_id.present || span_id.invalid ||
      !inferlab_trace::valid_w3c_span_id(span_id.value) ||
      !clock_domain.present || clock_domain.invalid ||
      !supported_clock_domain(clock_domain.value) ||
      !timestamp_ns.present || timestamp_ns.invalid ||
      (any_lifecycle_field && !complete_lifecycle_bundle) ||
      (complete_lifecycle_bundle &&
       (!span_start_timestamp_ns.present ||
        span_start_timestamp_ns.invalid ||
        !span_end_timestamp_ns.present ||
        span_end_timestamp_ns.invalid ||
        !span_terminal_status.present ||
        span_terminal_status.invalid)) ||
      (causal_predecessor_key_present &&
       (!complete_lifecycle_bundle ||
        !causal_predecessor_span_id.present ||
        causal_predecessor_span_id.invalid ||
        !inferlab_trace::valid_w3c_span_id(
            causal_predecessor_span_id.value) ||
        causal_predecessor_span_id.value == span_id.value)) ||
      (parent_key_present &&
       (!parent_span_id.present || parent_span_id.invalid ||
        !inferlab_trace::valid_w3c_span_id(parent_span_id.value) ||
        parent_span_id.value == span_id.value))) {
    throw std::invalid_argument(
        "native trace sidecar field contract is invalid at line " +
        std::to_string(line_number));
  }

  InputRecord record;
  record.source_line_number = line_number;
  record.timestamp_ns = timestamp_ns.value;
  record.record_key = record_key.value;
  record.trace_id = trace_id.value;
  record.span_id = span_id.value;
  record.clock_domain = clock_domain.value;
  record.parent_span_present = parent_key_present;
  if (record.parent_span_present) {
    record.parent_span_id = parent_span_id.value;
  }
  record.span_lifecycle_supplied = complete_lifecycle_bundle;
  if (record.span_lifecycle_supplied) {
    inferlab_trace_lifecycle::LifecycleInput lifecycle_input;
    lifecycle_input.start_event_count = 1U;
    lifecycle_input.terminal_event_count = 1U;
    lifecycle_input.start_timestamp_present = true;
    lifecycle_input.terminal_timestamp_present = true;
    lifecycle_input.start_timestamp = span_start_timestamp_ns.value;
    lifecycle_input.terminal_timestamp = span_end_timestamp_ns.value;
    lifecycle_input.observed_timestamp_bounds_present = true;
    lifecycle_input.observed_minimum_timestamp = record.timestamp_ns;
    lifecycle_input.observed_maximum_timestamp = record.timestamp_ns;
    lifecycle_input.terminal_status = span_terminal_status.value;
    const auto lifecycle =
        inferlab_trace_lifecycle::evaluate_lifecycle(lifecycle_input);
    if (!lifecycle.contract_ready) {
      throw std::invalid_argument(
          "native trace sidecar span lifecycle contract is invalid at line " +
          std::to_string(line_number));
    }
    record.span_start_timestamp_ns = span_start_timestamp_ns.value;
    record.span_end_timestamp_ns = span_end_timestamp_ns.value;
    record.span_terminal_status = span_terminal_status.value;
  }
  record.causal_predecessor_span_present =
      causal_predecessor_key_present;
  if (record.causal_predecessor_span_present) {
    record.causal_predecessor_span_id =
        causal_predecessor_span_id.value;
  }
  return record;
}

std::string expected_decision(const Summary& summary) {
  if (!summary.source_supplied) return "not_provided";
  if (summary.record_count > 0U &&
      summary.context_present_count == summary.record_count) {
    return "anonymous_trace_context_ready";
  }
  return "partial_anonymous_trace_context";
}

std::string expected_span_lifecycle_decision(const Summary& summary) {
  if (!summary.source_supplied ||
      summary.span_lifecycle_supplied_record_count == 0U) {
    return "not_provided";
  }
  if (summary.span_lifecycle_supplied_record_count <
      summary.context_present_count) {
    return "partial_span_lifecycle";
  }
  if (summary.span_lifecycle_calibrated_record_count <
      summary.span_lifecycle_supplied_record_count) {
    return "span_lifecycle_pending_clock_calibration";
  }
  return "span_lifecycle_ready";
}

void discard_raw_identity_values(Source& source) {
  for (auto& record : source.records) {
    record.trace_id.clear();
    record.span_id.clear();
    record.parent_span_id.clear();
    record.causal_predecessor_span_id.clear();
  }
  for (auto& entry : source.clock_calibration.entries) {
    entry.trace_id.clear();
  }
  source.clock_calibration.entries.clear();
}

void validate_source_for_binding(const Source& source) {
  inferlab_trace_clock::validate_clock_calibration_source(
      source.clock_calibration);
  if (!source.supplied) {
    if (source.present || source.readable || source.contract_ready ||
        source.path_redacted ||
        source.physical_line_count != 0U || !source.records.empty() ||
        source.source_path_label != "not_provided" ||
        source.path_probe_status != "not_configured" ||
        source.text_read_status != "not_configured" ||
        source.clock_calibration.supplied) {
      throw std::invalid_argument(
          "native trace context has data without a supplied source");
    }
    return;
  }
  if (!source.present || !source.readable || !source.contract_ready ||
      source.records.empty() ||
      !safe_supplied_source_path_label(source) ||
      source.path_probe_status != "file" ||
      source.text_read_status != "ok" ||
      source.physical_line_count < source.records.size() ||
      source.physical_line_count > kMaximumPhysicalLineCount) {
    throw std::invalid_argument(
        "native trace context supplied source metadata is inconsistent");
  }

  std::set<std::string> record_keys;
  std::set<std::pair<std::string, std::string>> span_contexts;
  for (const auto& record : source.records) {
    bool lifecycle_contract_ready = false;
    if (record.span_lifecycle_supplied) {
      inferlab_trace_lifecycle::LifecycleInput lifecycle_input;
      lifecycle_input.start_event_count = 1U;
      lifecycle_input.terminal_event_count = 1U;
      lifecycle_input.start_timestamp_present = true;
      lifecycle_input.terminal_timestamp_present = true;
      lifecycle_input.start_timestamp = record.span_start_timestamp_ns;
      lifecycle_input.terminal_timestamp = record.span_end_timestamp_ns;
      lifecycle_input.observed_timestamp_bounds_present = true;
      lifecycle_input.observed_minimum_timestamp = record.timestamp_ns;
      lifecycle_input.observed_maximum_timestamp = record.timestamp_ns;
      lifecycle_input.terminal_status = record.span_terminal_status;
      lifecycle_contract_ready =
          inferlab_trace_lifecycle::evaluate_lifecycle(lifecycle_input)
              .contract_ready;
    }
    if (record.source_line_number == 0U ||
        record.source_line_number > source.physical_line_count ||
        !valid_trace_record_key(record.record_key) ||
        !record_keys.insert(record.record_key).second ||
        !inferlab_trace::valid_w3c_trace_id(record.trace_id) ||
        !inferlab_trace::valid_w3c_span_id(record.span_id) ||
        !supported_clock_domain(record.clock_domain) ||
        (record.parent_span_present != !record.parent_span_id.empty()) ||
        (record.parent_span_present &&
         (!inferlab_trace::valid_w3c_span_id(record.parent_span_id) ||
          record.parent_span_id == record.span_id)) ||
        (record.span_lifecycle_supplied != lifecycle_contract_ready) ||
        (!record.span_lifecycle_supplied &&
         (record.span_start_timestamp_ns != 0U ||
          record.span_end_timestamp_ns != 0U ||
          record.span_terminal_status != "none" ||
          record.causal_predecessor_span_present ||
          !record.causal_predecessor_span_id.empty())) ||
        (record.causal_predecessor_span_present !=
         !record.causal_predecessor_span_id.empty()) ||
        (record.causal_predecessor_span_present &&
         (!record.span_lifecycle_supplied ||
          !inferlab_trace::valid_w3c_span_id(
              record.causal_predecessor_span_id) ||
          record.causal_predecessor_span_id == record.span_id)) ||
        !span_contexts.emplace(record.trace_id, record.span_id).second) {
      throw std::invalid_argument(
          "native trace context supplied record contract is inconsistent");
    }
  }
}

void validate_expected_record_keys(
    const std::vector<std::string>& expected_record_keys) {
  if (expected_record_keys.empty()) {
    throw std::invalid_argument(
        "native trace context requires expected record keys");
  }
  std::set<std::string> expected_set;
  for (const auto& key : expected_record_keys) {
    if (!valid_trace_record_key(key) || !expected_set.insert(key).second) {
      throw std::invalid_argument(
          "native trace expected record keys are invalid or duplicated");
    }
  }
}

}  // namespace

bool valid_trace_record_key(const std::string& value) noexcept {
  if (value.empty() || value.size() > kMaximumRecordKeyBytes) return false;
  return std::all_of(value.begin(), value.end(), [](const char ch) {
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' ||
           ch == '.' || ch == ':';
  });
}

const char* domain_name(const Domain domain) {
  const DomainSpec* spec = find_domain_spec(domain);
  if (spec != nullptr) return spec->name.data();
  throw std::invalid_argument("unknown native trace context domain");
}

const char* span_role_name(const Domain domain) {
  const DomainSpec* spec = find_domain_spec(domain);
  if (spec != nullptr) return spec->span_role.data();
  throw std::invalid_argument("unknown native trace context domain");
}

bool valid_trace_domain_name(const std::string& value) noexcept {
  return find_domain_spec(value) != nullptr;
}

bool trace_domain_span_role_matches(const std::string& domain,
                                    const std::string& span_role) noexcept {
  const DomainSpec* spec = find_domain_spec(domain);
  return spec != nullptr &&
         std::string_view(span_role) == spec->span_role;
}

const inferlab_plan::CliOptionSpec& trace_context_cli_spec() {
  static const inferlab_plan::CliOptionSpec spec{
      {"--repo-root", "--output", "--trace-context-file",
       inferlab_trace_clock::clock_calibration_cli_option()},
      {}};
  return spec;
}

Source load_trace_context_source(const std::filesystem::path& path) {
  Source source;
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
        "native trace sidecar is not a readable regular file: " +
        source.source_path_label);
  }

  const auto read = inferlab_artifact::read_text_with_status(
      path, kMaximumSourceBytes);
  source.readable = read.readable;
  source.text_read_status = read.status;
  if (!read.readable || read.empty) {
    throw std::invalid_argument(
        "native trace sidecar bounded read failed: " +
        source.source_path_label + ":" + read.status);
  }

  std::set<std::string> record_keys;
  std::set<std::pair<std::string, std::string>> span_keys;
  std::istringstream input(read.text);
  std::string line;
  std::size_t line_number = 0U;
  while (std::getline(input, line)) {
    increment(line_number, "native trace sidecar physical line count");
    if (line_number > kMaximumPhysicalLineCount) {
      throw std::invalid_argument(
          "native trace sidecar exceeds the physical line limit");
    }
    if (line.size() > kMaximumLineBytes) {
      throw std::invalid_argument(
          "native trace sidecar line exceeds the byte limit at line " +
          std::to_string(line_number));
    }
    if (!inferlab_plan::has_non_whitespace(line)) continue;
    InputRecord record = parse_record(line, line_number);
    if (!record_keys.insert(record.record_key).second) {
      throw std::invalid_argument(
          "native trace sidecar contains a duplicate record key at line " +
          std::to_string(line_number));
    }
    if (!span_keys.emplace(record.trace_id, record.span_id).second) {
      throw std::invalid_argument(
          "native trace sidecar contains a duplicate trace/span pair at line " +
          std::to_string(line_number));
    }
    source.records.push_back(std::move(record));
  }
  source.physical_line_count = line_number;
  if (source.records.empty()) {
    throw std::invalid_argument(
        "native trace sidecar contains no context records");
  }
  source.contract_ready = true;
  return source;
}

Source load_trace_context_source_from_cli(
    const std::filesystem::path& repo_root, const int argc, char** argv,
    const inferlab_plan::CliOptionSpec& cli_spec) {
  const std::filesystem::path argument = inferlab_plan::arg_value_with_spec(
      argc, argv, "--trace-context-file", "", cli_spec);
  Source source = load_trace_context_source(
      inferlab_plan::resolve_optional_repo_relative_path(repo_root,
                                                         argument));
  source.clock_calibration =
      inferlab_trace_clock::load_clock_calibration_source_from_cli(
          repo_root, argc, argv, cli_spec);
  if (!source.supplied && source.clock_calibration.supplied) {
    throw std::invalid_argument(
        "trace clock calibration requires --trace-context-file");
  }
  return source;
}

Source load_trace_context_source_from_cli(
    const std::filesystem::path& repo_root, const int argc, char** argv) {
  return load_trace_context_source_from_cli(
      repo_root, argc, argv, trace_context_cli_spec());
}

Projection bind_trace_context(
    const Domain domain,
    const std::vector<std::string>& expected_record_keys,
    Source source) {
  validate_expected_record_keys(expected_record_keys);
  validate_source_for_binding(source);
  (void)domain_name(domain);
  (void)span_role_name(domain);

  const std::set<std::string> expected_set(expected_record_keys.begin(),
                                           expected_record_keys.end());

  std::map<std::string, const InputRecord*> by_key;
  for (const auto& record : source.records) {
    if (expected_set.find(record.record_key) == expected_set.end()) {
      throw std::invalid_argument(
          "native trace sidecar record key is not owned by this artifact at line " +
          std::to_string(record.source_line_number));
    }
    by_key.emplace(record.record_key, &record);
  }

  Projection projection;
  projection.domain = domain;
  projection.source.supplied = source.supplied;
  projection.source.present = source.present;
  projection.source.readable = source.readable;
  projection.source.contract_ready = source.contract_ready;
  projection.source.path_redacted = source.path_redacted;
  projection.source.physical_line_count = source.physical_line_count;
  projection.source.source_path_label = source.source_path_label;
  projection.source.path_probe_status = source.path_probe_status;
  projection.source.text_read_status = source.text_read_status;
  projection.summary.source_supplied = source.supplied;
  projection.summary.source_contract_ready = source.contract_ready;
  projection.summary.source_record_count = source.records.size();
  projection.join_token_algorithm =
      inferlab_trace::trace_context_join_token_algorithm();
  projection.claim_boundary =
      "optional external native trace sidecar, explicit span lifecycle and affine clock-calibration projection only; raw W3C trace/span/predecessor ids, absolute timestamps and calibration anchors are not emitted, trace-relative intervals and declared predecessor tokens require downstream same-request and temporal evaluation, join tokens and calibration fingerprints are non-cryptographic equality hints, and this artifact does not perform a cross-artifact join, infer causality, propagate errors or cancellation, attribute latency, prove runtime behavior, performance, execute evidence, or release readiness";

  inferlab_trace_clock::CalibrationUsageTracker clock_usage(
      source.clock_calibration);
  inferlab_trace_clock::CalibrationUsageTracker lifecycle_clock_usage(
      source.clock_calibration);
  const std::string clock_source_scope =
      inferlab_trace_clock::native_trace_clock_source_scope(
          domain_name(domain));

  std::set<std::string> distinct_trace_tokens;
  std::set<std::string> distinct_trace_ids;
  std::set<std::string> clock_domains;
  projection.records.reserve(expected_record_keys.size());
  for (std::size_t index = 0U; index < expected_record_keys.size(); ++index) {
    RecordProjection record;
    record.record_ordinal = inferlab::internal::checked_add_size(
        index, 1U, "native trace record ordinal");
    record.record_key = expected_record_keys[index];
    record.span_role = span_role_name(domain);
    const auto found = by_key.find(record.record_key);
    if (found != by_key.end()) {
      const auto& input = *found->second;
      record.source_line_number = input.source_line_number;
      record.context_present = true;
      record.parent_span_present = input.parent_span_present;
      record.timestamp_present = true;
      record.correlation_eligible = true;
      record.clock_domain = input.clock_domain;
      const auto normalized = clock_usage.normalize(
          input.trace_id, clock_source_scope, input.clock_domain,
          input.timestamp_ns);
      record.clock_calibration_status = normalized.status;
      record.relative_timestamp_present = normalized.calibrated;
      record.trace_relative_timestamp_ns =
          normalized.trace_relative_timestamp_ns;
      record.clock_uncertainty_ns = normalized.uncertainty_ns;
      record.trace_join_token =
          inferlab_trace::trace_context_join_token(input.trace_id);
      record.span_join_token =
          inferlab_trace::span_context_join_token(input.trace_id,
                                                  input.span_id);
      if (input.parent_span_present) {
        record.parent_span_join_token =
            inferlab_trace::span_context_join_token(input.trace_id,
                                                    input.parent_span_id);
      }
      record.span_lifecycle_supplied = input.span_lifecycle_supplied;
      record.span_lifecycle_contract_ready =
          input.span_lifecycle_supplied;
      record.causal_predecessor_span_present =
          input.causal_predecessor_span_present;
      if (input.span_lifecycle_supplied) {
        record.span_terminal_status = input.span_terminal_status;
        const auto lifecycle_start = lifecycle_clock_usage.normalize(
            input.trace_id, clock_source_scope, input.clock_domain,
            input.span_start_timestamp_ns);
        const auto lifecycle_end = lifecycle_clock_usage.normalize(
            input.trace_id, clock_source_scope, input.clock_domain,
            input.span_end_timestamp_ns);
        if (lifecycle_start.calibrated != lifecycle_end.calibrated ||
            lifecycle_start.status != lifecycle_end.status ||
            lifecycle_start.uncertainty_ns !=
                lifecycle_end.uncertainty_ns ||
            (lifecycle_start.calibrated &&
             lifecycle_start.calibration_entry_ordinal !=
                 lifecycle_end.calibration_entry_ordinal)) {
          throw std::logic_error(
              "native trace span lifecycle calibration bounds disagree");
        }
        record.relative_span_lifecycle_present =
            lifecycle_start.calibrated;
        record.span_lifecycle_status =
            record.relative_span_lifecycle_present
                ? "span_lifecycle_ready"
                : "span_lifecycle_pending_clock_calibration";
        if (record.relative_span_lifecycle_present) {
          if (lifecycle_start.trace_relative_timestamp_ns >
                  lifecycle_end.trace_relative_timestamp_ns ||
              !record.relative_timestamp_present ||
              record.trace_relative_timestamp_ns <
                  lifecycle_start.trace_relative_timestamp_ns ||
              record.trace_relative_timestamp_ns >
                  lifecycle_end.trace_relative_timestamp_ns) {
            throw std::logic_error(
                "native trace calibrated point is outside its span lifecycle");
          }
          record.trace_relative_span_start_ns =
              lifecycle_start.trace_relative_timestamp_ns;
          record.trace_relative_span_end_ns =
              lifecycle_end.trace_relative_timestamp_ns;
          record.span_lifecycle_clock_uncertainty_ns =
              lifecycle_start.uncertainty_ns;
        }
        increment(
            projection.summary.span_lifecycle_supplied_record_count,
            "native trace span lifecycle supplied count");
        increment(
            projection.summary.span_lifecycle_contract_ready_record_count,
            "native trace span lifecycle contract-ready count");
        if (record.relative_span_lifecycle_present) {
          increment(
              projection.summary.span_lifecycle_calibrated_record_count,
              "native trace calibrated span lifecycle count");
        } else {
          increment(
              projection.summary.span_lifecycle_uncalibrated_record_count,
              "native trace uncalibrated span lifecycle count");
        }
      }
      if (input.causal_predecessor_span_present) {
        record.causal_predecessor_span_join_token =
            inferlab_trace::span_context_join_token(
                input.trace_id, input.causal_predecessor_span_id);
        increment(projection.summary.causal_predecessor_span_count,
                  "native trace causal predecessor count");
      }
      increment(projection.summary.context_present_count,
                "native trace context-present count");
      increment(projection.summary.trace_join_token_count,
                "native trace trace-token count");
      increment(projection.summary.span_join_token_count,
                "native trace span-token count");
      if (input.parent_span_present) {
        increment(projection.summary.parent_span_join_token_count,
                  "native trace parent-span-token count");
      }
      distinct_trace_tokens.insert(record.trace_join_token);
      distinct_trace_ids.insert(input.trace_id);
      clock_domains.insert(record.clock_domain);
    } else {
      increment(projection.summary.context_missing_count,
                "native trace context-missing count");
    }
    projection.records.push_back(std::move(record));
  }
  projection.summary.record_count = projection.records.size();
  projection.summary.distinct_trace_join_token_count =
      distinct_trace_tokens.size();
  if (distinct_trace_ids.size() != distinct_trace_tokens.size()) {
    throw std::logic_error(
        "native trace context trace join token collision detected");
  }
  projection.summary.clock_domain_count = clock_domains.size();
  projection.clock_calibration = clock_usage.finalize();
  projection.span_lifecycle_clock_calibration =
      lifecycle_clock_usage.finalize();
  projection.summary.clock_calibrated_record_count =
      projection.clock_calibration.calibrated_timestamp_count;
  projection.summary.clock_uncalibrated_record_count =
      projection.clock_calibration.uncalibrated_timestamp_count;
  projection.summary.clock_normalization_performed =
      projection.summary.clock_calibrated_record_count > 0U;
  projection.summary.span_lifecycle_normalization_performed =
      projection.summary.span_lifecycle_calibrated_record_count > 0U;
  projection.summary.all_contexts_bound =
      projection.summary.context_present_count ==
      projection.summary.record_count;
  projection.summary.decision = expected_decision(projection.summary);
  projection.summary.span_lifecycle_decision =
      expected_span_lifecycle_decision(projection.summary);
  discard_raw_identity_values(source);
  validate_trace_context_alignment(projection, domain,
                                   expected_record_keys);
  return projection;
}

void validate_trace_context_projection(const Projection& projection) {
  const auto& summary = projection.summary;
  if (projection.records.empty() ||
      summary.record_count != projection.records.size() ||
      summary.source_record_count > summary.record_count ||
      summary.source_record_count != summary.context_present_count ||
      inferlab::internal::checked_add_size(
          summary.context_present_count, summary.context_missing_count,
          "native trace context presence partition") !=
          summary.record_count ||
      summary.trace_join_token_count != summary.context_present_count ||
      summary.span_join_token_count != summary.context_present_count ||
      summary.distinct_trace_join_token_count >
          summary.trace_join_token_count ||
      summary.parent_span_join_token_count >
          summary.span_join_token_count ||
      summary.clock_domain_count > summary.context_present_count ||
      summary.clock_calibrated_record_count >
          summary.context_present_count ||
      inferlab::internal::checked_add_size(
          summary.clock_calibrated_record_count,
          summary.clock_uncalibrated_record_count,
          "native trace clock-calibration partition") !=
          summary.context_present_count ||
      summary.clock_calibrated_record_count !=
          projection.clock_calibration.calibrated_timestamp_count ||
      summary.clock_uncalibrated_record_count !=
          projection.clock_calibration.uncalibrated_timestamp_count ||
      projection.clock_calibration.lookup_count !=
          summary.context_present_count ||
      summary.clock_normalization_performed !=
          (summary.clock_calibrated_record_count > 0U) ||
      summary.span_lifecycle_contract_ready_record_count !=
          summary.span_lifecycle_supplied_record_count ||
      inferlab::internal::checked_add_size(
          summary.span_lifecycle_calibrated_record_count,
          summary.span_lifecycle_uncalibrated_record_count,
          "native trace span lifecycle calibration partition") !=
          summary.span_lifecycle_supplied_record_count ||
      summary.span_lifecycle_supplied_record_count >
          summary.context_present_count ||
      summary.causal_predecessor_span_count >
          summary.span_lifecycle_supplied_record_count ||
      projection.span_lifecycle_clock_calibration.lookup_count !=
          inferlab::internal::checked_product_size(
              summary.span_lifecycle_supplied_record_count,
              std::size_t{2U},
              "native trace span lifecycle calibration lookup count") ||
      projection.span_lifecycle_clock_calibration
              .calibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              summary.span_lifecycle_calibrated_record_count,
              std::size_t{2U},
              "native trace calibrated span lifecycle timestamp count") ||
      projection.span_lifecycle_clock_calibration
              .uncalibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              summary.span_lifecycle_uncalibrated_record_count,
              std::size_t{2U},
              "native trace uncalibrated span lifecycle timestamp count") ||
      summary.span_lifecycle_normalization_performed !=
          (summary.span_lifecycle_calibrated_record_count > 0U) ||
      summary.source_supplied != projection.source.supplied ||
      summary.source_contract_ready != projection.source.contract_ready ||
      !projection.source.records.empty() ||
      projection.source.clock_calibration.supplied ||
      !projection.source.clock_calibration.entries.empty() ||
      projection.source.present != summary.source_supplied ||
      projection.source.readable != summary.source_supplied ||
      projection.source.source_path_label.empty() ||
      projection.source.path_probe_status.empty() ||
      projection.source.text_read_status.empty() ||
      (summary.source_supplied && !summary.source_contract_ready) ||
      (!summary.source_supplied &&
       (summary.source_record_count != 0U ||
        projection.source.physical_line_count != 0U ||
        projection.source.path_redacted ||
        projection.source.source_path_label != "not_provided" ||
        projection.source.path_probe_status != "not_configured" ||
        projection.source.text_read_status != "not_configured")) ||
      (summary.source_supplied &&
       (!safe_supplied_source_path_label(projection.source) ||
        projection.source.path_probe_status != "file" ||
        projection.source.text_read_status != "ok" ||
        projection.source.physical_line_count <
            summary.source_record_count ||
        projection.source.physical_line_count >
            kMaximumPhysicalLineCount)) ||
      summary.all_contexts_bound !=
          (summary.context_present_count == summary.record_count) ||
      summary.decision != expected_decision(summary) ||
      summary.span_lifecycle_decision !=
          expected_span_lifecycle_decision(summary) ||
      projection.join_token_algorithm !=
          inferlab_trace::trace_context_join_token_algorithm() ||
      projection.claim_boundary.empty()) {
    throw std::logic_error(
        "native trace context summary is inconsistent");
  }
  inferlab_trace_clock::validate_clock_calibration_usage(
      projection.clock_calibration);
  inferlab_trace_clock::validate_clock_calibration_usage(
      projection.span_lifecycle_clock_calibration);

  std::set<std::string> record_keys;
  std::set<std::string> trace_tokens;
  std::set<std::string> span_tokens;
  std::set<std::string> clock_domains;
  std::size_t rebuilt_present = 0U;
  std::size_t rebuilt_parent = 0U;
  std::size_t rebuilt_calibrated = 0U;
  std::size_t rebuilt_lifecycle_supplied = 0U;
  std::size_t rebuilt_lifecycle_calibrated = 0U;
  std::size_t rebuilt_causal_predecessor = 0U;
  for (std::size_t index = 0U; index < projection.records.size(); ++index) {
    const auto& record = projection.records[index];
    if (record.record_ordinal != inferlab::internal::checked_add_size(
            index, 1U, "native trace reconciled record ordinal") ||
        !valid_trace_record_key(record.record_key) ||
        !record_keys.insert(record.record_key).second ||
        record.span_role != span_role_name(projection.domain)) {
      throw std::logic_error(
          "native trace context record identity is inconsistent");
    }
    if (!record.context_present) {
      if (record.source_line_number != 0U ||
          record.parent_span_present || record.timestamp_present ||
          record.correlation_eligible ||
          record.trace_join_token != "none" ||
          record.span_join_token != "none" ||
          record.parent_span_join_token != "none" ||
          record.clock_domain != "not_provided" ||
          record.clock_calibration_status != "not_applicable" ||
          record.relative_timestamp_present ||
          record.trace_relative_timestamp_ns != 0U ||
          record.clock_uncertainty_ns != 0U ||
          record.span_lifecycle_supplied ||
          record.span_lifecycle_contract_ready ||
          record.relative_span_lifecycle_present ||
          record.causal_predecessor_span_present ||
          record.causal_predecessor_span_join_token != "none" ||
          record.span_terminal_status != "none" ||
          record.span_lifecycle_status != "not_provided" ||
          record.trace_relative_span_start_ns != 0U ||
          record.trace_relative_span_end_ns != 0U ||
          record.span_lifecycle_clock_uncertainty_ns != 0U) {
        throw std::logic_error(
            "missing native trace context record exposes context fields");
      }
      continue;
    }
    if (record.source_line_number == 0U ||
        record.source_line_number > projection.source.physical_line_count ||
        !record.timestamp_present ||
        !record.correlation_eligible ||
        !inferlab_trace::valid_trace_context_join_token(
            record.trace_join_token) ||
        !inferlab_trace::valid_span_context_join_token(
            record.span_join_token) ||
        !supported_clock_domain(record.clock_domain) ||
        (record.relative_timestamp_present !=
         (record.clock_calibration_status ==
          "trace_relative_timestamp_ready")) ||
        (!record.relative_timestamp_present &&
         (record.trace_relative_timestamp_ns != 0U ||
          record.clock_uncertainty_ns != 0U)) ||
        (record.relative_timestamp_present &&
         record.trace_relative_timestamp_ns >
             inferlab_trace_clock::maximum_trace_relative_timestamp_ns()) ||
        (record.relative_timestamp_present &&
         record.clock_uncertainty_ns >
             inferlab_trace_clock::maximum_trace_relative_timestamp_ns()) ||
        (!record.relative_timestamp_present &&
         record.clock_calibration_status !=
             "clock_calibration_not_provided" &&
         record.clock_calibration_status !=
             "clock_calibration_entry_missing") ||
        (record.parent_span_present !=
         (record.parent_span_join_token != "none")) ||
        (record.parent_span_present &&
         !inferlab_trace::valid_span_context_join_token(
             record.parent_span_join_token)) ||
        record.span_lifecycle_contract_ready !=
            record.span_lifecycle_supplied ||
        (!record.span_lifecycle_supplied &&
         (record.relative_span_lifecycle_present ||
          record.causal_predecessor_span_present ||
          record.causal_predecessor_span_join_token != "none" ||
          record.span_terminal_status != "none" ||
          record.span_lifecycle_status != "not_provided" ||
          record.trace_relative_span_start_ns != 0U ||
          record.trace_relative_span_end_ns != 0U ||
          record.span_lifecycle_clock_uncertainty_ns != 0U)) ||
        (record.span_lifecycle_supplied &&
         (!inferlab_trace_lifecycle::valid_terminal_status(
              record.span_terminal_status) ||
          record.span_lifecycle_status !=
              (record.relative_span_lifecycle_present
                   ? "span_lifecycle_ready"
                   : "span_lifecycle_pending_clock_calibration"))) ||
        (record.relative_span_lifecycle_present &&
         (record.trace_relative_span_start_ns >
              record.trace_relative_span_end_ns ||
          record.trace_relative_span_end_ns >
              inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
          record.span_lifecycle_clock_uncertainty_ns >
              inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
          !record.relative_timestamp_present ||
          record.trace_relative_timestamp_ns <
              record.trace_relative_span_start_ns ||
          record.trace_relative_timestamp_ns >
              record.trace_relative_span_end_ns)) ||
        (!record.relative_span_lifecycle_present &&
         (record.trace_relative_span_start_ns != 0U ||
          record.trace_relative_span_end_ns != 0U ||
          record.span_lifecycle_clock_uncertainty_ns != 0U)) ||
        (record.causal_predecessor_span_present !=
         (record.causal_predecessor_span_join_token != "none")) ||
        (record.causal_predecessor_span_present &&
         (!record.span_lifecycle_supplied ||
          !inferlab_trace::valid_span_context_join_token(
              record.causal_predecessor_span_join_token)))) {
      throw std::logic_error(
          "present native trace context record is inconsistent");
    }
    increment(rebuilt_present, "native trace reconciled present count");
    if (record.relative_timestamp_present) {
      increment(rebuilt_calibrated,
                "native trace reconciled calibrated count");
    }
    if (record.parent_span_present) {
      increment(rebuilt_parent,
                "native trace reconciled parent-span count");
    }
    if (record.span_lifecycle_supplied) {
      increment(rebuilt_lifecycle_supplied,
                "native trace reconciled lifecycle-supplied count");
      if (record.relative_span_lifecycle_present) {
        increment(rebuilt_lifecycle_calibrated,
                  "native trace reconciled lifecycle-calibrated count");
      }
    }
    if (record.causal_predecessor_span_present) {
      increment(rebuilt_causal_predecessor,
                "native trace reconciled causal-predecessor count");
    }
    trace_tokens.insert(record.trace_join_token);
    if (!span_tokens.insert(record.span_join_token).second) {
      throw std::logic_error(
          "native trace context span join tokens are not unique");
    }
    clock_domains.insert(record.clock_domain);
  }
  if (rebuilt_present != summary.context_present_count ||
      rebuilt_parent != summary.parent_span_join_token_count ||
      rebuilt_calibrated != summary.clock_calibrated_record_count ||
      rebuilt_lifecycle_supplied !=
          summary.span_lifecycle_supplied_record_count ||
      rebuilt_lifecycle_calibrated !=
          summary.span_lifecycle_calibrated_record_count ||
      rebuilt_causal_predecessor !=
          summary.causal_predecessor_span_count ||
      trace_tokens.size() != summary.distinct_trace_join_token_count ||
      clock_domains.size() != summary.clock_domain_count) {
    throw std::logic_error(
        "native trace context reverse reconciliation failed");
  }
}

void validate_trace_context_alignment(
    const Projection& projection, const Domain expected_domain,
    const std::vector<std::string>& expected_record_keys) {
  validate_expected_record_keys(expected_record_keys);
  validate_trace_context_projection(projection);
  if (projection.domain != expected_domain ||
      projection.records.size() != expected_record_keys.size()) {
    throw std::logic_error(
        "native trace context domain or record cardinality is misaligned");
  }
  for (std::size_t index = 0U; index < expected_record_keys.size(); ++index) {
    if (projection.records[index].record_key != expected_record_keys[index]) {
      throw std::logic_error(
          "native trace context record ordering is misaligned");
    }
  }
}

}  // namespace inferlab_native_trace
