#include "product_replay_request_trace_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "trace_clock_calibration_view.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::product_replay_semantic_events {
namespace {

using inferlab_plan::json_bool;

std::string request_trace_record_json(const RequestTraceRecord& record,
                                      const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "request trace record JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"source_index\": " << record.source_index << ",\n";
  out << nested << "\"request_ordinal\": " << record.request_ordinal
      << ",\n";
  out << nested << "\"request_identity_redacted\": true,\n";
  out << nested << "\"product_profile_id\": "
      << inferlab_plan::json_string(record.product_profile_id) << ",\n";
  out << nested << "\"event_types\": "
      << inferlab_plan::json_string_array(record.event_types, nested_indent)
      << ",\n";
  out << nested << "\"event_membership_count\": "
      << record.event_membership_count << ",\n";
  out << nested << "\"direct_request_event_count\": "
      << record.direct_request_event_count << ",\n";
  out << nested << "\"batch_membership_event_count\": "
      << record.batch_membership_event_count << ",\n";
  out << nested << "\"timestamped_event_count\": "
      << record.timestamped_event_count << ",\n";
  out << nested << "\"same_record_identity_timestamp_count\": "
      << record.same_record_identity_timestamp_count << ",\n";
  out << nested << "\"fixture_event_count\": "
      << record.fixture_event_count << ",\n";
  out << nested << "\"explicit_replay_event_count\": "
      << record.explicit_replay_event_count << ",\n";
  out << nested << "\"replay_ready_event_count\": "
      << record.replay_ready_event_count << ",\n";
  out << nested << "\"comparable_order_pair_count\": "
      << record.comparable_order_pair_count << ",\n";
  out << nested << "\"ordered_pair_count\": "
      << record.ordered_pair_count << ",\n";
  out << nested << "\"duplicate_event_count\": "
      << record.duplicate_event_count << ",\n";
  out << nested << "\"out_of_order_event_count\": "
      << record.out_of_order_event_count << ",\n";
  out << nested << "\"trace_context_event_count\": "
      << record.trace_context_event_count << ",\n";
  out << nested << "\"request_span_context_event_count\": "
      << record.request_span_context_event_count << ",\n";
  out << nested << "\"request_arrival_event_count\": "
      << record.request_arrival_event_count << ",\n";
  out << nested << "\"request_terminal_event_count\": "
      << record.request_terminal_event_count << ",\n";
  out << nested << "\"direct_request_identity_present\": "
      << json_bool(record.direct_request_identity_present) << ",\n";
  out << nested << "\"batch_membership_identity_present\": "
      << json_bool(record.batch_membership_identity_present) << ",\n";
  out << nested << "\"request_arrival_present\": "
      << json_bool(record.request_arrival_present) << ",\n";
  out << nested << "\"request_terminal_present\": "
      << json_bool(record.request_terminal_present) << ",\n";
  out << nested << "\"prefill_start_present\": "
      << json_bool(record.prefill_start_present) << ",\n";
  out << nested << "\"prefill_end_present\": "
      << json_bool(record.prefill_end_present) << ",\n";
  out << nested << "\"decode_step_present\": "
      << json_bool(record.decode_step_present) << ",\n";
  out << nested << "\"kv_transfer_start_present\": "
      << json_bool(record.kv_transfer_start_present) << ",\n";
  out << nested << "\"kv_transfer_end_present\": "
      << json_bool(record.kv_transfer_end_present) << ",\n";
  out << nested << "\"multi_event_join\": "
      << json_bool(record.multi_event_join) << ",\n";
  out << nested << "\"timestamp_complete\": "
      << json_bool(record.timestamp_complete) << ",\n";
  out << nested << "\"ordering_valid\": "
      << json_bool(record.ordering_valid) << ",\n";
  out << nested << "\"duplicate_free\": "
      << json_bool(record.duplicate_free) << ",\n";
  out << nested << "\"trace_context_present\": "
      << json_bool(record.trace_context_present) << ",\n";
  out << nested << "\"trace_context_complete\": "
      << json_bool(record.trace_context_complete) << ",\n";
  out << nested << "\"trace_context_consistent\": "
      << json_bool(record.trace_context_consistent) << ",\n";
  out << nested << "\"trace_correlation_eligible\": "
      << json_bool(record.trace_correlation_eligible) << ",\n";
  out << nested << "\"trace_join_token_present\": "
      << json_bool(record.trace_join_token_present) << ",\n";
  out << nested << "\"trace_join_token\": "
      << inferlab_plan::json_string(record.trace_join_token) << ",\n";
  out << nested << "\"request_span_context_present\": "
      << json_bool(record.request_span_context_present) << ",\n";
  out << nested << "\"request_span_context_complete\": "
      << json_bool(record.request_span_context_complete) << ",\n";
  out << nested << "\"request_span_context_consistent\": "
      << json_bool(record.request_span_context_consistent) << ",\n";
  out << nested << "\"request_span_binding_unique\": "
      << json_bool(record.request_span_binding_unique) << ",\n";
  out << nested << "\"request_span_lineage_eligible\": "
      << json_bool(record.request_span_lineage_eligible) << ",\n";
  out << nested << "\"request_span_join_token_present\": "
      << json_bool(record.request_span_join_token_present) << ",\n";
  out << nested << "\"request_span_join_token\": "
      << inferlab_plan::json_string(record.request_span_join_token)
      << ",\n";
  out << nested << "\"span_lifecycle_supplied\": "
      << json_bool(record.span_lifecycle_supplied) << ",\n";
  out << nested << "\"span_lifecycle_contract_ready\": "
      << json_bool(record.span_lifecycle_contract_ready) << ",\n";
  out << nested << "\"span_lifecycle_interval_ordered\": "
      << json_bool(record.span_lifecycle_interval_ordered) << ",\n";
  out << nested << "\"span_lifecycle_events_contained\": "
      << json_bool(record.span_lifecycle_events_contained) << ",\n";
  out << nested << "\"span_terminal_status\": "
      << inferlab_plan::json_string(record.span_terminal_status) << ",\n";
  out << nested << "\"span_lifecycle_status\": "
      << inferlab_plan::json_string(record.span_lifecycle_status) << ",\n";
  out << nested << "\"clock_domain\": "
      << inferlab_plan::json_string(record.clock_domain) << ",\n";
  out << nested << "\"clock_calibration_eligible\": "
      << json_bool(record.clock_calibration_eligible) << ",\n";
  out << nested << "\"clock_calibration_status\": "
      << inferlab_plan::json_string(record.clock_calibration_status)
      << ",\n";
  out << nested << "\"relative_time_window_present\": "
      << json_bool(record.relative_time_window_present) << ",\n";
  out << nested << "\"trace_relative_window_start_ns\": "
      << record.trace_relative_window_start_ns << ",\n";
  out << nested << "\"trace_relative_window_end_ns\": "
      << record.trace_relative_window_end_ns << ",\n";
  out << nested << "\"clock_uncertainty_ns\": "
      << record.clock_uncertainty_ns << ",\n";
  out << nested << "\"replay_join_ready\": "
      << json_bool(record.replay_join_ready) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record.status) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(record.issues, nested_indent)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string request_trace_records_json(
    const RequestTraceProjection& projection, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "request trace records JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < projection.records.size(); ++index) {
    out << request_trace_record_json(projection.records[index], item_indent)
        << (index + 1U == projection.records.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string request_trace_summary_json(const RequestTraceSummary& summary,
                                       const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "request trace summary JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"schema_valid_event_count\": "
      << summary.schema_valid_event_count << ",\n";
  out << nested << "\"keyed_event_count\": "
      << summary.keyed_event_count << ",\n";
  out << nested << "\"unkeyed_event_count\": "
      << summary.unkeyed_event_count << ",\n";
  out << nested << "\"event_membership_count\": "
      << summary.event_membership_count << ",\n";
  out << nested << "\"direct_request_event_membership_count\": "
      << summary.direct_request_event_membership_count << ",\n";
  out << nested << "\"batch_request_event_membership_count\": "
      << summary.batch_request_event_membership_count << ",\n";
  out << nested << "\"request_count\": " << summary.request_count
      << ",\n";
  out << nested << "\"multi_event_request_count\": "
      << summary.multi_event_request_count << ",\n";
  out << nested << "\"singleton_request_count\": "
      << summary.singleton_request_count << ",\n";
  out << nested << "\"fixture_request_count\": "
      << summary.fixture_request_count << ",\n";
  out << nested << "\"explicit_replay_request_count\": "
      << summary.explicit_replay_request_count << ",\n";
  out << nested << "\"replay_ready_request_count\": "
      << summary.replay_ready_request_count << ",\n";
  out << nested << "\"timestamp_complete_request_count\": "
      << summary.timestamp_complete_request_count << ",\n";
  out << nested << "\"comparable_order_pair_count\": "
      << summary.comparable_order_pair_count << ",\n";
  out << nested << "\"ordered_pair_count\": "
      << summary.ordered_pair_count << ",\n";
  out << nested << "\"duplicate_event_count\": "
      << summary.duplicate_event_count << ",\n";
  out << nested << "\"out_of_order_event_count\": "
      << summary.out_of_order_event_count << ",\n";
  out << nested << "\"trace_context_event_membership_count\": "
      << summary.trace_context_event_membership_count << ",\n";
  out << nested << "\"request_span_context_event_membership_count\": "
      << summary.request_span_context_event_membership_count << ",\n";
  out << nested << "\"trace_context_request_count\": "
      << summary.trace_context_request_count << ",\n";
  out << nested << "\"trace_context_complete_request_count\": "
      << summary.trace_context_complete_request_count << ",\n";
  out << nested << "\"trace_context_consistent_request_count\": "
      << summary.trace_context_consistent_request_count << ",\n";
  out << nested << "\"trace_context_conflict_request_count\": "
      << summary.trace_context_conflict_request_count << ",\n";
  out << nested << "\"trace_correlation_eligible_request_count\": "
      << summary.trace_correlation_eligible_request_count << ",\n";
  out << nested << "\"trace_context_distinct_id_count\": "
      << summary.trace_context_distinct_id_count << ",\n";
  out << nested << "\"trace_join_token_count\": "
      << summary.trace_join_token_count << ",\n";
  out << nested << "\"distinct_trace_join_token_count\": "
      << summary.distinct_trace_join_token_count << ",\n";
  out << nested << "\"trace_join_token_collision_count\": "
      << summary.trace_join_token_collision_count << ",\n";
  out << nested << "\"request_span_context_request_count\": "
      << summary.request_span_context_request_count << ",\n";
  out << nested << "\"request_span_context_complete_request_count\": "
      << summary.request_span_context_complete_request_count << ",\n";
  out << nested << "\"request_span_context_consistent_request_count\": "
      << summary.request_span_context_consistent_request_count << ",\n";
  out << nested << "\"request_span_context_conflict_request_count\": "
      << summary.request_span_context_conflict_request_count << ",\n";
  out << nested << "\"request_span_binding_conflict_request_count\": "
      << summary.request_span_binding_conflict_request_count << ",\n";
  out << nested << "\"request_span_lineage_eligible_request_count\": "
      << summary.request_span_lineage_eligible_request_count << ",\n";
  out << nested << "\"request_span_join_token_count\": "
      << summary.request_span_join_token_count << ",\n";
  out << nested << "\"distinct_request_span_join_token_count\": "
      << summary.distinct_request_span_join_token_count << ",\n";
  out << nested << "\"span_lifecycle_supplied_request_count\": "
      << summary.span_lifecycle_supplied_request_count << ",\n";
  out << nested << "\"span_lifecycle_ready_request_count\": "
      << summary.span_lifecycle_ready_request_count << ",\n";
  out << nested << "\"span_lifecycle_blocked_request_count\": "
      << summary.span_lifecycle_blocked_request_count << ",\n";
  out << nested << "\"span_lifecycle_completed_request_count\": "
      << summary.span_lifecycle_completed_request_count << ",\n";
  out << nested << "\"span_lifecycle_error_request_count\": "
      << summary.span_lifecycle_error_request_count << ",\n";
  out << nested << "\"span_lifecycle_cancelled_request_count\": "
      << summary.span_lifecycle_cancelled_request_count << ",\n";
  out << nested << "\"span_lifecycle_rejected_request_count\": "
      << summary.span_lifecycle_rejected_request_count << ",\n";
  out << nested << "\"clock_calibration_eligible_request_count\": "
      << summary.clock_calibration_eligible_request_count << ",\n";
  out << nested << "\"clock_calibrated_request_count\": "
      << summary.clock_calibrated_request_count << ",\n";
  out << nested << "\"clock_uncalibrated_request_count\": "
      << summary.clock_uncalibrated_request_count << ",\n";
  out << nested << "\"clock_normalization_performed\": "
      << json_bool(summary.clock_normalization_performed) << ",\n";
  out << nested << "\"blocker_count\": " << summary.blocker_count
      << ",\n";
  out << nested << "\"identity_scope\": "
      << inferlab_plan::json_string(summary.identity_scope) << ",\n";
  out << nested << "\"trace_context_scope\": "
      << inferlab_plan::json_string(summary.trace_context_scope) << ",\n";
  out << nested << "\"trace_join_token_algorithm\": "
      << inferlab_plan::json_string(summary.trace_join_token_algorithm)
      << ",\n";
  out << nested << "\"request_span_binding_scope\": "
      << inferlab_plan::json_string(summary.request_span_binding_scope)
      << ",\n";
  out << nested << "\"request_span_join_token_algorithm\": "
      << inferlab_plan::json_string(
             summary.request_span_join_token_algorithm)
      << ",\n";
  out << nested << "\"span_lifecycle_contract\": "
      << inferlab_plan::json_string(summary.span_lifecycle_contract)
      << ",\n";
  out << nested << "\"trace_correlation_fingerprint_algorithm\": "
      << inferlab_plan::json_string(
             summary.trace_correlation_fingerprint_algorithm)
      << ",\n";
  out << nested << "\"trace_correlation_set_fingerprint\": "
      << inferlab_plan::json_string(
             summary.trace_correlation_set_fingerprint)
      << ",\n";
  out << nested << "\"timestamp_unit\": "
      << inferlab_plan::json_string(summary.timestamp_unit) << ",\n";
  out << nested << "\"normalized_timestamp_unit\": "
      << inferlab_plan::json_string(summary.normalized_timestamp_unit)
      << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(summary.decision) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string request_trace_projection_json(
    const RequestTraceProjection& projection, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "request trace projection JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"identity_values_emitted\": false,\n";
  out << nested << "\"raw_identity_values_emitted\": false,\n";
  out << nested << "\"trace_identity_values_emitted\": false,\n";
  out << nested << "\"span_identity_values_emitted\": false,\n";
  out << nested << "\"absolute_timestamp_values_emitted\": false,\n";
  out << nested << "\"relative_timestamp_values_emitted\": "
      << json_bool(projection.summary.clock_calibrated_request_count > 0U)
      << ",\n";
  out << nested << "\"anonymous_trace_join_tokens_emitted\": "
      << json_bool(projection.summary.trace_join_token_count > 0U)
      << ",\n";
  out << nested << "\"anonymous_request_span_join_tokens_emitted\": "
      << json_bool(projection.summary.request_span_join_token_count > 0U)
      << ",\n";
  out << nested << "\"per_request_trace_join_token_supported\": true,\n";
  out << nested << "\"per_request_span_binding_supported\": true,\n";
  out << nested << "\"explicit_request_span_lifecycle_supported\": true,\n";
  out << nested << "\"trace_join_tokens_are_cryptographic\": false,\n";
  out << nested
      << "\"request_span_join_tokens_are_cryptographic\": false,\n";
  out << nested << "\"trace_correlation_set_fingerprint_available\": "
      << json_bool(
             projection.summary.trace_correlation_eligible_request_count >
             0U)
      << ",\n";
  out << nested << "\"trace_context_summary_supported\": true,\n";
  out << nested
      << "\"cross_artifact_trace_id_join_performed\": false,\n";
  out << nested << "\"native_same_request_join_supported\": false,\n";
  out << nested << "\"request_span_binding_emitted_for_downstream_join\": "
      << json_bool(projection.summary.request_span_join_token_count > 0U)
      << ",\n";
  out << nested << "\"trace_id_join_supported\": false,\n";
  out << nested << "\"clock_calibration\": "
      << inferlab_trace_clock::clock_calibration_usage_json(
             projection.clock_calibration, nested_indent)
      << ",\n";
  out << nested << "\"summary\": "
      << request_trace_summary_json(projection.summary, nested_indent)
      << ",\n";
  out << nested << "\"blockers\": "
      << inferlab_plan::json_string_array(projection.blockers, nested_indent)
      << ",\n";
  out << nested << "\"records\": "
      << request_trace_records_json(projection, nested_indent) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(projection.claim_boundary) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab::product_replay_semantic_events
