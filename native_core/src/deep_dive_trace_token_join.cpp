#include "deep_dive_trace_token_join.hpp"

#include "checked_index.hpp"
#include "deep_dive_trace_time_window.hpp"
#include "deep_dive_trace_span_lineage.hpp"
#include "deep_dive_trace_span_lifecycle.hpp"
#include "deep_dive_trace_record_pointer.hpp"
#include "deep_dive_trace_scalar_projection.hpp"
#include "deep_dive_trace_token_join_policy.hpp"
#include "native_trace_context.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_correlation_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_deep_dive_trace {
namespace {

constexpr std::size_t kMaximumProductSourceIndex = 65535U;
constexpr char kProductArtifactType[] = "product_replay_semantic_events";

struct ParsedProductRecord {
  ProductTraceReference reference;
  std::string trace_join_token;
  bool eligible = false;
};
struct ParsedNativeRecord {
  NativeTraceReference reference;
  std::string trace_join_token;
  bool eligible = false;
};
struct InternalJoinGroup {
  std::vector<ProductTraceReference> product_requests;
  std::vector<NativeTraceReference> native_records;
};

#include "deep_dive_trace_clock_calibration_contract_impl.hpp"

std::vector<ParsedProductRecord> parse_product_source(
    const TraceTokenArtifactInput& input,
    TraceTokenSourceObservation& source) {
  std::vector<ParsedProductRecord> records;
  if (!source.structural_ready || !source.document_available) return records;
  source.token_contract_evaluated = true;
  const auto& document = *input.document;
  const auto& index = *input.semantic_index;

  bool identity_values_emitted = true;
  bool raw_identity_values_emitted = true;
  bool trace_identity_values_emitted = true;
  bool span_identity_values_emitted = true;
  bool anonymous_tokens_emitted = false;
  bool token_support = false;
  bool tokens_cryptographic = true;
  bool trace_summary_supported = false;
  bool trace_fingerprint_available = false;
  bool cross_artifact_join_performed = true;
  bool native_same_request_supported = true;
  bool trace_id_join_supported = true;
  bool anonymous_request_span_tokens_emitted = false;
  bool per_request_span_binding_supported = false;
  bool request_span_tokens_cryptographic = true;
  bool request_span_binding_emitted_for_downstream_join = false;
  required_bool(document, index, "identity_values_emitted",
                "/request_trace_join/identity_values_emitted",
                identity_values_emitted, source.issues);
  required_bool(document, index, "raw_identity_values_emitted",
                "/request_trace_join/raw_identity_values_emitted",
                raw_identity_values_emitted, source.issues);
  required_bool(document, index, "trace_identity_values_emitted",
                "/request_trace_join/trace_identity_values_emitted",
                trace_identity_values_emitted, source.issues);
  required_bool(document, index, "span_identity_values_emitted",
                "/request_trace_join/span_identity_values_emitted",
                span_identity_values_emitted,
                source.span_lineage_issues);
  required_bool(document, index, "anonymous_trace_join_tokens_emitted",
                "/request_trace_join/anonymous_trace_join_tokens_emitted",
                anonymous_tokens_emitted, source.issues);
  required_bool(document, index, "per_request_trace_join_token_supported",
                "/request_trace_join/per_request_trace_join_token_supported",
                token_support, source.issues);
  required_bool(document, index, "trace_join_tokens_are_cryptographic",
                "/request_trace_join/trace_join_tokens_are_cryptographic",
                tokens_cryptographic, source.issues);
  required_bool(document, index, "trace_context_summary_supported",
                "/request_trace_join/trace_context_summary_supported",
                trace_summary_supported, source.issues);
  required_bool(
      document, index, "trace_correlation_set_fingerprint_available",
      "/request_trace_join/trace_correlation_set_fingerprint_available",
      trace_fingerprint_available, source.issues);
  required_bool(document, index, "cross_artifact_trace_id_join_performed",
                "/request_trace_join/cross_artifact_trace_id_join_performed",
                cross_artifact_join_performed, source.issues);
  required_bool(document, index, "native_same_request_join_supported",
                "/request_trace_join/native_same_request_join_supported",
                native_same_request_supported, source.issues);
  required_bool(document, index, "trace_id_join_supported",
                "/request_trace_join/trace_id_join_supported",
                trace_id_join_supported, source.issues);
  required_bool(
      document, index, "anonymous_request_span_join_tokens_emitted",
      "/request_trace_join/anonymous_request_span_join_tokens_emitted",
      anonymous_request_span_tokens_emitted,
      source.span_lineage_issues);
  required_bool(document, index, "per_request_span_binding_supported",
                "/request_trace_join/per_request_span_binding_supported",
                per_request_span_binding_supported,
                source.span_lineage_issues);
  required_bool(
      document, index, "request_span_join_tokens_are_cryptographic",
      "/request_trace_join/request_span_join_tokens_are_cryptographic",
      request_span_tokens_cryptographic, source.span_lineage_issues);
  required_bool(
      document, index,
      "request_span_binding_emitted_for_downstream_join",
      "/request_trace_join/request_span_binding_emitted_for_downstream_join",
      request_span_binding_emitted_for_downstream_join,
      source.span_lineage_issues);
  if (identity_values_emitted || raw_identity_values_emitted ||
      trace_identity_values_emitted || !token_support ||
      tokens_cryptographic || !trace_summary_supported ||
      cross_artifact_join_performed || native_same_request_supported) {
    append_unique(source.issues,
                  "product_trace_privacy_or_claim_contract_invalid");
  }
  if (trace_id_join_supported) {
    append_unique(source.issues,
                  "product_trace_legacy_join_claim_contract_invalid");
  }
  if (!per_request_span_binding_supported ||
      request_span_tokens_cryptographic || span_identity_values_emitted) {
    append_unique(source.span_lineage_issues,
                  "product_request_span_capability_contract_invalid");
  }

  parse_clock_calibration_contract(
      document, index, "/request_trace_join/clock_calibration",
      "/request_trace_join/summary/clock_normalization_performed",
      "/request_trace_join/relative_timestamp_values_emitted", source);

  std::string algorithm;
  required_string(document, index, "trace_join_token_algorithm",
                  "/request_trace_join/summary/trace_join_token_algorithm",
                  algorithm, source.issues);
  source.join_algorithm_matches =
      algorithm == inferlab_trace::trace_context_join_token_algorithm();
  if (!source.join_algorithm_matches) {
    append_unique(source.issues, "trace_join_token_algorithm_mismatch");
  }
  required_unsigned(document, index, "request_count",
                    "/request_trace_join/summary/request_count",
                    kMaximumJoinedRecordsPerSource,
                    source.declared_record_count, source.issues);
  required_unsigned(
      document, index, "trace_correlation_eligible_request_count",
      "/request_trace_join/summary/trace_correlation_eligible_request_count",
      kMaximumJoinedRecordsPerSource,
      source.declared_eligible_record_count, source.issues);
  required_unsigned(document, index, "trace_join_token_count",
                    "/request_trace_join/summary/trace_join_token_count",
                    kMaximumJoinedRecordsPerSource,
                    source.declared_trace_join_token_count, source.issues);
  required_unsigned(
      document, index, "distinct_trace_join_token_count",
      "/request_trace_join/summary/distinct_trace_join_token_count",
      kMaximumJoinedRecordsPerSource,
      source.declared_distinct_trace_join_token_count, source.issues);
  std::size_t distinct_trace_id_count = 0U;
  required_unsigned(
      document, index, "trace_context_distinct_id_count",
      "/request_trace_join/summary/trace_context_distinct_id_count",
      kMaximumJoinedRecordsPerSource, distinct_trace_id_count,
      source.issues);
  std::string trace_context_scope;
  required_string(document, index, "trace_context_scope",
                  "/request_trace_join/summary/trace_context_scope",
                  trace_context_scope, source.issues);
  if (trace_context_scope !=
      "w3c_trace_id_anonymous_per_request_tokens_and_set_summary") {
    append_unique(source.issues, "product_trace_join_scope_mismatch");
  }
  std::size_t collision_count = 1U;
  required_unsigned(
      document, index, "trace_join_token_collision_count",
      "/request_trace_join/summary/trace_join_token_collision_count",
      kMaximumJoinedRecordsPerSource, collision_count, source.issues);
  if (collision_count != 0U) {
    append_unique(source.issues,
                  "product_trace_join_token_collision_reported");
  }
  if (anonymous_tokens_emitted !=
          (source.declared_trace_join_token_count > 0U) ||
      trace_fingerprint_available !=
          (source.declared_eligible_record_count > 0U) ||
      distinct_trace_id_count !=
          source.declared_distinct_trace_join_token_count) {
    append_unique(source.issues,
                  "product_trace_summary_capability_contract_invalid");
  }

  std::size_t request_span_lineage_eligible_count = 0U;
  std::size_t distinct_request_span_token_count = 0U;
  std::size_t request_span_binding_conflict_count = 0U;
  required_unsigned(
      document, index, "request_span_lineage_eligible_request_count",
      "/request_trace_join/summary/request_span_lineage_eligible_request_count",
      kMaximumJoinedRecordsPerSource,
      request_span_lineage_eligible_count, source.span_lineage_issues);
  required_unsigned(
      document, index, "request_span_join_token_count",
      "/request_trace_join/summary/request_span_join_token_count",
      kMaximumJoinedRecordsPerSource,
      source.declared_request_span_join_token_count,
      source.span_lineage_issues);
  required_unsigned(
      document, index, "distinct_request_span_join_token_count",
      "/request_trace_join/summary/distinct_request_span_join_token_count",
      kMaximumJoinedRecordsPerSource,
      distinct_request_span_token_count, source.span_lineage_issues);
  required_unsigned(
      document, index, "request_span_binding_conflict_request_count",
      "/request_trace_join/summary/request_span_binding_conflict_request_count",
      kMaximumJoinedRecordsPerSource,
      request_span_binding_conflict_count, source.span_lineage_issues);
  std::string request_span_binding_scope;
  std::string request_span_join_token_algorithm;
  required_string(
      document, index, "request_span_binding_scope",
      "/request_trace_join/summary/request_span_binding_scope",
      request_span_binding_scope, source.span_lineage_issues);
  required_string(
      document, index, "request_span_join_token_algorithm",
      "/request_trace_join/summary/request_span_join_token_algorithm",
      request_span_join_token_algorithm, source.span_lineage_issues);
  if (request_span_lineage_eligible_count !=
          source.declared_request_span_join_token_count ||
      distinct_request_span_token_count !=
          source.declared_request_span_join_token_count ||
      request_span_binding_conflict_count != 0U ||
      anonymous_request_span_tokens_emitted !=
          (source.declared_request_span_join_token_count > 0U) ||
      request_span_binding_emitted_for_downstream_join !=
          (source.declared_request_span_join_token_count > 0U) ||
      request_span_binding_scope !=
          "w3c_trace_id_plus_explicit_request_span_id" ||
      request_span_join_token_algorithm !=
          inferlab_trace::trace_context_join_token_algorithm()) {
    append_unique(source.span_lineage_issues,
                  "product_request_span_summary_contract_invalid");
  }

  const auto source_indices = collect_unsigned_field(
      document, index, kProductRecordPrefix, "source_index",
      kMaximumProductSourceIndex, source.issues);
  const auto request_ordinals = collect_unsigned_field(
      document, index, kProductRecordPrefix, "request_ordinal",
      kMaximumJoinedRecordsPerSource, source.issues);
  const auto product_profiles = collect_string_field(
      document, index, kProductRecordPrefix, "product_profile_id",
      source.issues);
  const auto eligible = collect_bool_field(
      document, index, kProductRecordPrefix,
      "trace_correlation_eligible", source.issues);
  const auto token_present = collect_bool_field(
      document, index, kProductRecordPrefix, "trace_join_token_present",
      source.issues);
  const auto tokens = collect_string_field(
      document, index, kProductRecordPrefix, "trace_join_token",
      source.issues);
  const auto request_span_context_present = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_context_present", source.span_lineage_issues);
  const auto request_span_context_complete = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_context_complete", source.span_lineage_issues);
  const auto request_span_context_consistent = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_context_consistent", source.span_lineage_issues);
  const auto request_span_binding_unique = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_binding_unique", source.span_lineage_issues);
  const auto request_span_lineage_eligible = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_lineage_eligible", source.span_lineage_issues);
  const auto request_span_token_present = collect_bool_field(
      document, index, kProductRecordPrefix,
      "request_span_join_token_present", source.span_lineage_issues);
  const auto request_span_tokens = collect_string_field(
      document, index, kProductRecordPrefix,
      "request_span_join_token", source.span_lineage_issues);
  const auto timestamp_complete = collect_bool_field(
      document, index, kProductRecordPrefix, "timestamp_complete",
      source.issues);
  const auto calibration_eligible = collect_bool_field(
      document, index, kProductRecordPrefix, "clock_calibration_eligible",
      source.issues);
  const auto relative_window_present = collect_bool_field(
      document, index, kProductRecordPrefix,
      "relative_time_window_present", source.issues);
  const auto relative_window_start = collect_unsigned_field(
      document, index, kProductRecordPrefix,
      "trace_relative_window_start_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.issues);
  const auto relative_window_end = collect_unsigned_field(
      document, index, kProductRecordPrefix,
      "trace_relative_window_end_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.issues);
  const auto clock_uncertainty = collect_unsigned_field(
      document, index, kProductRecordPrefix, "clock_uncertainty_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.issues);
  const auto clock_domains = collect_string_field(
      document, index, kProductRecordPrefix, "clock_domain",
      source.issues);
  const auto calibration_statuses = collect_string_field(
      document, index, kProductRecordPrefix, "clock_calibration_status",
      source.issues);
  if (source_indices.size() != source.declared_record_count ||
      request_ordinals.size() != source.declared_record_count ||
      product_profiles.size() != source.declared_record_count ||
      eligible.size() != source.declared_record_count ||
      token_present.size() != source.declared_record_count ||
      tokens.size() != source.declared_record_count ||
      timestamp_complete.size() != source.declared_record_count ||
      calibration_eligible.size() != source.declared_record_count ||
      relative_window_present.size() != source.declared_record_count ||
      relative_window_start.size() != source.declared_record_count ||
      relative_window_end.size() != source.declared_record_count ||
      clock_uncertainty.size() != source.declared_record_count ||
      clock_domains.size() != source.declared_record_count ||
      calibration_statuses.size() != source.declared_record_count) {
    append_unique(source.issues,
                  "product_request_trace_record_field_count_mismatch");
  }
  if (
      request_span_context_present.size() != source.declared_record_count ||
      request_span_context_complete.size() != source.declared_record_count ||
      request_span_context_consistent.size() != source.declared_record_count ||
      request_span_binding_unique.size() != source.declared_record_count ||
      request_span_lineage_eligible.size() != source.declared_record_count ||
      request_span_token_present.size() != source.declared_record_count ||
      request_span_tokens.size() != source.declared_record_count) {
    append_unique(source.span_lineage_issues,
                  "product_request_span_record_field_count_mismatch");
  }

  std::set<std::size_t> request_ordinal_set;
  std::set<std::string> distinct_tokens;
  std::set<std::string> distinct_request_span_tokens;
  std::size_t parsed_calibration_eligible_count = 0U;
  std::size_t parsed_calibrated_request_count = 0U;
  records.reserve(source.declared_record_count);
  for (std::size_t row = 0U; row < source.declared_record_count; ++row) {
    const auto* source_index = find_row_value(
        source_indices, row, "source_index", source.issues);
    const auto* request_ordinal = find_row_value(
        request_ordinals, row, "request_ordinal", source.issues);
    const auto* product_profile = find_row_value(
        product_profiles, row, "product_profile_id", source.issues);
    const auto* is_eligible = find_row_value(
        eligible, row, "trace_correlation_eligible", source.issues);
    const auto* has_token = find_row_value(
        token_present, row, "trace_join_token_present", source.issues);
    const auto* token = find_row_value(
        tokens, row, "trace_join_token", source.issues);
    const auto* has_request_span_context = find_row_value(
        request_span_context_present, row,
        "request_span_context_present", source.span_lineage_issues);
    const auto* has_complete_request_span_context = find_row_value(
        request_span_context_complete, row,
        "request_span_context_complete", source.span_lineage_issues);
    const auto* has_consistent_request_span_context = find_row_value(
        request_span_context_consistent, row,
        "request_span_context_consistent", source.span_lineage_issues);
    const auto* has_unique_request_span_binding = find_row_value(
        request_span_binding_unique, row,
        "request_span_binding_unique", source.span_lineage_issues);
    const auto* is_request_span_lineage_eligible = find_row_value(
        request_span_lineage_eligible, row,
        "request_span_lineage_eligible", source.span_lineage_issues);
    const auto* has_request_span_token = find_row_value(
        request_span_token_present, row,
        "request_span_join_token_present", source.span_lineage_issues);
    const auto* request_span_token = find_row_value(
        request_span_tokens, row,
        "request_span_join_token", source.span_lineage_issues);
    const auto* has_complete_timestamp = find_row_value(
        timestamp_complete, row, "timestamp_complete", source.issues);
    const auto* is_calibration_eligible = find_row_value(
        calibration_eligible, row, "clock_calibration_eligible",
        source.issues);
    const auto* has_relative_window = find_row_value(
        relative_window_present, row, "relative_time_window_present",
        source.issues);
    const auto* window_start = find_row_value(
        relative_window_start, row, "trace_relative_window_start_ns",
        source.issues);
    const auto* window_end = find_row_value(
        relative_window_end, row, "trace_relative_window_end_ns",
        source.issues);
    const auto* uncertainty = find_row_value(
        clock_uncertainty, row, "clock_uncertainty_ns", source.issues);
    const auto* clock_domain = find_row_value(
        clock_domains, row, "clock_domain", source.issues);
    const auto* calibration_status = find_row_value(
        calibration_statuses, row, "clock_calibration_status",
        source.issues);
    if (source_index == nullptr || request_ordinal == nullptr ||
        product_profile == nullptr || is_eligible == nullptr ||
        has_token == nullptr || token == nullptr ||
        has_complete_timestamp == nullptr ||
        is_calibration_eligible == nullptr ||
        has_relative_window == nullptr || window_start == nullptr ||
        window_end == nullptr || uncertainty == nullptr ||
        clock_domain == nullptr || calibration_status == nullptr) {
      continue;
    }
    const bool request_span_fields_present =
        has_request_span_context != nullptr &&
        has_complete_request_span_context != nullptr &&
        has_consistent_request_span_context != nullptr &&
        has_unique_request_span_binding != nullptr &&
        is_request_span_lineage_eligible != nullptr &&
        has_request_span_token != nullptr &&
        request_span_token != nullptr;
    const bool request_span_record_valid =
        request_span_fields_present &&
        (!*has_complete_request_span_context ||
         *has_request_span_context) &&
        (!*is_request_span_lineage_eligible ||
         (*is_eligible && *has_complete_request_span_context &&
          *has_consistent_request_span_context &&
          *has_unique_request_span_binding)) &&
        (*has_request_span_token == *is_request_span_lineage_eligible) &&
        (*has_request_span_token
             ? inferlab_trace::valid_span_context_join_token(
                   *request_span_token)
             : *request_span_token == "none");
    if (!request_span_record_valid) {
      append_unique(source.span_lineage_issues,
                    "product_request_span_record_contract_invalid");
    }
    const std::size_t expected_ordinal =
        inferlab::internal::checked_add_size(
            row, 1U, "product trace expected request ordinal");
    if (*request_ordinal != expected_ordinal ||
        !request_ordinal_set.insert(*request_ordinal).second ||
        product_profile->empty() || product_profile->size() > 256U ||
        (*is_eligible != *has_token) ||
        *clock_domain != inferlab_trace_clock::product_replay_clock_domain() ||
        (*is_calibration_eligible !=
         (*is_eligible && *has_complete_timestamp)) ||
        (*has_relative_window !=
         (*calibration_status == "trace_relative_timestamp_ready")) ||
        (*has_relative_window && !*is_calibration_eligible) ||
        (!*is_calibration_eligible &&
         *calibration_status != "not_applicable") ||
        (*is_calibration_eligible && !*has_relative_window &&
         *calibration_status != "clock_calibration_not_provided" &&
         *calibration_status != "clock_calibration_entry_missing") ||
        (!*has_relative_window &&
         (*window_start != 0U || *window_end != 0U ||
          *uncertainty != 0U)) ||
        (*has_relative_window && *window_start > *window_end) ||
        (*has_relative_window &&
         (!source.clock_calibration_source_supplied ||
          source.clock_calibration_fingerprint == "none")) ||
        (*is_eligible
             ? !inferlab_trace::valid_trace_context_join_token(*token)
             : *token != "none")) {
      append_unique(source.issues,
                    "product_request_trace_record_contract_invalid");
      continue;
    }
    ParsedProductRecord record;
    record.reference.source_index = *source_index;
    record.reference.request_ordinal = *request_ordinal;
    record.reference.product_profile_id = *product_profile;
    record.reference.relative_time_window_present = *has_relative_window;
    record.reference.trace_relative_window_start_ns = *window_start;
    record.reference.trace_relative_window_end_ns = *window_end;
    record.reference.clock_uncertainty_ns = *uncertainty;
    record.reference.clock_calibration_fingerprint =
        *has_relative_window ? source.clock_calibration_fingerprint : "none";
    record.eligible = *is_eligible;
    record.trace_join_token = *token;
    if (request_span_record_valid) {
      record.reference.request_span_binding_present =
          *is_request_span_lineage_eligible;
      record.reference.request_span_join_token = *request_span_token;
      if (*is_request_span_lineage_eligible) {
        source.parsed_request_span_join_token_count =
            inferlab::internal::checked_add_size(
                source.parsed_request_span_join_token_count, 1U,
                "parsed product request-span token count");
        if (!distinct_request_span_tokens.insert(*request_span_token).second) {
          append_unique(source.span_lineage_issues,
                        "product_request_span_token_is_duplicated");
        }
      }
    }
    if (record.eligible) {
      source.parsed_eligible_record_count =
          inferlab::internal::checked_add_size(
              source.parsed_eligible_record_count, 1U,
              "parsed product eligible trace count");
      source.parsed_trace_join_token_count =
          inferlab::internal::checked_add_size(
              source.parsed_trace_join_token_count, 1U,
              "parsed product trace token count");
      distinct_tokens.insert(record.trace_join_token);
    }
    if (*is_calibration_eligible) {
      parsed_calibration_eligible_count =
          inferlab::internal::checked_add_size(
              parsed_calibration_eligible_count, 1U,
              "parsed product clock-calibration-eligible count");
    }
    if (*has_relative_window) {
      parsed_calibrated_request_count =
          inferlab::internal::checked_add_size(
              parsed_calibrated_request_count, 1U,
              "parsed product calibrated request count");
    }
    records.push_back(std::move(record));
  }
  source.parsed_record_count = records.size();
  source.parsed_distinct_trace_join_token_count = distinct_tokens.size();
  source.summary_matches_records =
      source.declared_record_count == source.parsed_record_count &&
      source.declared_eligible_record_count ==
          source.parsed_eligible_record_count &&
      source.declared_trace_join_token_count ==
          source.parsed_trace_join_token_count &&
      source.declared_distinct_trace_join_token_count ==
          source.parsed_distinct_trace_join_token_count &&
      source.declared_eligible_record_count ==
          source.declared_trace_join_token_count &&
      source.declared_clock_calibration_lookup_count ==
          inferlab::internal::checked_product_size(
              parsed_calibration_eligible_count, std::size_t{2U},
              "product clock calibration lookup reconciliation") &&
      source.declared_clock_calibrated_timestamp_count ==
          inferlab::internal::checked_product_size(
              parsed_calibrated_request_count, std::size_t{2U},
              "product calibrated timestamp reconciliation") &&
      source.declared_clock_uncalibrated_timestamp_count ==
          inferlab::internal::checked_product_size(
              parsed_calibration_eligible_count -
                  parsed_calibrated_request_count,
              std::size_t{2U},
              "product uncalibrated timestamp reconciliation");
  if (!source.summary_matches_records) {
    append_unique(source.issues,
                  "product_trace_summary_record_reconciliation_failed");
  }
  source.span_lineage_contract_evaluated = true;
  if (source.declared_request_span_join_token_count != source.parsed_request_span_join_token_count ||
      distinct_request_span_token_count != distinct_request_span_tokens.size() ||
      distinct_request_span_tokens.size() != source.parsed_request_span_join_token_count) {
    append_unique(source.span_lineage_issues,
                  "product_request_span_summary_record_reconciliation_failed");
  }
  source.token_contract_ready = source.issues.empty();
  source.span_lineage_contract_ready =
      source.token_contract_ready && source.span_lineage_issues.empty();
  source.source_decision =
      !source.token_contract_ready
          ? "token_contract_invalid"
          : source.parsed_trace_join_token_count == 0U
                ? "token_contract_ready_without_candidates"
                : "token_contract_ready_with_candidates";
  return records;
}

std::vector<ParsedNativeRecord> parse_native_source(
    const TraceTokenArtifactInput& input,
    TraceTokenSourceObservation& source) {
  std::vector<ParsedNativeRecord> records;
  if (!source.structural_ready || !source.document_available) return records;
  source.token_contract_evaluated = true;
  const auto& document = *input.document;
  const auto& index = *input.semantic_index;

  required_string(document, index, "domain",
                  "/native_trace_context/domain", source.domain,
                  source.issues);
  if (!inferlab_native_trace::valid_trace_domain_name(source.domain)) {
    append_unique(source.issues, "native_trace_domain_invalid");
  }
  std::string algorithm;
  required_string(document, index, "join_token_algorithm",
                  "/native_trace_context/join_token_algorithm", algorithm,
                  source.issues);
  source.join_algorithm_matches =
      algorithm == inferlab_trace::trace_context_join_token_algorithm();
  if (!source.join_algorithm_matches) {
    append_unique(source.issues, "trace_join_token_algorithm_mismatch");
  }
  bool identity_values_emitted = true;
  bool raw_identity_values_emitted = true;
  bool timestamp_values_emitted = true;
  bool anonymous_tokens_emitted = false;
  bool tokens_cryptographic = true;
  bool cross_artifact_join_performed = true;
  bool clock_normalized = true;
  bool parentage_proven = true;
  bool causality_proven = true;
  bool source_contract_ready = false;
  bool source_present = false;
  bool source_readable = false;
  required_bool(document, index, "identity_values_emitted",
                "/native_trace_context/identity_values_emitted",
                identity_values_emitted, source.issues);
  required_bool(document, index, "raw_identity_values_emitted",
                "/native_trace_context/raw_identity_values_emitted",
                raw_identity_values_emitted, source.issues);
  required_bool(document, index, "timestamp_values_emitted",
                "/native_trace_context/timestamp_values_emitted",
                timestamp_values_emitted, source.issues);
  required_bool(document, index, "anonymous_join_tokens_emitted",
                "/native_trace_context/anonymous_join_tokens_emitted",
                anonymous_tokens_emitted, source.issues);
  required_bool(document, index, "join_tokens_are_cryptographic",
                "/native_trace_context/join_tokens_are_cryptographic",
                tokens_cryptographic, source.issues);
  required_bool(document, index, "cross_artifact_join_performed",
                "/native_trace_context/cross_artifact_join_performed",
                cross_artifact_join_performed, source.issues);
  required_bool(document, index, "clock_normalization_performed",
                "/native_trace_context/clock_normalization_performed",
                clock_normalized, source.issues);
  required_bool(document, index, "span_parentage_proven",
                "/native_trace_context/span_parentage_proven",
                parentage_proven, source.issues);
  required_bool(document, index, "causal_relation_proven",
                "/native_trace_context/causal_relation_proven",
                causality_proven, source.issues);
  required_bool(document, index, "source_contract_ready",
                "/native_trace_context/source_contract_ready",
                source_contract_ready, source.issues);
  required_bool(document, index, "source_present",
                "/native_trace_context/source_present", source_present,
                source.issues);
  required_bool(document, index, "source_readable",
                "/native_trace_context/source_readable", source_readable,
                source.issues);
  if (identity_values_emitted || raw_identity_values_emitted ||
      timestamp_values_emitted || tokens_cryptographic ||
      cross_artifact_join_performed || parentage_proven || causality_proven ||
      (source_present != source_readable) ||
      (source_present != source_contract_ready)) {
    append_unique(source.issues,
                  "native_trace_privacy_or_claim_contract_invalid");
  }

  parse_clock_calibration_contract(
      document, index, "/native_trace_context/clock_calibration",
      "/native_trace_context/clock_normalization_performed",
      "/native_trace_context/relative_timestamp_values_emitted", source);
  if (clock_normalized != source.clock_normalization_performed) {
    append_unique(source.issues,
                  "native_trace_clock_normalization_projection_mismatch");
  }

  required_unsigned(document, index, "record_count",
                    "/native_trace_context/summary/record_count",
                    kMaximumJoinedRecordsPerSource,
                    source.declared_record_count, source.issues);
  if (source.declared_record_count == 0U) {
    append_unique(source.issues, "native_trace_record_inventory_empty");
  }
  std::size_t declared_context_present_count = 0U;
  std::size_t declared_context_missing_count = 0U;
  std::size_t declared_clock_calibrated_record_count = 0U;
  std::size_t declared_clock_uncalibrated_record_count = 0U;
  required_unsigned(document, index, "context_present_count",
                    "/native_trace_context/summary/context_present_count",
                    kMaximumJoinedRecordsPerSource,
                    declared_context_present_count, source.issues);
  required_unsigned(document, index, "context_missing_count",
                    "/native_trace_context/summary/context_missing_count",
                    kMaximumJoinedRecordsPerSource,
                    declared_context_missing_count, source.issues);
  required_unsigned(
      document, index, "clock_calibrated_record_count",
      "/native_trace_context/summary/clock_calibrated_record_count",
      kMaximumJoinedRecordsPerSource,
      declared_clock_calibrated_record_count, source.issues);
  required_unsigned(
      document, index, "clock_uncalibrated_record_count",
      "/native_trace_context/summary/clock_uncalibrated_record_count",
      kMaximumJoinedRecordsPerSource,
      declared_clock_uncalibrated_record_count, source.issues);
  required_unsigned(document, index, "trace_join_token_count",
                    "/native_trace_context/summary/trace_join_token_count",
                    kMaximumJoinedRecordsPerSource,
                    source.declared_trace_join_token_count, source.issues);
  source.declared_eligible_record_count =
      source.declared_trace_join_token_count;
  required_unsigned(
      document, index, "distinct_trace_join_token_count",
      "/native_trace_context/summary/distinct_trace_join_token_count",
      kMaximumJoinedRecordsPerSource,
      source.declared_distinct_trace_join_token_count, source.issues);
  std::size_t declared_parent_span_join_token_count = 0U;
  required_unsigned(
      document, index, "span_join_token_count",
      "/native_trace_context/summary/span_join_token_count",
      kMaximumJoinedRecordsPerSource,
      source.declared_span_join_token_count,
      source.span_lineage_issues);
  required_unsigned(
      document, index, "parent_span_join_token_count",
      "/native_trace_context/summary/parent_span_join_token_count",
      kMaximumJoinedRecordsPerSource,
      declared_parent_span_join_token_count,
      source.span_lineage_issues);
  if (source.declared_span_join_token_count !=
          declared_context_present_count ||
      declared_parent_span_join_token_count >
          source.declared_span_join_token_count) {
    append_unique(source.span_lineage_issues,
                  "native_span_lineage_summary_contract_invalid");
  }
  bool declared_all_contexts_bound = false;
  required_bool(document, index, "all_contexts_bound",
                "/native_trace_context/summary/all_contexts_bound",
                declared_all_contexts_bound, source.issues);
  std::string native_trace_decision;
  required_string(document, index, "native_trace_context_decision",
                  "/native_trace_context/summary/native_trace_context_decision",
                  native_trace_decision, source.issues);
  const std::string expected_native_trace_decision =
      !source_present
          ? "not_provided"
          : declared_context_present_count == source.declared_record_count &&
                    source.declared_record_count > 0U
                ? "anonymous_trace_context_ready"
                : "partial_anonymous_trace_context";
  if (declared_context_present_count !=
          source.declared_trace_join_token_count ||
      inferlab::internal::checked_add_size(
          declared_context_present_count, declared_context_missing_count,
          "native trace declared context partition") !=
          source.declared_record_count ||
      declared_all_contexts_bound !=
          (source.declared_record_count > 0U &&
           declared_context_present_count == source.declared_record_count) ||
      native_trace_decision != expected_native_trace_decision ||
      anonymous_tokens_emitted !=
          (source.declared_trace_join_token_count > 0U) ||
      (source.declared_trace_join_token_count > 0U && !source_present)) {
    append_unique(source.issues,
                  "native_trace_summary_capability_contract_invalid");
  }
  if (inferlab::internal::checked_add_size(
          declared_clock_calibrated_record_count,
          declared_clock_uncalibrated_record_count,
          "native trace declared clock partition") !=
          declared_context_present_count ||
      declared_clock_calibrated_record_count !=
          source.declared_clock_calibrated_timestamp_count ||
      declared_clock_uncalibrated_record_count !=
          source.declared_clock_uncalibrated_timestamp_count ||
      source.declared_clock_calibration_lookup_count !=
          declared_context_present_count) {
    append_unique(source.issues,
                  "native_trace_clock_summary_contract_invalid");
  }

  const auto record_ordinals = collect_unsigned_field(
      document, index, kNativeRecordPrefix, "record_ordinal",
      kMaximumJoinedRecordsPerSource, source.issues);
  const auto record_keys = collect_string_field(
      document, index, kNativeRecordPrefix, "record_key", source.issues);
  const auto span_roles = collect_string_field(
      document, index, kNativeRecordPrefix, "span_role", source.issues);
  const auto context_present = collect_bool_field(
      document, index, kNativeRecordPrefix, "context_present",
      source.issues);
  const auto eligible = collect_bool_field(
      document, index, kNativeRecordPrefix, "correlation_eligible",
      source.issues);
  const auto tokens = collect_string_field(
      document, index, kNativeRecordPrefix, "trace_join_token",
      source.issues);
  const auto span_tokens = collect_string_field(
      document, index, kNativeRecordPrefix, "span_join_token",
      source.span_lineage_issues);
  const auto parent_span_present = collect_bool_field(
      document, index, kNativeRecordPrefix, "parent_span_present",
      source.span_lineage_issues);
  const auto parent_span_tokens = collect_string_field(
      document, index, kNativeRecordPrefix, "parent_span_join_token",
      source.span_lineage_issues);
  const auto timestamp_present = collect_bool_field(
      document, index, kNativeRecordPrefix, "timestamp_present",
      source.issues);
  const auto timestamp_value_emitted = collect_bool_field(
      document, index, kNativeRecordPrefix, "timestamp_value_emitted",
      source.issues);
  const auto source_line_numbers = collect_unsigned_field(
      document, index, kNativeRecordPrefix, "source_line_number",
      kMaximumJoinedRecordsPerSource, source.issues);
  const auto relative_timestamp_present = collect_bool_field(
      document, index, kNativeRecordPrefix, "relative_timestamp_present",
      source.issues);
  const auto relative_timestamps = collect_unsigned_field(
      document, index, kNativeRecordPrefix,
      "trace_relative_timestamp_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.issues);
  const auto clock_uncertainties = collect_unsigned_field(
      document, index, kNativeRecordPrefix, "clock_uncertainty_ns",
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns(),
      source.issues);
  const auto calibration_statuses = collect_string_field(
      document, index, kNativeRecordPrefix, "clock_calibration_status",
      source.issues);
  if (record_ordinals.size() != source.declared_record_count ||
      record_keys.size() != source.declared_record_count ||
      span_roles.size() != source.declared_record_count ||
      context_present.size() != source.declared_record_count ||
      eligible.size() != source.declared_record_count ||
      tokens.size() != source.declared_record_count ||
      timestamp_present.size() != source.declared_record_count ||
      timestamp_value_emitted.size() != source.declared_record_count ||
      source_line_numbers.size() != source.declared_record_count ||
      relative_timestamp_present.size() != source.declared_record_count ||
      relative_timestamps.size() != source.declared_record_count ||
      clock_uncertainties.size() != source.declared_record_count ||
      calibration_statuses.size() != source.declared_record_count) {
    append_unique(source.issues,
                  "native_trace_record_field_count_mismatch");
  }
  if (span_tokens.size() != source.declared_record_count ||
      parent_span_present.size() != source.declared_record_count ||
      parent_span_tokens.size() != source.declared_record_count) {
    append_unique(source.span_lineage_issues,
                  "native_span_lineage_record_field_count_mismatch");
  }

  std::set<std::size_t> record_ordinal_set;
  std::set<std::string> record_key_set;
  std::set<std::string> distinct_tokens;
  std::set<std::string> distinct_span_tokens;
  std::size_t parsed_calibrated_record_count = 0U;
  records.reserve(source.declared_record_count);
  for (std::size_t row = 0U; row < source.declared_record_count; ++row) {
    const auto* record_ordinal = find_row_value(
        record_ordinals, row, "record_ordinal", source.issues);
    const auto* record_key = find_row_value(
        record_keys, row, "record_key", source.issues);
    const auto* span_role = find_row_value(
        span_roles, row, "span_role", source.issues);
    const auto* has_context = find_row_value(
        context_present, row, "context_present", source.issues);
    const auto* is_eligible = find_row_value(
        eligible, row, "correlation_eligible", source.issues);
    const auto* token = find_row_value(
        tokens, row, "trace_join_token", source.issues);
    const auto* span_token = find_row_value(
        span_tokens, row, "span_join_token",
        source.span_lineage_issues);
    const auto* has_parent_span = find_row_value(
        parent_span_present, row, "parent_span_present",
        source.span_lineage_issues);
    const auto* parent_span_token = find_row_value(
        parent_span_tokens, row, "parent_span_join_token",
        source.span_lineage_issues);
    const auto* has_timestamp = find_row_value(
        timestamp_present, row, "timestamp_present", source.issues);
    const auto* emitted_timestamp = find_row_value(
        timestamp_value_emitted, row, "timestamp_value_emitted",
        source.issues);
    const auto* source_line_number = find_row_value(
        source_line_numbers, row, "source_line_number", source.issues);
    const auto* has_relative_timestamp = find_row_value(
        relative_timestamp_present, row, "relative_timestamp_present",
        source.issues);
    const auto* relative_timestamp = find_row_value(
        relative_timestamps, row, "trace_relative_timestamp_ns",
        source.issues);
    const auto* uncertainty = find_row_value(
        clock_uncertainties, row, "clock_uncertainty_ns", source.issues);
    const auto* calibration_status = find_row_value(
        calibration_statuses, row, "clock_calibration_status",
        source.issues);
    if (record_ordinal == nullptr || record_key == nullptr ||
        span_role == nullptr || has_context == nullptr ||
        is_eligible == nullptr || token == nullptr ||
        has_timestamp == nullptr || emitted_timestamp == nullptr ||
        source_line_number == nullptr ||
        has_relative_timestamp == nullptr || relative_timestamp == nullptr ||
        uncertainty == nullptr || calibration_status == nullptr) {
      continue;
    }
    const bool span_fields_present = span_token != nullptr &&
                                     has_parent_span != nullptr &&
                                     parent_span_token != nullptr;
    const bool span_record_valid =
        span_fields_present &&
        (*is_eligible
             ? inferlab_trace::valid_span_context_join_token(*span_token)
             : *span_token == "none") &&
        (*has_parent_span
             ? (*is_eligible &&
                inferlab_trace::valid_span_context_join_token(
                    *parent_span_token) &&
                *parent_span_token != *span_token)
             : *parent_span_token == "none");
    if (!span_record_valid) {
      append_unique(source.span_lineage_issues,
                    "native_span_lineage_record_contract_invalid");
    }
    const std::size_t expected_ordinal =
        inferlab::internal::checked_add_size(
            row, 1U, "native trace expected record ordinal");
    if (*record_ordinal != expected_ordinal ||
        !record_ordinal_set.insert(*record_ordinal).second ||
        !inferlab_native_trace::valid_trace_record_key(*record_key) ||
        !record_key_set.insert(*record_key).second ||
        !inferlab_native_trace::trace_domain_span_role_matches(
            source.domain, *span_role) ||
        (*has_context != *is_eligible) ||
        (*has_timestamp != *is_eligible) || *emitted_timestamp ||
        (*has_relative_timestamp !=
         (*calibration_status == "trace_relative_timestamp_ready")) ||
        (*has_relative_timestamp && !*is_eligible) ||
        (!*has_relative_timestamp &&
         (*relative_timestamp != 0U || *uncertainty != 0U)) ||
        (*is_eligible && !*has_relative_timestamp &&
         *calibration_status != "clock_calibration_not_provided" &&
         *calibration_status != "clock_calibration_entry_missing") ||
        (!*is_eligible && *calibration_status != "not_applicable") ||
        (*has_relative_timestamp &&
         (!source.clock_calibration_source_supplied ||
          source.clock_calibration_fingerprint == "none")) ||
        (*is_eligible ? *source_line_number == 0U
                      : *source_line_number != 0U) ||
        (*is_eligible
             ? !inferlab_trace::valid_trace_context_join_token(*token)
             : *token != "none")) {
      append_unique(source.issues,
                    "native_trace_record_contract_invalid");
      continue;
    }
    ParsedNativeRecord record;
    record.reference.artifact = input.artifact;
    record.reference.domain = source.domain;
    record.reference.record_ordinal = *record_ordinal;
    record.reference.record_key = *record_key;
    record.reference.span_role = *span_role;
    record.reference.relative_timestamp_present = *has_relative_timestamp;
    record.reference.trace_relative_timestamp_ns = *relative_timestamp;
    record.reference.clock_uncertainty_ns = *uncertainty;
    record.reference.clock_calibration_fingerprint =
        *has_relative_timestamp ? source.clock_calibration_fingerprint
                                : "none";
    record.eligible = *is_eligible;
    record.trace_join_token = *token;
    if (span_record_valid) {
      record.reference.span_join_token = *span_token;
      record.reference.parent_span_present = *has_parent_span;
      record.reference.parent_span_join_token = *parent_span_token;
      if (*is_eligible) {
        source.parsed_span_join_token_count =
            inferlab::internal::checked_add_size(
                source.parsed_span_join_token_count, 1U,
                "parsed native span-token count");
        if (!distinct_span_tokens.insert(*span_token).second) {
          append_unique(source.span_lineage_issues,
                        "native_span_join_token_is_duplicated");
        }
      }
    }
    if (record.eligible) {
      source.parsed_eligible_record_count =
          inferlab::internal::checked_add_size(
              source.parsed_eligible_record_count, 1U,
              "parsed native eligible trace count");
      source.parsed_trace_join_token_count =
          inferlab::internal::checked_add_size(
              source.parsed_trace_join_token_count, 1U,
              "parsed native trace token count");
      distinct_tokens.insert(record.trace_join_token);
    }
    if (*has_relative_timestamp) {
      parsed_calibrated_record_count =
          inferlab::internal::checked_add_size(
              parsed_calibrated_record_count, 1U,
              "parsed native clock-calibrated record count");
    }
    records.push_back(std::move(record));
  }
  source.parsed_record_count = records.size();
  source.parsed_distinct_trace_join_token_count = distinct_tokens.size();
  source.summary_matches_records =
      source.declared_record_count == source.parsed_record_count &&
      source.declared_eligible_record_count ==
          source.parsed_eligible_record_count &&
      source.declared_trace_join_token_count ==
          source.parsed_trace_join_token_count &&
      source.declared_distinct_trace_join_token_count ==
          source.parsed_distinct_trace_join_token_count &&
      declared_clock_calibrated_record_count ==
          parsed_calibrated_record_count &&
      declared_clock_uncalibrated_record_count ==
          source.parsed_eligible_record_count -
              parsed_calibrated_record_count;
  if (!source.summary_matches_records) {
    append_unique(source.issues,
                  "native_trace_summary_record_reconciliation_failed");
  }
  source.span_lineage_contract_evaluated = true;
  if (source.declared_span_join_token_count != source.parsed_span_join_token_count ||
      distinct_span_tokens.size() != source.parsed_span_join_token_count) {
    append_unique(source.span_lineage_issues,
                  "native_span_lineage_summary_record_reconciliation_failed");
  }
  source.token_contract_ready = source.issues.empty();
  source.span_lineage_contract_ready =
      source.token_contract_ready && source.span_lineage_issues.empty();
  source.source_decision =
      !source.token_contract_ready
          ? "token_contract_invalid"
          : source.parsed_trace_join_token_count == 0U
                ? "token_contract_ready_without_candidates"
                : "token_contract_ready_with_candidates";
  return records;
}

}  // namespace

TraceTokenJoinProjection build_trace_token_join_projection(
    const std::vector<TraceTokenArtifactInput>& inputs) {
  TraceTokenJoinProjection projection;
  projection.join_token_algorithm =
      inferlab_trace::trace_context_join_token_algorithm();
  projection.claim_boundary =
      "Compares canonical non-cryptographic anonymous trace tokens from Product Replay request records and canonical-contract-discovered native serving/deep-dive records, reports strict candidate/unmatched partitions, applies optional shared-calibration time-window filtering, resolves an explicitly declared bounded parent chain to one Product request span, and checks explicitly declared predecessor edges against complete uncertainty-expanded span intervals. A resolved chain is supplied-contract same-request lineage evidence and a declared happens-before result is temporal-consistency evidence only; neither is cryptographic identity, inferred causality, automatic error or cancellation propagation, latency attribution, product execution, benchmark performance, or release readiness.";

  std::set<std::string> input_artifacts;
  std::vector<std::vector<ParsedNativeRecord>> native_records_by_source;
  std::vector<ParsedProductRecord> product_records;
  bool product_source_seen = false;
  for (const auto& input : inputs) {
    if (input.artifact.empty() || input.artifact_type.empty() ||
        !input_artifacts.insert(input.artifact).second) {
      throw std::logic_error(
          "trace-token join input artifact identity is invalid or duplicated");
    }
    const bool product_source = input.artifact_type == kProductArtifactType;
    const bool native_source =
        trace_token_input_contains_metric(
            input, "native_trace_context_decision");
    if (product_source && native_source) {
      throw std::logic_error(
          "trace-token join source cannot be both Product Replay and native");
    }
    if (!product_source && !native_source) continue;

    TraceTokenSourceObservation source;
    initialize_trace_token_source(
        source, input,
        product_source ? TraceTokenSourceRole::ProductReplay
                       : TraceTokenSourceRole::NativeDeepDive);
    if (product_source) {
      if (product_source_seen) {
        throw std::logic_error(
            "trace-token join resolved more than one Product Replay source");
      }
      product_source_seen = true;
      product_records = parse_product_source(input, source);
    } else {
      native_records_by_source.push_back(parse_native_source(input, source));
    }
    projection.sources.push_back(std::move(source));
  }
  if (!product_source_seen || native_records_by_source.empty()) {
    throw std::logic_error(
        "trace-token join canonical source inventory is incomplete");
  }

  projection.summary.source_count = projection.sources.size();
  for (const auto& source : projection.sources) {
    if (source.role == TraceTokenSourceRole::ProductReplay) {
      projection.summary.product_source_count =
          inferlab::internal::checked_add_size(
              projection.summary.product_source_count, 1U,
              "trace-token product source count");
    } else {
      projection.summary.native_source_count =
          inferlab::internal::checked_add_size(
              projection.summary.native_source_count, 1U,
              "trace-token native source count");
    }
    projection.summary.source_contract_issue_count =
        inferlab::internal::checked_add_size(
            projection.summary.source_contract_issue_count,
            source.issues.size(), "trace-token source issue count");
    if (source.token_contract_ready) {
      projection.summary.ready_source_count =
          inferlab::internal::checked_add_size(
              projection.summary.ready_source_count, 1U,
              "trace-token ready source count");
      if (source.role == TraceTokenSourceRole::ProductReplay) {
        projection.summary.product_request_count =
            source.parsed_record_count;
        projection.summary.product_eligible_request_count =
            source.parsed_eligible_record_count;
      } else {
        projection.summary.ready_native_source_count =
            inferlab::internal::checked_add_size(
                projection.summary.ready_native_source_count, 1U,
                "trace-token ready native source count");
        projection.summary.native_record_count =
            inferlab::internal::checked_add_size(
                projection.summary.native_record_count,
                source.parsed_record_count,
                "trace-token native record count");
        projection.summary.native_eligible_record_count =
            inferlab::internal::checked_add_size(
                projection.summary.native_eligible_record_count,
                source.parsed_eligible_record_count,
                "trace-token native eligible record count");
      }
    }
    append_trace_token_source_blockers_and_actions(projection, source);
  }

  std::map<std::string, InternalJoinGroup> groups;
  const auto product_source = std::find_if(
      projection.sources.begin(), projection.sources.end(),
      [](const TraceTokenSourceObservation& source) {
        return source.role == TraceTokenSourceRole::ProductReplay;
      });
  if (product_source != projection.sources.end() &&
      product_source->token_contract_ready) {
    for (const auto& record : product_records) {
      if (record.eligible) {
        groups[record.trace_join_token].product_requests.push_back(
            record.reference);
      }
    }
  }
  std::size_t native_record_source_index = 0U;
  for (const auto& source : projection.sources) {
    if (source.role != TraceTokenSourceRole::NativeDeepDive) continue;
    if (native_record_source_index >= native_records_by_source.size()) {
      throw std::logic_error(
          "trace-token native source projection index drift");
    }
    if (source.token_contract_ready) {
      for (const auto& record :
           native_records_by_source[native_record_source_index]) {
        if (record.eligible) {
          groups[record.trace_join_token].native_records.push_back(
              record.reference);
        }
      }
    }
    native_record_source_index = inferlab::internal::checked_add_size(
        native_record_source_index, 1U,
        "trace-token native source projection index");
  }

  projection.summary.comparison_performed =
      product_source != projection.sources.end() &&
      product_source->token_contract_ready &&
      projection.summary.ready_native_source_count > 0U;
  if (projection.summary.comparison_performed) {
    for (const auto& entry : groups) {
      const auto& group = entry.second;
      if (!group.product_requests.empty() && !group.native_records.empty()) {
        TraceTokenCandidateGroup output;
        output.join_group_ordinal =
            inferlab::internal::checked_add_size(
                projection.matched_groups.size(), 1U,
                "trace-token candidate group ordinal");
        output.product_requests = group.product_requests;
        output.native_records = group.native_records;
        output.one_product_request = output.product_requests.size() == 1U;
        output.same_request_proven = false;
        output.status =
            output.one_product_request
                ? "trace_scope_candidate_match"
                : "ambiguous_trace_scope_multiple_product_requests";
        projection.summary.matched_product_request_count =
            inferlab::internal::checked_add_size(
                projection.summary.matched_product_request_count,
                output.product_requests.size(),
                "trace-token matched product request count");
        projection.summary.matched_native_record_count =
            inferlab::internal::checked_add_size(
                projection.summary.matched_native_record_count,
                output.native_records.size(),
                "trace-token matched native record count");
        if (output.one_product_request) {
          projection.summary
              .single_product_request_matched_trace_group_count =
              inferlab::internal::checked_add_size(
                  projection.summary
                      .single_product_request_matched_trace_group_count,
                  1U, "trace-token unambiguous candidate group count");
        } else {
          projection.summary.multi_product_request_matched_trace_group_count =
              inferlab::internal::checked_add_size(
                  projection.summary
                      .multi_product_request_matched_trace_group_count,
                  1U, "trace-token ambiguous candidate group count");
        }
        projection.matched_groups.push_back(std::move(output));
      } else if (!group.product_requests.empty()) {
        for (const auto& request : group.product_requests) {
          projection.unmatched_product_requests.push_back(
              {request, "no_native_record_with_equal_anonymous_trace_token"});
        }
      } else {
        for (const auto& record : group.native_records) {
          projection.unmatched_native_records.push_back(
              {record, "no_product_request_with_equal_anonymous_trace_token"});
        }
      }
    }
  }
  projection.summary.matched_trace_group_count =
      projection.matched_groups.size();
  projection.summary.unmatched_product_request_count =
      projection.unmatched_product_requests.size();
  projection.summary.unmatched_native_record_count =
      projection.unmatched_native_records.size();
  projection.summary.candidate_join_present =
      !projection.matched_groups.empty();
  evaluate_trace_time_windows(projection);
  evaluate_trace_span_lineage(projection);
  evaluate_trace_span_lifecycle(inputs, projection);

  if (projection.summary.unmatched_product_request_count > 0U) {
    append_unique(projection.blockers,
                  "product_requests_without_native_trace_token_match");
  }
  if (projection.summary.unmatched_native_record_count > 0U) {
    append_unique(projection.blockers,
                  "native_records_without_product_trace_token_match");
  }

  if (projection.summary.comparison_performed &&
      projection.summary.matched_trace_group_count == 0U &&
      projection.summary.product_eligible_request_count > 0U &&
      projection.summary.native_eligible_record_count > 0U) {
    append_unique(projection.blockers, "no_trace_token_intersection");
    append_unique(projection.recommended_actions,
                  "verify_product_and_native_trace_contexts_use_the_same_trace_scope");
  }
  if (projection.summary.multi_product_request_matched_trace_group_count >
      0U) {
    append_unique(projection.blockers,
                  "trace_token_maps_multiple_product_requests_to_one_trace_scope");
    append_unique(projection.recommended_actions,
                  "add_span_or_request_scope_evidence_before_same_request_interpretation");
  }
  projection.summary.blocker_count = projection.blockers.size();
  projection.summary.decision =
      derive_trace_token_join_decision(projection);
  validate_trace_token_join_projection(projection);
  return projection;
}

}  // namespace inferlab_deep_dive_trace
