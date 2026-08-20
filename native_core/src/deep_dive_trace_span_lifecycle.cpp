#include "deep_dive_trace_span_lifecycle.hpp"

#include "checked_index.hpp"
#include "deep_dive_trace_scalar_projection.hpp"
#include "plan_json_utils.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_correlation_contract.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace inferlab_deep_dive_trace {
namespace {

constexpr char kProductArtifactType[] =
    "product_replay_semantic_events";

struct ProductLifecycleRow {
  std::size_t source_index = 0U;
  std::size_t request_ordinal = 0U;
  bool supplied = false;
  bool contract_ready = false;
  bool relative_interval_present = false;
  std::size_t interval_start_ns = 0U;
  std::size_t interval_end_ns = 0U;
  std::size_t uncertainty_ns = 0U;
  std::string terminal_status = "none";
  std::string lifecycle_status = "not_provided";
};

struct NativeLifecycleRow {
  std::size_t record_ordinal = 0U;
  std::string record_key;
  bool supplied = false;
  bool contract_ready = false;
  bool relative_interval_present = false;
  std::size_t interval_start_ns = 0U;
  std::size_t interval_end_ns = 0U;
  std::size_t uncertainty_ns = 0U;
  std::string terminal_status = "none";
  std::string lifecycle_status = "not_provided";
  bool predecessor_present = false;
  std::string predecessor_span_token = "none";
};

struct SpanInterval {
  bool ready = false;
  std::size_t start_ns = 0U;
  std::size_t end_ns = 0U;
  std::size_t uncertainty_ns = 0U;
  std::string calibration_fingerprint = "none";
};

bool scalar_pointer_present(
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, const std::string& pointer) {
  return std::any_of(
      index.fields.begin(), index.fields.end(),
      [&key, &pointer](const auto& occurrence) {
        return occurrence.key == key &&
               occurrence.json_pointer == pointer;
      });
}

bool lifecycle_contract_missing_only(
    const TraceTokenSourceObservation& source) {
  return source.span_lifecycle_issues.size() == 1U &&
         source.span_lifecycle_issues.front() ==
             "span_lifecycle_contract_not_available";
}

bool lifecycle_source_contract_blocked(
    const TraceTokenSourceObservation& source) {
  return source.span_lifecycle_contract_evaluated &&
         !source.span_lifecycle_contract_ready &&
         !lifecycle_contract_missing_only(source);
}

const TraceTokenArtifactInput& require_input(
    const std::vector<TraceTokenArtifactInput>& inputs,
    const std::string& artifact) {
  const TraceTokenArtifactInput* match = nullptr;
  for (const auto& input : inputs) {
    if (input.artifact != artifact) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "span lifecycle resolved duplicate artifact input");
    }
    match = &input;
  }
  if (match == nullptr) {
    throw std::logic_error(
        "span lifecycle source has no matching artifact input");
  }
  return *match;
}

