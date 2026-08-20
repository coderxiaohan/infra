#include "deep_dive_trace_token_join_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab_deep_dive_trace {
namespace {

std::string product_request_json(const ProductTraceReference& request,
                                 const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token product request JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"product_profile_id\": "
      << inferlab_plan::json_string(request.product_profile_id) << ",\n";
  out << nested << "\"request_identity_redacted\": true,\n";
  out << nested << "\"request_ordinal\": " << request.request_ordinal
      << ",\n";
  out << nested << "\"source_index\": " << request.source_index
      << ",\n";
  out << nested << "\"request_span_binding_present\": "
      << inferlab_plan::json_bool(request.request_span_binding_present)
      << ",\n";
  out << nested << "\"request_span_join_token_emitted\": false,\n";
  out << nested << "\"span_lifecycle_supplied\": "
      << inferlab_plan::json_bool(request.span_lifecycle_supplied)
      << ",\n";
  out << nested << "\"span_lifecycle_contract_ready\": "
      << inferlab_plan::json_bool(request.span_lifecycle_contract_ready)
      << ",\n";
  out << nested << "\"span_lifecycle_status\": "
      << inferlab_plan::json_string(request.span_lifecycle_status) << ",\n";
  out << nested << "\"span_terminal_status\": "
      << inferlab_plan::json_string(request.span_terminal_status) << ",\n";
  out << nested << "\"relative_time_window_present\": "
      << inferlab_plan::json_bool(request.relative_time_window_present)
      << ",\n";
  out << nested << "\"trace_relative_window_start_ns\": "
      << request.trace_relative_window_start_ns << ",\n";
  out << nested << "\"trace_relative_window_end_ns\": "
      << request.trace_relative_window_end_ns << ",\n";
  out << nested << "\"clock_uncertainty_ns\": "
      << request.clock_uncertainty_ns << "\n";
  out << pad << "}";
  return out.str();
}

std::string native_record_json(const NativeTraceReference& record,
                               const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token native record JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(record.artifact) << ",\n";
  out << nested << "\"domain\": "
      << inferlab_plan::json_string(record.domain) << ",\n";
  out << nested << "\"record_key\": "
      << inferlab_plan::json_string(record.record_key) << ",\n";
  out << nested << "\"record_ordinal\": " << record.record_ordinal
      << ",\n";
  out << nested << "\"span_role\": "
      << inferlab_plan::json_string(record.span_role) << ",\n";
  out << nested << "\"parent_span_present\": "
      << inferlab_plan::json_bool(record.parent_span_present) << ",\n";
  out << nested << "\"span_join_tokens_emitted\": false,\n";
  out << nested << "\"same_request_lineage_proven\": "
      << inferlab_plan::json_bool(record.same_request_lineage_proven)
      << ",\n";
  out << nested << "\"matched_product_source_index\": "
      << record.matched_product_source_index << ",\n";
  out << nested << "\"matched_product_request_ordinal\": "
      << record.matched_product_request_ordinal << ",\n";
  out << nested << "\"request_lineage_depth\": "
      << record.request_lineage_depth << ",\n";
  out << nested << "\"request_lineage_relation\": "
      << inferlab_plan::json_string(record.request_lineage_relation)
      << ",\n";
  out << nested << "\"relative_timestamp_present\": "
      << inferlab_plan::json_bool(record.relative_timestamp_present)
      << ",\n";
  out << nested << "\"trace_relative_timestamp_ns\": "
      << record.trace_relative_timestamp_ns << ",\n";
  out << nested << "\"clock_uncertainty_ns\": "
      << record.clock_uncertainty_ns << ",\n";
  out << nested << "\"time_window_relation\": "
      << inferlab_plan::json_string(record.time_window_relation) << ",\n";
  out << nested << "\"span_lifecycle_supplied\": "
      << inferlab_plan::json_bool(record.span_lifecycle_supplied) << ",\n";
  out << nested << "\"span_lifecycle_contract_ready\": "
      << inferlab_plan::json_bool(record.span_lifecycle_contract_ready)
      << ",\n";
  out << nested << "\"relative_span_lifecycle_present\": "
      << inferlab_plan::json_bool(record.relative_span_lifecycle_present)
      << ",\n";
  out << nested << "\"trace_relative_span_start_ns\": "
      << record.trace_relative_span_start_ns << ",\n";
  out << nested << "\"trace_relative_span_end_ns\": "
      << record.trace_relative_span_end_ns << ",\n";
  out << nested << "\"span_lifecycle_clock_uncertainty_ns\": "
      << record.span_lifecycle_clock_uncertainty_ns << ",\n";
  out << nested << "\"span_terminal_status\": "
      << inferlab_plan::json_string(record.span_terminal_status) << ",\n";
  out << nested << "\"span_lifecycle_status\": "
      << inferlab_plan::json_string(record.span_lifecycle_status) << ",\n";
  out << nested << "\"causal_predecessor_span_present\": "
      << inferlab_plan::json_bool(record.causal_predecessor_span_present)
      << ",\n";
  out << nested
      << "\"causal_predecessor_span_join_token_emitted\": false,\n";
  out << nested << "\"declared_temporal_relation\": "
      << inferlab_plan::json_string(record.declared_temporal_relation)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string product_requests_json(
    const std::vector<ProductTraceReference>& requests, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token product request array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < requests.size(); ++index) {
    out << product_request_json(requests[index], item_indent)
        << (index == requests.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string native_records_json(
    const std::vector<NativeTraceReference>& records, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token native record array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < records.size(); ++index) {
    out << native_record_json(records[index], item_indent)
        << (index == records.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string source_json(const TraceTokenSourceObservation& source,
                        const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token source JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"artifact\": "
      << inferlab_plan::json_string(source.artifact) << ",\n";
  out << nested << "\"artifact_type\": "
      << inferlab_plan::json_string(source.artifact_type) << ",\n";
  out << nested << "\"declared_distinct_trace_join_token_count\": "
      << source.declared_distinct_trace_join_token_count << ",\n";
  out << nested << "\"declared_eligible_record_count\": "
      << source.declared_eligible_record_count << ",\n";
  out << nested << "\"declared_record_count\": "
      << source.declared_record_count << ",\n";
  out << nested << "\"declared_trace_join_token_count\": "
      << source.declared_trace_join_token_count << ",\n";
  out << nested << "\"document_available\": "
      << inferlab_plan::json_bool(source.document_available) << ",\n";
  out << nested << "\"domain\": "
      << inferlab_plan::json_string(source.domain) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(source.issues, nested_indent)
      << ",\n";
  out << nested << "\"join_algorithm_matches\": "
      << inferlab_plan::json_bool(source.join_algorithm_matches) << ",\n";
  out << nested << "\"clock_calibration_contract_evaluated\": "
      << inferlab_plan::json_bool(
             source.clock_calibration_contract_evaluated)
      << ",\n";
  out << nested << "\"clock_calibration_contract_ready\": "
      << inferlab_plan::json_bool(source.clock_calibration_contract_ready)
      << ",\n";
  out << nested << "\"clock_calibration_source_supplied\": "
      << inferlab_plan::json_bool(
             source.clock_calibration_source_supplied)
      << ",\n";
  out << nested << "\"clock_calibration_all_lookups_calibrated\": "
      << inferlab_plan::json_bool(
             source.clock_calibration_all_lookups_calibrated)
      << ",\n";
  out << nested << "\"clock_normalization_performed\": "
      << inferlab_plan::json_bool(source.clock_normalization_performed)
      << ",\n";
  out << nested << "\"declared_clock_calibration_lookup_count\": "
      << source.declared_clock_calibration_lookup_count << ",\n";
  out << nested << "\"declared_clock_calibrated_timestamp_count\": "
      << source.declared_clock_calibrated_timestamp_count << ",\n";
  out << nested << "\"declared_clock_uncalibrated_timestamp_count\": "
      << source.declared_clock_uncalibrated_timestamp_count << ",\n";
  out << nested << "\"clock_calibration_fingerprint\": "
      << inferlab_plan::json_string(
             source.clock_calibration_fingerprint)
      << ",\n";
  out << nested << "\"clock_calibration_fingerprint_algorithm\": "
      << inferlab_plan::json_string(
             source.clock_calibration_fingerprint_algorithm)
      << ",\n";
  out << nested << "\"clock_calibration_decision\": "
      << inferlab_plan::json_string(source.clock_calibration_decision)
      << ",\n";
  out << nested << "\"span_lineage_contract_evaluated\": "
      << inferlab_plan::json_bool(
             source.span_lineage_contract_evaluated)
      << ",\n";
  out << nested << "\"span_lineage_contract_ready\": "
      << inferlab_plan::json_bool(source.span_lineage_contract_ready)
      << ",\n";
  out << nested << "\"declared_request_span_join_token_count\": "
      << source.declared_request_span_join_token_count << ",\n";
  out << nested << "\"parsed_request_span_join_token_count\": "
      << source.parsed_request_span_join_token_count << ",\n";
  out << nested << "\"declared_span_join_token_count\": "
      << source.declared_span_join_token_count << ",\n";
  out << nested << "\"parsed_span_join_token_count\": "
      << source.parsed_span_join_token_count << ",\n";
  out << nested << "\"span_lineage_issues\": "
      << inferlab_plan::json_string_array(source.span_lineage_issues,
                                          nested_indent)
      << ",\n";
  out << nested << "\"span_lifecycle_contract_evaluated\": "
      << inferlab_plan::json_bool(
             source.span_lifecycle_contract_evaluated)
      << ",\n";
  out << nested << "\"span_lifecycle_contract_ready\": "
      << inferlab_plan::json_bool(source.span_lifecycle_contract_ready)
      << ",\n";
  out << nested << "\"declared_span_lifecycle_record_count\": "
      << source.declared_span_lifecycle_record_count << ",\n";
  out << nested << "\"parsed_span_lifecycle_record_count\": "
      << source.parsed_span_lifecycle_record_count << ",\n";
  out << nested << "\"declared_causal_predecessor_count\": "
      << source.declared_causal_predecessor_count << ",\n";
  out << nested << "\"parsed_causal_predecessor_count\": "
      << source.parsed_causal_predecessor_count << ",\n";
  out << nested << "\"span_lifecycle_issues\": "
      << inferlab_plan::json_string_array(source.span_lifecycle_issues,
                                          nested_indent)
      << ",\n";
  out << nested << "\"parsed_distinct_trace_join_token_count\": "
      << source.parsed_distinct_trace_join_token_count << ",\n";
  out << nested << "\"parsed_eligible_record_count\": "
      << source.parsed_eligible_record_count << ",\n";
  out << nested << "\"parsed_record_count\": "
      << source.parsed_record_count << ",\n";
  out << nested << "\"parsed_trace_join_token_count\": "
      << source.parsed_trace_join_token_count << ",\n";
  out << nested << "\"refresh_command\": "
      << inferlab_plan::json_string(source.refresh_command) << ",\n";
  out << nested << "\"role\": "
      << inferlab_plan::json_string(
             trace_token_source_role_name(source.role))
      << ",\n";
  out << nested << "\"source_decision\": "
      << inferlab_plan::json_string(source.source_decision) << ",\n";
  out << nested << "\"structural_ready\": "
      << inferlab_plan::json_bool(source.structural_ready) << ",\n";
  out << nested << "\"summary_matches_records\": "
      << inferlab_plan::json_bool(source.summary_matches_records) << ",\n";
  out << nested << "\"token_contract_evaluated\": "
      << inferlab_plan::json_bool(source.token_contract_evaluated) << ",\n";
  out << nested << "\"token_contract_ready\": "
      << inferlab_plan::json_bool(source.token_contract_ready) << "\n";
  out << pad << "}";
  return out.str();
}

std::string sources_json(
    const std::vector<TraceTokenSourceObservation>& sources,
    const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token source array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < sources.size(); ++index) {
    out << source_json(sources[index], item_indent)
        << (index == sources.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string candidate_group_json(const TraceTokenCandidateGroup& group,
                                 const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token candidate group JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"anonymous_join_token_emitted\": false,\n";
  out << nested << "\"join_group_ordinal\": "
      << group.join_group_ordinal << ",\n";
  out << nested << "\"native_record_count\": "
      << group.native_records.size() << ",\n";
  out << nested << "\"native_records\": "
      << native_records_json(group.native_records, nested_indent) << ",\n";
  out << nested << "\"one_product_request\": "
      << inferlab_plan::json_bool(group.one_product_request) << ",\n";
  out << nested << "\"product_request_count\": "
      << group.product_requests.size() << ",\n";
  out << nested << "\"product_requests\": "
      << product_requests_json(group.product_requests, nested_indent)
      << ",\n";
  out << nested << "\"same_request_proven\": "
      << inferlab_plan::json_bool(group.same_request_proven) << ",\n";
  out << nested << "\"span_lineage_evaluated\": "
      << inferlab_plan::json_bool(group.span_lineage_evaluated) << ",\n";
  out << nested << "\"request_span_bound_product_request_count\": "
      << group.request_span_bound_product_request_count << ",\n";
  out << nested << "\"lineage_resolved_native_record_count\": "
      << group.lineage_resolved_native_record_count << ",\n";
  out << nested << "\"lineage_unresolved_native_record_count\": "
      << group.lineage_unresolved_native_record_count << ",\n";
  out << nested << "\"lineage_conflict_native_record_count\": "
      << group.lineage_conflict_native_record_count << ",\n";
  out << nested << "\"span_lineage_status\": "
      << inferlab_plan::json_string(group.span_lineage_status) << ",\n";
  out << nested << "\"span_lifecycle_evaluated\": "
      << inferlab_plan::json_bool(group.span_lifecycle_evaluated) << ",\n";
  out << nested << "\"lifecycle_ready_product_request_count\": "
      << group.lifecycle_ready_product_request_count << ",\n";
  out << nested << "\"lifecycle_ready_native_record_count\": "
      << group.lifecycle_ready_native_record_count << ",\n";
  out << nested << "\"declared_causal_edge_count\": "
      << group.declared_causal_edge_count << ",\n";
  out << nested << "\"resolved_causal_edge_count\": "
      << group.resolved_causal_edge_count << ",\n";
  out << nested << "\"declared_happens_before_edge_count\": "
      << group.declared_happens_before_edge_count << ",\n";
  out << nested << "\"indeterminate_causal_edge_count\": "
      << group.indeterminate_causal_edge_count << ",\n";
  out << nested << "\"conflict_causal_edge_count\": "
      << group.conflict_causal_edge_count << ",\n";
  out << nested << "\"span_lifecycle_status\": "
      << inferlab_plan::json_string(group.span_lifecycle_status) << ",\n";
  out << nested << "\"clock_calibration_fingerprint_matches\": "
      << inferlab_plan::json_bool(
             group.clock_calibration_fingerprint_matches)
      << ",\n";
  out << nested << "\"time_window_comparison_performed\": "
      << inferlab_plan::json_bool(group.time_window_comparison_performed)
      << ",\n";
  out << nested << "\"time_window_compatible_native_record_count\": "
      << group.time_window_compatible_native_record_count << ",\n";
  out << nested << "\"time_window_excluded_native_record_count\": "
      << group.time_window_excluded_native_record_count << ",\n";
  out << nested << "\"time_window_indeterminate_native_record_count\": "
      << group.time_window_indeterminate_native_record_count << ",\n";
  out << nested << "\"time_window_status\": "
      << inferlab_plan::json_string(group.time_window_status) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(group.status) << "\n";
  out << pad << "}";
  return out.str();
}

std::string candidate_groups_json(
    const std::vector<TraceTokenCandidateGroup>& groups,
    const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token candidate group array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < groups.size(); ++index) {
    out << candidate_group_json(groups[index], item_indent)
        << (index == groups.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string unmatched_product_json(const UnmatchedProductTrace& item,
                                   const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token unmatched product JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"reason\": "
      << inferlab_plan::json_string(item.reason) << ",\n";
  out << nested << "\"request\": "
      << product_request_json(item.request, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

std::string unmatched_product_requests_json(
    const std::vector<UnmatchedProductTrace>& items, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token unmatched product array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < items.size(); ++index) {
    out << unmatched_product_json(items[index], item_indent)
        << (index == items.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string unmatched_native_json(const UnmatchedNativeTrace& item,
                                  const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token unmatched native JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"reason\": "
      << inferlab_plan::json_string(item.reason) << ",\n";
  out << nested << "\"record\": "
      << native_record_json(item.record, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

std::string unmatched_native_records_json(
    const std::vector<UnmatchedNativeTrace>& items, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token unmatched native array indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < items.size(); ++index) {
    out << unmatched_native_json(items[index], item_indent)
        << (index == items.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string summary_json(const TraceTokenJoinSummary& summary,
                         const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token summary JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"blocker_count\": " << summary.blocker_count
      << ",\n";
  out << nested << "\"candidate_join_present\": "
      << inferlab_plan::json_bool(summary.candidate_join_present) << ",\n";
  out << nested << "\"comparison_performed\": "
      << inferlab_plan::json_bool(summary.comparison_performed) << ",\n";
  out << nested << "\"matched_native_record_count\": "
      << summary.matched_native_record_count << ",\n";
  out << nested << "\"matched_product_request_count\": "
      << summary.matched_product_request_count << ",\n";
  out << nested << "\"matched_trace_group_count\": "
      << summary.matched_trace_group_count << ",\n";
  out << nested
      << "\"multi_product_request_matched_trace_group_count\": "
      << summary.multi_product_request_matched_trace_group_count << ",\n";
  out << nested << "\"native_eligible_record_count\": "
      << summary.native_eligible_record_count << ",\n";
  out << nested << "\"native_record_count\": "
      << summary.native_record_count << ",\n";
  out << nested << "\"native_source_count\": "
      << summary.native_source_count << ",\n";
  out << nested << "\"product_eligible_request_count\": "
      << summary.product_eligible_request_count << ",\n";
  out << nested << "\"product_request_count\": "
      << summary.product_request_count << ",\n";
  out << nested << "\"product_source_count\": "
      << summary.product_source_count << ",\n";
  out << nested << "\"ready_native_source_count\": "
      << summary.ready_native_source_count << ",\n";
  out << nested << "\"ready_source_count\": "
      << summary.ready_source_count << ",\n";
  out << nested
      << "\"single_product_request_matched_trace_group_count\": "
      << summary.single_product_request_matched_trace_group_count << ",\n";
  out << nested << "\"source_contract_issue_count\": "
      << summary.source_contract_issue_count << ",\n";
  out << nested << "\"span_lineage_source_contract_issue_count\": "
      << summary.span_lineage_source_contract_issue_count << ",\n";
  out << nested << "\"span_lineage_ready_source_count\": "
      << summary.span_lineage_ready_source_count << ",\n";
  out << nested << "\"source_count\": " << summary.source_count << ",\n";
  out << nested << "\"single_request_time_window_group_count\": "
      << summary.single_request_time_window_group_count << ",\n";
  out << nested << "\"ambiguous_time_window_native_record_count\": "
      << summary.ambiguous_time_window_native_record_count << ",\n";
  out << nested << "\"time_window_comparison_count\": "
      << summary.time_window_comparison_count << ",\n";
  out << nested << "\"time_window_compatible_native_record_count\": "
      << summary.time_window_compatible_native_record_count << ",\n";
  out << nested << "\"time_window_excluded_native_record_count\": "
      << summary.time_window_excluded_native_record_count << ",\n";
  out << nested << "\"time_window_indeterminate_native_record_count\": "
      << summary.time_window_indeterminate_native_record_count << ",\n";
  out << nested << "\"time_window_filter_performed\": "
      << inferlab_plan::json_bool(summary.time_window_filter_performed)
      << ",\n";
  out << nested << "\"time_window_decision\": "
      << inferlab_plan::json_string(
             trace_time_window_decision_name(summary.time_window_decision))
      << ",\n";
  out << nested << "\"request_span_bound_product_request_count\": "
      << summary.request_span_bound_product_request_count << ",\n";
  out << nested << "\"span_lineage_evaluated_group_count\": "
      << summary.span_lineage_evaluated_group_count << ",\n";
  out << nested << "\"same_request_lineage_proven_group_count\": "
      << summary.same_request_lineage_proven_group_count << ",\n";
  out << nested << "\"lineage_resolved_native_record_count\": "
      << summary.lineage_resolved_native_record_count << ",\n";
  out << nested << "\"lineage_unresolved_native_record_count\": "
      << summary.lineage_unresolved_native_record_count << ",\n";
  out << nested << "\"lineage_conflict_native_record_count\": "
      << summary.lineage_conflict_native_record_count << ",\n";
  out << nested << "\"span_lineage_evaluation_performed\": "
      << inferlab_plan::json_bool(
             summary.span_lineage_evaluation_performed)
      << ",\n";
  out << nested << "\"span_lineage_decision\": "
      << inferlab_plan::json_string(
             trace_span_lineage_decision_name(
                 summary.span_lineage_decision))
      << ",\n";
  out << nested << "\"span_lifecycle_source_contract_issue_count\": "
      << summary.span_lifecycle_source_contract_issue_count << ",\n";
  out << nested << "\"span_lifecycle_ready_source_count\": "
      << summary.span_lifecycle_ready_source_count << ",\n";
  out << nested << "\"lifecycle_ready_product_request_count\": "
      << summary.lifecycle_ready_product_request_count << ",\n";
  out << nested << "\"lifecycle_ready_native_record_count\": "
      << summary.lifecycle_ready_native_record_count << ",\n";
  out << nested << "\"span_lifecycle_evaluated_group_count\": "
      << summary.span_lifecycle_evaluated_group_count << ",\n";
  out << nested << "\"declared_causal_edge_count\": "
      << summary.declared_causal_edge_count << ",\n";
  out << nested << "\"resolved_causal_edge_count\": "
      << summary.resolved_causal_edge_count << ",\n";
  out << nested << "\"declared_happens_before_edge_count\": "
      << summary.declared_happens_before_edge_count << ",\n";
  out << nested << "\"indeterminate_causal_edge_count\": "
      << summary.indeterminate_causal_edge_count << ",\n";
  out << nested << "\"conflict_causal_edge_count\": "
      << summary.conflict_causal_edge_count << ",\n";
  out << nested << "\"span_lifecycle_evaluation_performed\": "
      << inferlab_plan::json_bool(
             summary.span_lifecycle_evaluation_performed)
      << ",\n";
  out << nested << "\"span_lifecycle_decision\": "
      << inferlab_plan::json_string(
             trace_span_lifecycle_decision_name(
                 summary.span_lifecycle_decision))
      << ",\n";
  out << nested << "\"trace_token_join_decision\": "
      << inferlab_plan::json_string(
             trace_token_join_decision_name(summary.decision))
      << ",\n";
  out << nested << "\"unmatched_native_record_count\": "
      << summary.unmatched_native_record_count << ",\n";
  out << nested << "\"unmatched_product_request_count\": "
      << summary.unmatched_product_request_count << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string trace_token_join_json(
    const TraceTokenJoinProjection& projection, const int indent) {
  validate_trace_token_join_projection(projection);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace-token projection JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const bool upstream_clock_normalization_consumed = std::any_of(
      projection.sources.begin(), projection.sources.end(),
      [](const TraceTokenSourceObservation& source) {
        return source.clock_normalization_performed;
      });
  const bool same_request_proven =
      projection.summary.span_lineage_decision ==
      TraceSpanLineageDecision::SameRequestLineageEvidenceReady;
  const bool span_parentage_proven = std::any_of(
      projection.matched_groups.begin(), projection.matched_groups.end(),
      [](const TraceTokenCandidateGroup& group) {
        return std::any_of(
            group.native_records.begin(), group.native_records.end(),
            [](const NativeTraceReference& record) {
              return record.same_request_lineage_proven &&
                     record.request_lineage_depth > 0U;
            });
      });
  const bool declared_happens_before_evidence =
      projection.summary.span_lifecycle_decision ==
      TraceSpanLifecycleDecision::DeclaredHappensBeforeEvidenceReady;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"anonymous_join_tokens_emitted\": false,\n";
  out << nested << "\"causality_proven\": false,\n";
  out << nested << "\"absolute_timestamp_values_emitted\": false,\n";
  out << nested << "\"trace_relative_time_fields_emitted\": true,\n";
  out << nested << "\"clock_normalization_performed\": false,\n";
  out << nested << "\"declared_causal_predecessor_tokens_emitted\": false,\n";
  out << nested << "\"declared_happens_before_evidence\": "
      << inferlab_plan::json_bool(declared_happens_before_evidence)
      << ",\n";
  out << nested << "\"cross_artifact_trace_token_candidate_join_performed\": "
      << inferlab_plan::json_bool(
             projection.summary.comparison_performed)
      << ",\n";
  out << nested << "\"input_join_tokens_are_cryptographic\": false,\n";
  out << nested << "\"join_token_algorithm\": "
      << inferlab_plan::json_string(projection.join_token_algorithm)
      << ",\n";
  out << nested << "\"matched_trace_candidate_groups\": "
      << candidate_groups_json(projection.matched_groups, nested_indent)
      << ",\n";
  out << nested << "\"product_or_native_execute_performed\": false,\n";
  out << nested << "\"recommended_actions\": "
      << inferlab_plan::json_string_array(projection.recommended_actions,
                                          nested_indent)
      << ",\n";
  out << nested << "\"recommended_commands\": "
      << inferlab_plan::json_string_array(projection.recommended_commands,
                                          nested_indent)
      << ",\n";
  out << nested << "\"same_request_proven\": "
      << inferlab_plan::json_bool(same_request_proven) << ",\n";
  out << nested << "\"schema_version\": 1,\n";
  out << nested << "\"source_contracts\": "
      << sources_json(projection.sources, nested_indent) << ",\n";
  out << nested << "\"span_parentage_proven\": "
      << inferlab_plan::json_bool(span_parentage_proven) << ",\n";
  out << nested << "\"request_to_span_lineage_evaluated\": "
      << inferlab_plan::json_bool(
             projection.summary.span_lineage_evaluation_performed)
      << ",\n";
  out << nested << "\"span_lineage_limitations\": "
      << inferlab_plan::json_string_array(
             projection.span_lineage_limitations, nested_indent)
      << ",\n";
  out << nested << "\"span_lineage_recommended_actions\": "
      << inferlab_plan::json_string_array(
             projection.span_lineage_recommended_actions, nested_indent)
      << ",\n";
  out << nested << "\"span_lifecycle_evaluation_performed\": "
      << inferlab_plan::json_bool(
             projection.summary.span_lifecycle_evaluation_performed)
      << ",\n";
  out << nested << "\"span_lifecycle_limitations\": "
      << inferlab_plan::json_string_array(
             projection.span_lifecycle_limitations, nested_indent)
      << ",\n";
  out << nested << "\"span_lifecycle_recommended_actions\": "
      << inferlab_plan::json_string_array(
             projection.span_lifecycle_recommended_actions,
             nested_indent)
      << ",\n";
  out << nested << "\"time_window_filter_performed\": "
      << inferlab_plan::json_bool(
             projection.summary.time_window_filter_performed)
      << ",\n";
  out << nested << "\"time_window_limitations\": "
      << inferlab_plan::json_string_array(
             projection.time_window_limitations, nested_indent)
      << ",\n";
  out << nested << "\"time_window_recommended_actions\": "
      << inferlab_plan::json_string_array(
             projection.time_window_recommended_actions, nested_indent)
      << ",\n";
  out << nested << "\"summary\": "
      << summary_json(projection.summary, nested_indent) << ",\n";
  out << nested << "\"token_contract_blockers\": "
      << inferlab_plan::json_string_array(projection.blockers, nested_indent)
      << ",\n";
  out << nested << "\"unmatched_native_records\": "
      << unmatched_native_records_json(
             projection.unmatched_native_records, nested_indent)
      << ",\n";
  out << nested << "\"unmatched_product_requests\": "
      << unmatched_product_requests_json(
             projection.unmatched_product_requests, nested_indent)
      << ",\n";
  out << nested << "\"upstream_clock_normalization_consumed\": "
      << inferlab_plan::json_bool(upstream_clock_normalization_consumed)
      << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(projection.claim_boundary) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_deep_dive_trace