std::vector<ProductLifecycleRow> parse_product_lifecycle(
    const TraceTokenArtifactInput& input,
    TraceTokenSourceObservation& source) {
  std::vector<ProductLifecycleRow> rows;
  source.span_lifecycle_contract_evaluated = true;
  if (!source.structural_ready || !source.document_available) {
    append_unique(source.span_lifecycle_issues,
                  "span_lifecycle_contract_not_available");
    return rows;
  }
  const auto& document = *input.document;
  const auto& index = *input.semantic_index;
  const std::string support_pointer =
      "/request_trace_join/explicit_request_span_lifecycle_supported";
  if (!scalar_pointer_present(index,
                              "explicit_request_span_lifecycle_supported",
                              support_pointer)) {
    append_unique(source.span_lifecycle_issues,
                  "span_lifecycle_contract_not_available");
    return rows;
  }

  bool lifecycle_supported = false;
  required_bool(document, index,
                "explicit_request_span_lifecycle_supported",
                support_pointer, lifecycle_supported,
                source.span_lifecycle_issues);
  std::string lifecycle_contract;
  required_string(
      document, index, "span_lifecycle_contract",
      "/request_trace_join/summary/span_lifecycle_contract",
      lifecycle_contract, source.span_lifecycle_issues);
  std::size_t declared_supplied = 0U;
  std::size_t declared_ready = 0U;
  std::size_t declared_blocked = 0U;
  std::size_t declared_completed = 0U;
  std::size_t declared_error = 0U;
  std::size_t declared_cancelled = 0U;
  std::size_t declared_rejected = 0U;
  required_unsigned(
      document, index, "span_lifecycle_supplied_request_count",
      "/request_trace_join/summary/span_lifecycle_supplied_request_count",
      kMaximumJoinedRecordsPerSource, declared_supplied,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_ready_request_count",
      "/request_trace_join/summary/span_lifecycle_ready_request_count",
      kMaximumJoinedRecordsPerSource, declared_ready,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_blocked_request_count",
      "/request_trace_join/summary/span_lifecycle_blocked_request_count",
      kMaximumJoinedRecordsPerSource, declared_blocked,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_completed_request_count",
      "/request_trace_join/summary/span_lifecycle_completed_request_count",
      kMaximumJoinedRecordsPerSource, declared_completed,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_error_request_count",
      "/request_trace_join/summary/span_lifecycle_error_request_count",
      kMaximumJoinedRecordsPerSource, declared_error,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_cancelled_request_count",
      "/request_trace_join/summary/span_lifecycle_cancelled_request_count",
      kMaximumJoinedRecordsPerSource, declared_cancelled,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_rejected_request_count",
      "/request_trace_join/summary/span_lifecycle_rejected_request_count",
      kMaximumJoinedRecordsPerSource, declared_rejected,
      source.span_lifecycle_issues);
  source.declared_span_lifecycle_record_count = declared_supplied;
  if (!lifecycle_supported ||
      lifecycle_contract !=
          "one_request_arrival_one_request_terminal_all_events_contained" ||
      inferlab::internal::checked_add_size(
          declared_ready, declared_blocked,
          "product lifecycle readiness partition") != declared_supplied ||
      inferlab::internal::checked_sum_size(
          {declared_completed, declared_error, declared_cancelled,
           declared_rejected},
          "product lifecycle terminal status partition") !=
          declared_ready ||
      declared_supplied > source.declared_record_count) {
    append_unique(source.span_lifecycle_issues,
                  "product_span_lifecycle_summary_contract_invalid");
  }

  const auto source_indices = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix, "source_index",
      65535U, source.span_lifecycle_issues);
  const auto request_ordinals = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix, "request_ordinal",
      kMaximumJoinedRecordsPerSource, source.span_lifecycle_issues);
  const auto arrival_counts = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix,
      "request_arrival_event_count", kMaximumJoinedRecordsPerSource,
      source.span_lifecycle_issues);
  const auto terminal_counts = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix,
      "request_terminal_event_count", kMaximumJoinedRecordsPerSource,
      source.span_lifecycle_issues);
  const auto supplied = collect_bool_field(
      document, index, kProductTraceRecordPrefix,
      "span_lifecycle_supplied", source.span_lifecycle_issues);
  const auto ready = collect_bool_field(
      document, index, kProductTraceRecordPrefix,
      "span_lifecycle_contract_ready", source.span_lifecycle_issues);
  const auto terminal_statuses = collect_string_field(
      document, index, kProductTraceRecordPrefix,
      "span_terminal_status", source.span_lifecycle_issues);
  const auto lifecycle_statuses = collect_string_field(
      document, index, kProductTraceRecordPrefix,
      "span_lifecycle_status", source.span_lifecycle_issues);
  const auto relative_present = collect_bool_field(
      document, index, kProductTraceRecordPrefix,
      "relative_time_window_present", source.span_lifecycle_issues);
  const auto interval_starts = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix,
      "trace_relative_window_start_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);
  const auto interval_ends = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix,
      "trace_relative_window_end_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);
  const auto uncertainties = collect_unsigned_field(
      document, index, kProductTraceRecordPrefix,
      "clock_uncertainty_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);

  std::size_t parsed_supplied = 0U;
  std::size_t parsed_ready = 0U;
  std::size_t parsed_blocked = 0U;
  std::size_t parsed_completed = 0U;
  std::size_t parsed_error = 0U;
  std::size_t parsed_cancelled = 0U;
  std::size_t parsed_rejected = 0U;
  rows.reserve(source.declared_record_count);
  for (std::size_t row_index = 0U;
       row_index < source.declared_record_count; ++row_index) {
    const auto* source_index = find_row_value(
        source_indices, row_index, "source_index",
        source.span_lifecycle_issues);
    const auto* request_ordinal = find_row_value(
        request_ordinals, row_index, "request_ordinal",
        source.span_lifecycle_issues);
    const auto* arrival_count = find_row_value(
        arrival_counts, row_index, "request_arrival_event_count",
        source.span_lifecycle_issues);
    const auto* terminal_count = find_row_value(
        terminal_counts, row_index, "request_terminal_event_count",
        source.span_lifecycle_issues);
    const auto* is_supplied = find_row_value(
        supplied, row_index, "span_lifecycle_supplied",
        source.span_lifecycle_issues);
    const auto* is_ready = find_row_value(
        ready, row_index, "span_lifecycle_contract_ready",
        source.span_lifecycle_issues);
    const auto* terminal_status = find_row_value(
        terminal_statuses, row_index, "span_terminal_status",
        source.span_lifecycle_issues);
    const auto* lifecycle_status = find_row_value(
        lifecycle_statuses, row_index, "span_lifecycle_status",
        source.span_lifecycle_issues);
    const auto* has_relative_interval = find_row_value(
        relative_present, row_index, "relative_time_window_present",
        source.span_lifecycle_issues);
    const auto* interval_start = find_row_value(
        interval_starts, row_index, "trace_relative_window_start_ns",
        source.span_lifecycle_issues);
    const auto* interval_end = find_row_value(
        interval_ends, row_index, "trace_relative_window_end_ns",
        source.span_lifecycle_issues);
    const auto* uncertainty = find_row_value(
        uncertainties, row_index, "clock_uncertainty_ns",
        source.span_lifecycle_issues);
    if (source_index == nullptr || request_ordinal == nullptr ||
        arrival_count == nullptr || terminal_count == nullptr ||
        is_supplied == nullptr || is_ready == nullptr ||
        terminal_status == nullptr || lifecycle_status == nullptr ||
        has_relative_interval == nullptr || interval_start == nullptr ||
        interval_end == nullptr || uncertainty == nullptr) {
      continue;
    }

    const bool interval_valid =
        !*has_relative_interval ||
        (*interval_start <= *interval_end &&
         *interval_end <=
             inferlab_trace_clock::maximum_trace_relative_timestamp_ns());
    const bool unsupplied_defaults =
        *is_supplied ||
        (!*is_ready && *arrival_count <= 1U && *terminal_count == 0U &&
         *terminal_status == "none" &&
         *lifecycle_status == "not_provided");
    const bool ready_contract =
        !*is_ready ||
        (*is_supplied && *arrival_count == 1U && *terminal_count == 1U &&
         inferlab_trace_lifecycle::valid_terminal_status(
             *terminal_status) &&
         *lifecycle_status == "span_lifecycle_ready");
    const bool supplied_status =
        !*is_supplied ||
        (*lifecycle_status == "span_lifecycle_ready" ||
         *lifecycle_status == "span_lifecycle_contract_blocked" ||
         *lifecycle_status ==
             "span_lifecycle_request_span_binding_pending");
    const bool supplied_terminal_status =
        !*is_supplied ||
        inferlab_trace_lifecycle::valid_terminal_status(*terminal_status);
    if (!interval_valid || !unsupplied_defaults || !ready_contract ||
        !supplied_status || !supplied_terminal_status ||
        (*is_ready && !*is_supplied)) {
      append_unique(source.span_lifecycle_issues,
                    "product_span_lifecycle_record_contract_invalid");
      continue;
    }
    if (*is_supplied) {
      parsed_supplied = inferlab::internal::checked_add_size(
          parsed_supplied, 1U,
          "parsed product lifecycle supplied count");
      if (*is_ready) {
        parsed_ready = inferlab::internal::checked_add_size(
            parsed_ready, 1U,
            "parsed product lifecycle ready count");
        if (*terminal_status == "completed") {
          parsed_completed = inferlab::internal::checked_add_size(
              parsed_completed, 1U,
              "parsed completed product lifecycle count");
        } else if (*terminal_status == "error") {
          parsed_error = inferlab::internal::checked_add_size(
              parsed_error, 1U,
              "parsed error product lifecycle count");
        } else if (*terminal_status == "cancelled") {
          parsed_cancelled = inferlab::internal::checked_add_size(
              parsed_cancelled, 1U,
              "parsed cancelled product lifecycle count");
        } else if (*terminal_status == "rejected") {
          parsed_rejected = inferlab::internal::checked_add_size(
              parsed_rejected, 1U,
              "parsed rejected product lifecycle count");
        } else {
          throw std::logic_error(
              "validated Product lifecycle has unknown terminal status");
        }
      } else {
        parsed_blocked = inferlab::internal::checked_add_size(
            parsed_blocked, 1U,
            "parsed product lifecycle blocked count");
      }
    }
    rows.push_back({*source_index, *request_ordinal, *is_supplied,
                    *is_ready, *has_relative_interval, *interval_start,
                    *interval_end, *uncertainty, *terminal_status,
                    *lifecycle_status});
  }
  source.parsed_span_lifecycle_record_count = parsed_supplied;
  if (rows.size() != source.declared_record_count ||
      parsed_supplied != declared_supplied ||
      parsed_ready != declared_ready ||
      parsed_blocked != declared_blocked ||
      parsed_completed != declared_completed ||
      parsed_error != declared_error ||
      parsed_cancelled != declared_cancelled ||
      parsed_rejected != declared_rejected) {
    append_unique(source.span_lifecycle_issues,
                  "product_span_lifecycle_summary_record_reconciliation_failed");
  }
  source.span_lifecycle_contract_ready =
      source.span_lineage_contract_ready &&
      source.span_lifecycle_issues.empty();
  return rows;
}

std::vector<NativeLifecycleRow> parse_native_lifecycle(
    const TraceTokenArtifactInput& input,
    TraceTokenSourceObservation& source) {
  std::vector<NativeLifecycleRow> rows;
  source.span_lifecycle_contract_evaluated = true;
  if (!source.structural_ready || !source.document_available) {
    append_unique(source.span_lifecycle_issues,
                  "span_lifecycle_contract_not_available");
    return rows;
  }
  const auto& document = *input.document;
  const auto& index = *input.semantic_index;
  const std::string support_pointer =
      "/native_trace_context/explicit_span_lifecycle_supported";
  if (!scalar_pointer_present(index, "explicit_span_lifecycle_supported",
                              support_pointer)) {
    append_unique(source.span_lifecycle_issues,
                  "span_lifecycle_contract_not_available");
    return rows;
  }

  bool lifecycle_supported = false;
  required_bool(document, index, "explicit_span_lifecycle_supported",
                support_pointer, lifecycle_supported,
                source.span_lifecycle_issues);
  std::size_t declared_supplied = 0U;
  std::size_t declared_ready = 0U;
  std::size_t declared_calibrated = 0U;
  std::size_t declared_uncalibrated = 0U;
  std::size_t declared_predecessors = 0U;
  required_unsigned(
      document, index, "span_lifecycle_supplied_record_count",
      "/native_trace_context/summary/span_lifecycle_supplied_record_count",
      kMaximumJoinedRecordsPerSource, declared_supplied,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_contract_ready_record_count",
      "/native_trace_context/summary/span_lifecycle_contract_ready_record_count",
      kMaximumJoinedRecordsPerSource, declared_ready,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_calibrated_record_count",
      "/native_trace_context/summary/span_lifecycle_calibrated_record_count",
      kMaximumJoinedRecordsPerSource, declared_calibrated,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "span_lifecycle_uncalibrated_record_count",
      "/native_trace_context/summary/span_lifecycle_uncalibrated_record_count",
      kMaximumJoinedRecordsPerSource, declared_uncalibrated,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "causal_predecessor_span_count",
      "/native_trace_context/summary/causal_predecessor_span_count",
      kMaximumJoinedRecordsPerSource, declared_predecessors,
      source.span_lifecycle_issues);
  source.declared_span_lifecycle_record_count = declared_supplied;
  source.declared_causal_predecessor_count = declared_predecessors;
  if (!lifecycle_supported || declared_ready != declared_supplied ||
      inferlab::internal::checked_add_size(
          declared_calibrated, declared_uncalibrated,
          "native lifecycle calibration partition") != declared_supplied ||
      declared_predecessors > declared_supplied ||
      declared_supplied > source.declared_record_count) {
    append_unique(source.span_lifecycle_issues,
                  "native_span_lifecycle_summary_contract_invalid");
  }

  std::size_t lifecycle_lookup_count = 0U;
  std::size_t lifecycle_calibrated_timestamp_count = 0U;
  std::size_t lifecycle_uncalibrated_timestamp_count = 0U;
  std::string lifecycle_calibration_fingerprint;
  std::string lifecycle_fingerprint_algorithm;
  std::string lifecycle_calibration_decision;
  std::string lifecycle_normalized_timestamp_unit;
  std::string lifecycle_calibration_scope;
  bool lifecycle_source_supplied = false;
  bool lifecycle_source_present = false;
  bool lifecycle_source_readable = false;
  bool lifecycle_source_contract_ready = false;
  bool lifecycle_all_lookups_calibrated = false;
  bool lifecycle_raw_trace_identity_emitted = true;
  bool lifecycle_clock_anchor_values_emitted = true;
  bool lifecycle_fingerprint_cryptographic = true;
  bool lifecycle_normalization_performed = false;
  const std::string calibration_pointer =
      "/native_trace_context/span_lifecycle_clock_calibration";
  required_unsigned(
      document, index, "lookup_count", calibration_pointer + "/lookup_count",
      kMaximumProjectedScalarValues, lifecycle_lookup_count,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "calibrated_timestamp_count",
      calibration_pointer + "/calibrated_timestamp_count",
      kMaximumProjectedScalarValues,
      lifecycle_calibrated_timestamp_count,
      source.span_lifecycle_issues);
  required_unsigned(
      document, index, "uncalibrated_timestamp_count",
      calibration_pointer + "/uncalibrated_timestamp_count",
      kMaximumProjectedScalarValues,
      lifecycle_uncalibrated_timestamp_count,
      source.span_lifecycle_issues);
  required_string(
      document, index, "contract_fingerprint",
      calibration_pointer + "/contract_fingerprint",
      lifecycle_calibration_fingerprint, source.span_lifecycle_issues);
  required_string(document, index, "fingerprint_algorithm",
                  calibration_pointer + "/fingerprint_algorithm",
                  lifecycle_fingerprint_algorithm,
                  source.span_lifecycle_issues);
  required_string(document, index, "decision",
                  calibration_pointer + "/decision",
                  lifecycle_calibration_decision,
                  source.span_lifecycle_issues);
  required_string(document, index, "normalized_timestamp_unit",
                  calibration_pointer + "/normalized_timestamp_unit",
                  lifecycle_normalized_timestamp_unit,
                  source.span_lifecycle_issues);
  required_string(document, index, "calibration_scope",
                  calibration_pointer + "/calibration_scope",
                  lifecycle_calibration_scope,
                  source.span_lifecycle_issues);
  required_bool(document, index, "source_supplied",
                calibration_pointer + "/source_supplied",
                lifecycle_source_supplied, source.span_lifecycle_issues);
  required_bool(document, index, "source_present",
                calibration_pointer + "/source_present",
                lifecycle_source_present, source.span_lifecycle_issues);
  required_bool(document, index, "source_readable",
                calibration_pointer + "/source_readable",
                lifecycle_source_readable, source.span_lifecycle_issues);
  required_bool(document, index, "source_contract_ready",
                calibration_pointer + "/source_contract_ready",
                lifecycle_source_contract_ready,
                source.span_lifecycle_issues);
  required_bool(document, index, "all_lookups_calibrated",
                calibration_pointer + "/all_lookups_calibrated",
                lifecycle_all_lookups_calibrated,
                source.span_lifecycle_issues);
  required_bool(document, index, "raw_trace_identity_values_emitted",
                calibration_pointer + "/raw_trace_identity_values_emitted",
                lifecycle_raw_trace_identity_emitted,
                source.span_lifecycle_issues);
  required_bool(document, index, "clock_anchor_values_emitted",
                calibration_pointer + "/clock_anchor_values_emitted",
                lifecycle_clock_anchor_values_emitted,
                source.span_lifecycle_issues);
  required_bool(document, index, "fingerprint_is_cryptographic",
                calibration_pointer + "/fingerprint_is_cryptographic",
                lifecycle_fingerprint_cryptographic,
                source.span_lifecycle_issues);
  required_bool(
      document, index, "span_lifecycle_normalization_performed",
      "/native_trace_context/summary/span_lifecycle_normalization_performed",
      lifecycle_normalization_performed, source.span_lifecycle_issues);
  const bool lifecycle_source_flags_match =
      lifecycle_source_supplied == source.clock_calibration_source_supplied &&
      lifecycle_source_supplied == lifecycle_source_present &&
      lifecycle_source_supplied == lifecycle_source_readable &&
      lifecycle_source_supplied == lifecycle_source_contract_ready;
  const bool lifecycle_fingerprint_matches_source =
      inferlab_trace_clock::valid_clock_calibration_fingerprint(
          lifecycle_calibration_fingerprint) &&
      (lifecycle_source_supplied
           ? lifecycle_calibration_fingerprint != "none"
           : lifecycle_calibration_fingerprint == "none");
  const bool lifecycle_lookup_partition_matches =
      inferlab::internal::checked_add_size(
          lifecycle_calibrated_timestamp_count,
          lifecycle_uncalibrated_timestamp_count,
          "native lifecycle calibration usage partition") ==
      lifecycle_lookup_count;
  if (lifecycle_lookup_count != inferlab::internal::checked_product_size(
          declared_supplied, std::size_t{2U},
          "native lifecycle lookup count") ||
      lifecycle_calibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              declared_calibrated, std::size_t{2U},
              "native lifecycle calibrated timestamp count") ||
      lifecycle_uncalibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              declared_uncalibrated, std::size_t{2U},
              "native lifecycle uncalibrated timestamp count") ||
      lifecycle_calibration_fingerprint !=
          source.clock_calibration_fingerprint ||
      !lifecycle_source_flags_match ||
      !lifecycle_fingerprint_matches_source ||
      !lifecycle_lookup_partition_matches ||
      lifecycle_all_lookups_calibrated !=
          (lifecycle_lookup_count > 0U &&
           lifecycle_calibrated_timestamp_count ==
               lifecycle_lookup_count) ||
      lifecycle_normalization_performed !=
          (declared_calibrated > 0U) ||
      lifecycle_calibration_decision !=
          inferlab_trace_clock::clock_calibration_usage_decision(
              lifecycle_source_supplied, lifecycle_lookup_count,
              lifecycle_calibrated_timestamp_count) ||
      lifecycle_fingerprint_algorithm !=
          inferlab_trace_clock::clock_calibration_fingerprint_algorithm() ||
      lifecycle_normalized_timestamp_unit !=
          "trace_relative_nanoseconds" ||
      lifecycle_calibration_scope !=
          "trace_id_source_scope_clock_domain_affine_anchor_v1" ||
      lifecycle_raw_trace_identity_emitted ||
      lifecycle_clock_anchor_values_emitted ||
      lifecycle_fingerprint_cryptographic) {
    append_unique(source.span_lifecycle_issues,
                  "native_span_lifecycle_calibration_contract_invalid");
  }

  const auto record_ordinals = collect_unsigned_field(
      document, index, kNativeTraceRecordPrefix, "record_ordinal",
      kMaximumJoinedRecordsPerSource, source.span_lifecycle_issues);
  const auto record_keys = collect_string_field(
      document, index, kNativeTraceRecordPrefix, "record_key",
      source.span_lifecycle_issues);
  const auto supplied = collect_bool_field(
      document, index, kNativeTraceRecordPrefix,
      "span_lifecycle_supplied", source.span_lifecycle_issues);
  const auto ready = collect_bool_field(
      document, index, kNativeTraceRecordPrefix,
      "span_lifecycle_contract_ready", source.span_lifecycle_issues);
  const auto relative_present = collect_bool_field(
      document, index, kNativeTraceRecordPrefix,
      "relative_span_lifecycle_present", source.span_lifecycle_issues);
  const auto interval_starts = collect_unsigned_field(
      document, index, kNativeTraceRecordPrefix,
      "trace_relative_span_start_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);
  const auto interval_ends = collect_unsigned_field(
      document, index, kNativeTraceRecordPrefix,
      "trace_relative_span_end_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);
  const auto uncertainties = collect_unsigned_field(
      document, index, kNativeTraceRecordPrefix,
      "span_lifecycle_clock_uncertainty_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.span_lifecycle_issues);
  const auto terminal_statuses = collect_string_field(
      document, index, kNativeTraceRecordPrefix,
      "span_terminal_status", source.span_lifecycle_issues);
  const auto lifecycle_statuses = collect_string_field(
      document, index, kNativeTraceRecordPrefix,
      "span_lifecycle_status", source.span_lifecycle_issues);
  const auto predecessor_present = collect_bool_field(
      document, index, kNativeTraceRecordPrefix,
      "causal_predecessor_span_present", source.span_lifecycle_issues);
  const auto predecessor_tokens = collect_string_field(
      document, index, kNativeTraceRecordPrefix,
      "causal_predecessor_span_join_token",
      source.span_lifecycle_issues);

  std::size_t parsed_supplied = 0U;
  std::size_t parsed_calibrated = 0U;
  std::size_t parsed_predecessors = 0U;
  rows.reserve(source.declared_record_count);
  for (std::size_t row_index = 0U;
       row_index < source.declared_record_count; ++row_index) {
    const auto* record_ordinal = find_row_value(
        record_ordinals, row_index, "record_ordinal",
        source.span_lifecycle_issues);
    const auto* record_key = find_row_value(
        record_keys, row_index, "record_key",
        source.span_lifecycle_issues);
    const auto* is_supplied = find_row_value(
        supplied, row_index, "span_lifecycle_supplied",
        source.span_lifecycle_issues);
    const auto* is_ready = find_row_value(
        ready, row_index, "span_lifecycle_contract_ready",
        source.span_lifecycle_issues);
    const auto* has_relative_interval = find_row_value(
        relative_present, row_index, "relative_span_lifecycle_present",
        source.span_lifecycle_issues);
    const auto* interval_start = find_row_value(
        interval_starts, row_index, "trace_relative_span_start_ns",
        source.span_lifecycle_issues);
    const auto* interval_end = find_row_value(
        interval_ends, row_index, "trace_relative_span_end_ns",
        source.span_lifecycle_issues);
    const auto* uncertainty = find_row_value(
        uncertainties, row_index,
        "span_lifecycle_clock_uncertainty_ns",
        source.span_lifecycle_issues);
    const auto* terminal_status = find_row_value(
        terminal_statuses, row_index, "span_terminal_status",
        source.span_lifecycle_issues);
    const auto* lifecycle_status = find_row_value(
        lifecycle_statuses, row_index, "span_lifecycle_status",
        source.span_lifecycle_issues);
    const auto* has_predecessor = find_row_value(
        predecessor_present, row_index,
        "causal_predecessor_span_present",
        source.span_lifecycle_issues);
    const auto* predecessor_token = find_row_value(
        predecessor_tokens, row_index,
        "causal_predecessor_span_join_token",
        source.span_lifecycle_issues);
    if (record_ordinal == nullptr || record_key == nullptr ||
        is_supplied == nullptr || is_ready == nullptr ||
        has_relative_interval == nullptr || interval_start == nullptr ||
        interval_end == nullptr || uncertainty == nullptr ||
        terminal_status == nullptr || lifecycle_status == nullptr ||
        has_predecessor == nullptr || predecessor_token == nullptr) {
      continue;
    }
    const bool interval_valid =
        !*has_relative_interval ||
        (*interval_start <= *interval_end &&
         *interval_end <=
             inferlab_trace_clock::maximum_trace_relative_timestamp_ns());
    const bool unsupplied_defaults =
        *is_supplied ||
        (!*is_ready && !*has_relative_interval && !*has_predecessor &&
         *terminal_status == "none" &&
         *lifecycle_status == "not_provided" &&
         *interval_start == 0U && *interval_end == 0U &&
         *uncertainty == 0U && *predecessor_token == "none");
    const bool supplied_contract =
        !*is_supplied ||
        (*is_ready &&
         inferlab_trace_lifecycle::valid_terminal_status(
             *terminal_status) &&
         *lifecycle_status ==
             (*has_relative_interval
                  ? "span_lifecycle_ready"
                  : "span_lifecycle_pending_clock_calibration"));
    const bool predecessor_contract =
        *has_predecessor
            ? (*is_supplied &&
               inferlab_trace::valid_span_context_join_token(
                   *predecessor_token))
            : *predecessor_token == "none";
    if (!interval_valid || !unsupplied_defaults || !supplied_contract ||
        !predecessor_contract) {
      append_unique(source.span_lifecycle_issues,
                    "native_span_lifecycle_record_contract_invalid");
      continue;
    }
    if (*is_supplied) {
      parsed_supplied = inferlab::internal::checked_add_size(
          parsed_supplied, 1U,
          "parsed native lifecycle supplied count");
      if (*has_relative_interval) {
        parsed_calibrated = inferlab::internal::checked_add_size(
            parsed_calibrated, 1U,
            "parsed native lifecycle calibrated count");
      }
    }
    if (*has_predecessor) {
      parsed_predecessors = inferlab::internal::checked_add_size(
          parsed_predecessors, 1U,
          "parsed native causal predecessor count");
    }
    rows.push_back({*record_ordinal, *record_key, *is_supplied,
                    *is_ready, *has_relative_interval, *interval_start,
                    *interval_end, *uncertainty, *terminal_status,
                    *lifecycle_status, *has_predecessor,
                    *predecessor_token});
  }
  source.parsed_span_lifecycle_record_count = parsed_supplied;
  source.parsed_causal_predecessor_count = parsed_predecessors;
  if (rows.size() != source.declared_record_count ||
      parsed_supplied != declared_supplied ||
      parsed_supplied != declared_ready ||
      parsed_calibrated != declared_calibrated ||
      parsed_calibrated > parsed_supplied ||
      parsed_supplied - parsed_calibrated != declared_uncalibrated ||
      parsed_predecessors != declared_predecessors) {
    append_unique(source.span_lifecycle_issues,
                  "native_span_lifecycle_summary_record_reconciliation_failed");
  }
  source.span_lifecycle_contract_ready =
      source.span_lineage_contract_ready &&
      source.span_lifecycle_issues.empty();
  return rows;
}

void attach_product_row(const ProductLifecycleRow& row,
                        ProductTraceReference& reference) {
  reference.span_lifecycle_supplied = row.supplied;
  reference.span_lifecycle_contract_ready = row.contract_ready;
  reference.span_terminal_status = row.terminal_status;
  reference.span_lifecycle_status = row.lifecycle_status;
  if (row.relative_interval_present !=
          reference.relative_time_window_present ||
      row.interval_start_ns != reference.trace_relative_window_start_ns ||
      row.interval_end_ns != reference.trace_relative_window_end_ns ||
      row.uncertainty_ns != reference.clock_uncertainty_ns) {
    throw std::logic_error(
        "product lifecycle interval drifted from token projection");
  }
}

void attach_native_row(const NativeLifecycleRow& row,
                       NativeTraceReference& reference) {
  reference.span_lifecycle_supplied = row.supplied;
  reference.span_lifecycle_contract_ready = row.contract_ready;
  reference.relative_span_lifecycle_present =
      row.relative_interval_present;
  reference.trace_relative_span_start_ns = row.interval_start_ns;
  reference.trace_relative_span_end_ns = row.interval_end_ns;
  reference.span_lifecycle_clock_uncertainty_ns = row.uncertainty_ns;
  reference.span_terminal_status = row.terminal_status;
  reference.span_lifecycle_status = row.lifecycle_status;
  reference.causal_predecessor_span_present = row.predecessor_present;
  reference.causal_predecessor_span_join_token =
      row.predecessor_span_token;
}

SpanInterval product_interval(const ProductTraceReference& request) {
  return {request.span_lifecycle_contract_ready &&
              request.relative_time_window_present,
          request.trace_relative_window_start_ns,
          request.trace_relative_window_end_ns,
          request.clock_uncertainty_ns,
          request.clock_calibration_fingerprint};
}

SpanInterval native_interval(const NativeTraceReference& record) {
  return {record.span_lifecycle_contract_ready &&
              record.relative_span_lifecycle_present,
          record.trace_relative_span_start_ns,
          record.trace_relative_span_end_ns,
          record.span_lifecycle_clock_uncertainty_ns,
          record.clock_calibration_fingerprint};
}

TraceSpanLifecycleDecision derive_lifecycle_decision(
    const TraceTokenJoinProjection& projection) {
  if (projection.matched_groups.empty()) {
    return TraceSpanLifecycleDecision::PendingTraceCandidateGroups;
  }
  if (std::any_of(projection.sources.begin(), projection.sources.end(),
                  lifecycle_source_contract_blocked)) {
    return TraceSpanLifecycleDecision::BlockedLifecycleContract;
  }
  if (projection.summary.span_lifecycle_ready_source_count !=
      projection.summary.source_count) {
    return TraceSpanLifecycleDecision::PendingLifecycleEvidence;
  }
  if (projection.summary.declared_causal_edge_count == 0U) {
    return TraceSpanLifecycleDecision::PendingLifecycleEvidence;
  }
  if (projection.summary.conflict_causal_edge_count > 0U) {
    return TraceSpanLifecycleDecision::BlockedDeclaredTemporalOrder;
  }
  if (projection.summary.indeterminate_causal_edge_count > 0U ||
      projection.summary.declared_happens_before_edge_count !=
          projection.summary.declared_causal_edge_count) {
    return TraceSpanLifecycleDecision::PartialDeclaredTemporalEvidence;
  }
  return TraceSpanLifecycleDecision::DeclaredHappensBeforeEvidenceReady;
}

std::string expected_group_lifecycle_status(
    const TraceTokenCandidateGroup& group) {
  if (!group.same_request_proven || group.product_requests.size() != 1U) {
    return "pending_same_request_lineage";
  }
  if (group.declared_causal_edge_count == 0U) {
    return "pending_declared_causal_edges";
  }
  if (group.conflict_causal_edge_count > 0U) {
    return "blocked_declared_temporal_order";
  }
  return group.indeterminate_causal_edge_count > 0U
             ? "partial_declared_temporal_evidence"
             : "declared_happens_before_evidence_ready";
}

bool valid_declared_predecessor_relation(const std::string& relation) {
  return relation == "causal_predecessor_not_in_candidate_group" ||
         relation == "span_lifecycle_or_calibration_pending" ||
         relation ==
             "span_lifecycle_calibration_fingerprint_mismatch" ||
         relation == inferlab_trace_lifecycle::temporal_relation_name(
                         inferlab_trace_lifecycle::TemporalRelation::
                             declared_happens_before) ||
         relation == inferlab_trace_lifecycle::temporal_relation_name(
                         inferlab_trace_lifecycle::TemporalRelation::
                             temporal_order_indeterminate) ||
         relation == inferlab_trace_lifecycle::temporal_relation_name(
                         inferlab_trace_lifecycle::TemporalRelation::
                             temporal_order_reversed);
}

void evaluate_group_lifecycle(TraceTokenCandidateGroup& group,
                              TraceTokenJoinSummary& summary) {
  group.span_lifecycle_evaluated = false;
  group.lifecycle_ready_product_request_count = 0U;
  group.lifecycle_ready_native_record_count = 0U;
  group.declared_causal_edge_count = 0U;
  group.resolved_causal_edge_count = 0U;
  group.declared_happens_before_edge_count = 0U;
  group.indeterminate_causal_edge_count = 0U;
  group.conflict_causal_edge_count = 0U;
  if (!group.same_request_proven || group.product_requests.size() != 1U) {
    group.span_lifecycle_status =
        expected_group_lifecycle_status(group);
    for (auto& record : group.native_records) {
      record.declared_temporal_relation =
          "pending_same_request_lineage";
    }
    return;
  }

  auto& request = group.product_requests.front();
  if (request.span_lifecycle_contract_ready) {
    group.lifecycle_ready_product_request_count = 1U;
  }
  std::map<std::string, SpanInterval> intervals;
  if (request.request_span_binding_present) {
    intervals.emplace(request.request_span_join_token,
                      product_interval(request));
  }
  for (auto& record : group.native_records) {
    if (record.span_lifecycle_contract_ready) {
      group.lifecycle_ready_native_record_count =
          inferlab::internal::checked_add_size(
              group.lifecycle_ready_native_record_count, 1U,
              "group lifecycle-ready native count");
    }
    if (!intervals.emplace(record.span_join_token,
                           native_interval(record)).second) {
      throw std::logic_error(
          "span lifecycle candidate group has duplicate span token");
    }
  }

  for (auto& record : group.native_records) {
    if (!record.causal_predecessor_span_present) {
      record.declared_temporal_relation =
          "causal_predecessor_not_declared";
      continue;
    }
    group.declared_causal_edge_count =
        inferlab::internal::checked_add_size(
            group.declared_causal_edge_count, 1U,
            "group declared causal edge count");
    const auto predecessor = intervals.find(
        record.causal_predecessor_span_join_token);
    if (predecessor == intervals.end()) {
      record.declared_temporal_relation =
          "causal_predecessor_not_in_candidate_group";
      group.indeterminate_causal_edge_count =
          inferlab::internal::checked_add_size(
              group.indeterminate_causal_edge_count, 1U,
              "group unresolved causal edge count");
      continue;
    }
    group.resolved_causal_edge_count =
        inferlab::internal::checked_add_size(
            group.resolved_causal_edge_count, 1U,
            "group resolved causal edge count");
    const SpanInterval successor = native_interval(record);
    if (!predecessor->second.ready || !successor.ready) {
      record.declared_temporal_relation =
          "span_lifecycle_or_calibration_pending";
      group.indeterminate_causal_edge_count =
          inferlab::internal::checked_add_size(
              group.indeterminate_causal_edge_count, 1U,
              "group pending lifecycle causal edge count");
      continue;
    }
    if (predecessor->second.calibration_fingerprint == "none" ||
        predecessor->second.calibration_fingerprint !=
            successor.calibration_fingerprint) {
      record.declared_temporal_relation =
          "span_lifecycle_calibration_fingerprint_mismatch";
      group.indeterminate_causal_edge_count =
          inferlab::internal::checked_add_size(
              group.indeterminate_causal_edge_count, 1U,
              "group calibration-mismatch causal edge count");
      continue;
    }
    const auto relation =
        inferlab_trace_lifecycle::evaluate_declared_temporal_relation(
            predecessor->second.start_ns, predecessor->second.end_ns,
            predecessor->second.uncertainty_ns, successor.start_ns,
            successor.end_ns, successor.uncertainty_ns);
    record.declared_temporal_relation =
        inferlab_trace_lifecycle::temporal_relation_name(relation);
    if (relation ==
        inferlab_trace_lifecycle::TemporalRelation::declared_happens_before) {
      group.declared_happens_before_edge_count =
          inferlab::internal::checked_add_size(
              group.declared_happens_before_edge_count, 1U,
              "group happens-before edge count");
    } else if (relation ==
               inferlab_trace_lifecycle::TemporalRelation::temporal_order_reversed) {
      group.conflict_causal_edge_count =
          inferlab::internal::checked_add_size(
              group.conflict_causal_edge_count, 1U,
              "group reversed causal edge count");
    } else {
      group.indeterminate_causal_edge_count =
          inferlab::internal::checked_add_size(
              group.indeterminate_causal_edge_count, 1U,
              "group indeterminate causal edge count");
    }
  }

  group.span_lifecycle_evaluated =
      group.declared_causal_edge_count > 0U;
  group.span_lifecycle_status = expected_group_lifecycle_status(group);

  summary.lifecycle_ready_product_request_count =
      inferlab::internal::checked_add_size(
          summary.lifecycle_ready_product_request_count,
          group.lifecycle_ready_product_request_count,
          "lifecycle-ready product request total");
  summary.lifecycle_ready_native_record_count =
      inferlab::internal::checked_add_size(
          summary.lifecycle_ready_native_record_count,
          group.lifecycle_ready_native_record_count,
          "lifecycle-ready native record total");
  if (group.span_lifecycle_evaluated) {
    summary.span_lifecycle_evaluated_group_count =
        inferlab::internal::checked_add_size(
            summary.span_lifecycle_evaluated_group_count, 1U,
            "span lifecycle evaluated group total");
  }
  summary.declared_causal_edge_count =
      inferlab::internal::checked_add_size(
          summary.declared_causal_edge_count,
          group.declared_causal_edge_count,
          "declared causal edge total");
  summary.resolved_causal_edge_count =
      inferlab::internal::checked_add_size(
          summary.resolved_causal_edge_count,
          group.resolved_causal_edge_count,
          "resolved causal edge total");
  summary.declared_happens_before_edge_count =
      inferlab::internal::checked_add_size(
          summary.declared_happens_before_edge_count,
          group.declared_happens_before_edge_count,
          "declared happens-before edge total");
  summary.indeterminate_causal_edge_count =
      inferlab::internal::checked_add_size(
          summary.indeterminate_causal_edge_count,
          group.indeterminate_causal_edge_count,
          "indeterminate causal edge total");
  summary.conflict_causal_edge_count =
      inferlab::internal::checked_add_size(
          summary.conflict_causal_edge_count,
          group.conflict_causal_edge_count,
          "conflict causal edge total");
}

}  // namespace

const char* trace_span_lifecycle_decision_name(
    const TraceSpanLifecycleDecision decision) {
  switch (decision) {
    case TraceSpanLifecycleDecision::PendingTraceCandidateGroups:
      return "pending_trace_candidate_groups";
    case TraceSpanLifecycleDecision::PendingLifecycleEvidence:
      return "pending_span_lifecycle_evidence";
    case TraceSpanLifecycleDecision::BlockedLifecycleContract:
      return "blocked_span_lifecycle_contract";
    case TraceSpanLifecycleDecision::BlockedDeclaredTemporalOrder:
      return "blocked_declared_temporal_order";
    case TraceSpanLifecycleDecision::PartialDeclaredTemporalEvidence:
      return "partial_declared_temporal_evidence";
    case TraceSpanLifecycleDecision::DeclaredHappensBeforeEvidenceReady:
      return "declared_happens_before_evidence_ready";
  }
  throw std::invalid_argument("unknown trace span lifecycle decision");
}

void evaluate_trace_span_lifecycle(
    const std::vector<TraceTokenArtifactInput>& inputs,
    TraceTokenJoinProjection& projection) {
  projection.summary.span_lifecycle_source_contract_issue_count = 0U;
  projection.summary.span_lifecycle_ready_source_count = 0U;
  projection.summary.lifecycle_ready_product_request_count = 0U;
  projection.summary.lifecycle_ready_native_record_count = 0U;
  projection.summary.span_lifecycle_evaluated_group_count = 0U;
  projection.summary.declared_causal_edge_count = 0U;
  projection.summary.resolved_causal_edge_count = 0U;
  projection.summary.declared_happens_before_edge_count = 0U;
  projection.summary.indeterminate_causal_edge_count = 0U;
  projection.summary.conflict_causal_edge_count = 0U;
  projection.summary.span_lifecycle_evaluation_performed = false;
  projection.span_lifecycle_limitations.clear();
  projection.span_lifecycle_recommended_actions.clear();

  std::map<std::pair<std::size_t, std::size_t>, ProductLifecycleRow>
      product_rows;
  std::map<std::string,
           std::map<std::pair<std::size_t, std::string>,
                    NativeLifecycleRow>> native_rows;
  for (auto& source : projection.sources) {
    source.span_lifecycle_issues.clear();
    source.span_lifecycle_contract_evaluated = false;
    source.span_lifecycle_contract_ready = false;
    source.declared_span_lifecycle_record_count = 0U;
    source.parsed_span_lifecycle_record_count = 0U;
    source.declared_causal_predecessor_count = 0U;
    source.parsed_causal_predecessor_count = 0U;
    const auto& input = require_input(inputs, source.artifact);
    if (source.role == TraceTokenSourceRole::ProductReplay) {
      if (input.artifact_type != kProductArtifactType) {
        throw std::logic_error(
            "product lifecycle source artifact type drift");
      }
      for (const auto& row : parse_product_lifecycle(input, source)) {
        const auto key = std::make_pair(row.source_index,
                                        row.request_ordinal);
        if (!product_rows.emplace(key, row).second) {
          append_unique(source.span_lifecycle_issues,
                        "duplicate_product_lifecycle_identity");
        }
      }
    } else {
      auto& rows = native_rows[source.artifact];
      for (const auto& row : parse_native_lifecycle(input, source)) {
        const auto key = std::make_pair(row.record_ordinal,
                                        row.record_key);
        if (!rows.emplace(key, row).second) {
          append_unique(source.span_lifecycle_issues,
                        "duplicate_native_lifecycle_identity");
        }
      }
    }
    if (!source.span_lineage_contract_ready &&
        !lifecycle_contract_missing_only(source)) {
      append_unique(source.span_lifecycle_issues,
                    "span_lifecycle_requires_ready_span_lineage_contract");
    }
    source.span_lifecycle_contract_ready =
        source.span_lineage_contract_ready &&
        source.span_lifecycle_issues.empty();
    projection.summary.span_lifecycle_source_contract_issue_count =
        inferlab::internal::checked_add_size(
            projection.summary.span_lifecycle_source_contract_issue_count,
            source.span_lifecycle_issues.size(),
            "span lifecycle source issue total");
    if (source.span_lifecycle_contract_ready) {
      projection.summary.span_lifecycle_ready_source_count =
          inferlab::internal::checked_add_size(
              projection.summary.span_lifecycle_ready_source_count, 1U,
              "span lifecycle ready source total");
    }
    if (lifecycle_contract_missing_only(source)) {
      append_unique(projection.span_lifecycle_limitations,
                    source.artifact +
                        ":span_lifecycle_contract_not_available");
      if (!source.refresh_command.empty()) {
        append_unique(projection.span_lifecycle_recommended_actions,
                      "refresh_lifecycle_capable_artifact:" +
                          source.artifact);
      }
    } else if (lifecycle_source_contract_blocked(source)) {
      append_unique(projection.span_lifecycle_limitations,
                    source.artifact +
                        ":span_lifecycle_contract_blocked");
      append_unique(projection.span_lifecycle_recommended_actions,
                    "repair_span_lifecycle_contract:" +
                        source.artifact);
    }
  }

  const auto attach_product = [&product_rows](
                                  ProductTraceReference& reference) {
    const auto found = product_rows.find(
        {reference.source_index, reference.request_ordinal});
    if (found == product_rows.end()) return;
    attach_product_row(found->second, reference);
  };
  const auto attach_native = [&native_rows](
                                 NativeTraceReference& reference) {
    const auto artifact = native_rows.find(reference.artifact);
    if (artifact == native_rows.end()) return;
    const auto found = artifact->second.find(
        {reference.record_ordinal, reference.record_key});
    if (found == artifact->second.end()) return;
    attach_native_row(found->second, reference);
  };
  for (auto& group : projection.matched_groups) {
    for (auto& request : group.product_requests) attach_product(request);
    for (auto& record : group.native_records) attach_native(record);
  }
  for (auto& unmatched : projection.unmatched_product_requests) {
    attach_product(unmatched.request);
  }
  for (auto& unmatched : projection.unmatched_native_records) {
    attach_native(unmatched.record);
  }

  for (auto& group : projection.matched_groups) {
    evaluate_group_lifecycle(group, projection.summary);
  }
  projection.summary.span_lifecycle_evaluation_performed =
      projection.summary.span_lifecycle_evaluated_group_count > 0U;
  projection.summary.span_lifecycle_decision =
      derive_lifecycle_decision(projection);
  if (projection.summary.indeterminate_causal_edge_count > 0U) {
    append_unique(projection.span_lifecycle_limitations,
                  "declared_causal_edges_lack_comparable_complete_span_intervals");
    append_unique(projection.span_lifecycle_recommended_actions,
                  "supply_complete_span_lifecycle_and_shared_clock_calibration");
  }
  if (projection.summary.conflict_causal_edge_count > 0U) {
    append_unique(projection.span_lifecycle_limitations,
                  "declared_causal_edges_conflict_with_temporal_order");
    append_unique(projection.span_lifecycle_recommended_actions,
                  "repair_reversed_span_intervals_or_predecessor_binding");
  }
  if (projection.summary.declared_causal_edge_count == 0U) {
    append_unique(projection.span_lifecycle_limitations,
                  "no_declared_cross_span_causal_predecessor_edges");
    append_unique(projection.span_lifecycle_recommended_actions,
                  "add_explicit_causal_predecessor_span_id_to_native_trace_sidecar");
  }
  validate_trace_span_lifecycle_projection(projection);
}

void validate_trace_span_lifecycle_projection(
    const TraceTokenJoinProjection& projection) {
  std::size_t source_issue_count = 0U;
  std::size_t ready_source_count = 0U;
  for (const auto& source : projection.sources) {
    if (!source.span_lifecycle_contract_evaluated ||
        source.parsed_span_lifecycle_record_count >
            source.declared_span_lifecycle_record_count ||
        source.parsed_causal_predecessor_count >
            source.declared_causal_predecessor_count ||
        source.span_lifecycle_contract_ready !=
            (source.span_lineage_contract_ready &&
             source.span_lifecycle_issues.empty())) {
      throw std::logic_error(
          "trace span lifecycle source contract drift");
    }
    source_issue_count = inferlab::internal::checked_add_size(
        source_issue_count, source.span_lifecycle_issues.size(),
        "reconciled span lifecycle source issue count");
    if (source.span_lifecycle_contract_ready) {
      ready_source_count = inferlab::internal::checked_add_size(
          ready_source_count, 1U,
          "reconciled span lifecycle ready source count");
    }
  }

  TraceTokenJoinSummary rebuilt;
  for (const auto& group : projection.matched_groups) {
    if (group.resolved_causal_edge_count >
            group.declared_causal_edge_count ||
        group.lifecycle_ready_product_request_count >
            group.product_requests.size() ||
        group.lifecycle_ready_native_record_count >
            group.native_records.size() ||
        inferlab::internal::checked_sum_size(
            {group.declared_happens_before_edge_count,
             group.indeterminate_causal_edge_count,
             group.conflict_causal_edge_count},
            "group temporal relation partition") !=
            group.declared_causal_edge_count ||
        inferlab::internal::checked_add_size(
            group.declared_happens_before_edge_count,
            group.conflict_causal_edge_count,
            "group resolved temporal relation lower bound") >
            group.resolved_causal_edge_count ||
        group.span_lifecycle_evaluated !=
            (group.declared_causal_edge_count > 0U &&
             group.same_request_proven) ||
        group.span_lifecycle_status !=
            expected_group_lifecycle_status(group)) {
      throw std::logic_error(
          "trace span lifecycle group count drift");
    }
    if (group.same_request_proven) {
      rebuilt.lifecycle_ready_product_request_count =
          inferlab::internal::checked_add_size(
              rebuilt.lifecycle_ready_product_request_count,
              group.lifecycle_ready_product_request_count,
              "reconciled lifecycle-ready product count");
      rebuilt.lifecycle_ready_native_record_count =
          inferlab::internal::checked_add_size(
              rebuilt.lifecycle_ready_native_record_count,
              group.lifecycle_ready_native_record_count,
              "reconciled lifecycle-ready native count");
      if (group.span_lifecycle_evaluated) {
        rebuilt.span_lifecycle_evaluated_group_count =
            inferlab::internal::checked_add_size(
                rebuilt.span_lifecycle_evaluated_group_count, 1U,
                "reconciled lifecycle evaluated group count");
      }
      rebuilt.declared_causal_edge_count =
          inferlab::internal::checked_add_size(
              rebuilt.declared_causal_edge_count,
              group.declared_causal_edge_count,
              "reconciled declared causal edge count");
      rebuilt.resolved_causal_edge_count =
          inferlab::internal::checked_add_size(
              rebuilt.resolved_causal_edge_count,
              group.resolved_causal_edge_count,
              "reconciled resolved causal edge count");
      rebuilt.declared_happens_before_edge_count =
          inferlab::internal::checked_add_size(
              rebuilt.declared_happens_before_edge_count,
              group.declared_happens_before_edge_count,
              "reconciled happens-before edge count");
      rebuilt.indeterminate_causal_edge_count =
          inferlab::internal::checked_add_size(
              rebuilt.indeterminate_causal_edge_count,
              group.indeterminate_causal_edge_count,
              "reconciled indeterminate causal edge count");
      rebuilt.conflict_causal_edge_count =
          inferlab::internal::checked_add_size(
              rebuilt.conflict_causal_edge_count,
              group.conflict_causal_edge_count,
              "reconciled conflict causal edge count");
    }
    for (const auto& request : group.product_requests) {
      const bool supplied_contract =
          !request.span_lifecycle_supplied ||
          (inferlab_trace_lifecycle::valid_terminal_status(
               request.span_terminal_status) &&
           (request.span_lifecycle_contract_ready
                ? (request.request_span_binding_present &&
                   request.span_lifecycle_status ==
                       "span_lifecycle_ready")
                : (request.span_lifecycle_status ==
                       "span_lifecycle_contract_blocked" ||
                   request.span_lifecycle_status ==
                       "span_lifecycle_request_span_binding_pending")));
      if (!supplied_contract ||
          (!request.span_lifecycle_supplied &&
           (request.span_lifecycle_contract_ready ||
            request.span_terminal_status != "none" ||
            request.span_lifecycle_status != "not_provided"))) {
        throw std::logic_error(
            "product span lifecycle reference contract drift");
      }
    }
    for (const auto& record : group.native_records) {
      if (record.causal_predecessor_span_present !=
              (record.causal_predecessor_span_join_token != "none") ||
          record.span_lifecycle_contract_ready !=
              record.span_lifecycle_supplied ||
          (!record.span_lifecycle_supplied &&
           (record.relative_span_lifecycle_present ||
            record.span_terminal_status != "none" ||
            record.span_lifecycle_status != "not_provided" ||
            record.causal_predecessor_span_present ||
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
                inferlab_trace_clock::maximum_trace_relative_timestamp_ns())) ||
          (!record.relative_span_lifecycle_present &&
           (record.trace_relative_span_start_ns != 0U ||
            record.trace_relative_span_end_ns != 0U ||
            record.span_lifecycle_clock_uncertainty_ns != 0U)) ||
          (record.causal_predecessor_span_present &&
           (!record.span_lifecycle_supplied ||
            !inferlab_trace::valid_span_context_join_token(
                record.causal_predecessor_span_join_token))) ||
          (!group.same_request_proven &&
           record.declared_temporal_relation !=
               "pending_same_request_lineage") ||
          (group.same_request_proven &&
           !record.causal_predecessor_span_present &&
           record.declared_temporal_relation !=
               "causal_predecessor_not_declared") ||
          (group.same_request_proven &&
           record.causal_predecessor_span_present &&
           !valid_declared_predecessor_relation(
               record.declared_temporal_relation))) {
        throw std::logic_error(
            "native span lifecycle reference contract drift");
      }
    }
  }
  const auto& summary = projection.summary;
  if (source_issue_count !=
          summary.span_lifecycle_source_contract_issue_count ||
      ready_source_count != summary.span_lifecycle_ready_source_count ||
      rebuilt.lifecycle_ready_product_request_count !=
          summary.lifecycle_ready_product_request_count ||
      rebuilt.lifecycle_ready_native_record_count !=
          summary.lifecycle_ready_native_record_count ||
      rebuilt.span_lifecycle_evaluated_group_count !=
          summary.span_lifecycle_evaluated_group_count ||
      rebuilt.declared_causal_edge_count !=
          summary.declared_causal_edge_count ||
      rebuilt.resolved_causal_edge_count !=
          summary.resolved_causal_edge_count ||
      rebuilt.declared_happens_before_edge_count !=
          summary.declared_happens_before_edge_count ||
      rebuilt.indeterminate_causal_edge_count !=
          summary.indeterminate_causal_edge_count ||
      rebuilt.conflict_causal_edge_count !=
          summary.conflict_causal_edge_count ||
      summary.span_lifecycle_evaluation_performed !=
          (summary.span_lifecycle_evaluated_group_count > 0U) ||
      summary.span_lifecycle_decision !=
          derive_lifecycle_decision(projection)) {
    throw std::logic_error(
        "trace span lifecycle summary reconciliation failed");
  }
}

}  // namespace inferlab_deep_dive_trace
