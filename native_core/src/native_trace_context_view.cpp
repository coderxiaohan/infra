#include "native_trace_context_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "trace_clock_calibration_view.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab_native_trace {
namespace {

std::string record_json(const RecordProjection& record, const int indent) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "native trace context record JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"record_ordinal\": " << record.record_ordinal
      << ",\n";
  out << nested << "\"record_key\": "
      << inferlab_plan::json_string(record.record_key) << ",\n";
  out << nested << "\"span_role\": "
      << inferlab_plan::json_string(record.span_role) << ",\n";
  out << nested << "\"context_present\": "
      << inferlab_plan::json_bool(record.context_present) << ",\n";
  out << nested << "\"correlation_eligible\": "
      << inferlab_plan::json_bool(record.correlation_eligible) << ",\n";
  out << nested << "\"trace_join_token\": "
      << inferlab_plan::json_string(record.trace_join_token) << ",\n";
  out << nested << "\"span_join_token\": "
      << inferlab_plan::json_string(record.span_join_token) << ",\n";
  out << nested << "\"parent_span_present\": "
      << inferlab_plan::json_bool(record.parent_span_present) << ",\n";
  out << nested << "\"parent_span_join_token\": "
      << inferlab_plan::json_string(record.parent_span_join_token)
      << ",\n";
  out << nested << "\"span_lifecycle_supplied\": "
      << inferlab_plan::json_bool(record.span_lifecycle_supplied)
      << ",\n";
  out << nested << "\"span_lifecycle_contract_ready\": "
      << inferlab_plan::json_bool(record.span_lifecycle_contract_ready)
      << ",\n";
  out << nested << "\"span_terminal_status\": "
      << inferlab_plan::json_string(record.span_terminal_status) << ",\n";
  out << nested << "\"span_lifecycle_status\": "
      << inferlab_plan::json_string(record.span_lifecycle_status) << ",\n";
  out << nested << "\"relative_span_lifecycle_present\": "
      << inferlab_plan::json_bool(
             record.relative_span_lifecycle_present)
      << ",\n";
  out << nested << "\"trace_relative_span_start_ns\": "
      << record.trace_relative_span_start_ns << ",\n";
  out << nested << "\"trace_relative_span_end_ns\": "
      << record.trace_relative_span_end_ns << ",\n";
  out << nested << "\"span_lifecycle_clock_uncertainty_ns\": "
      << record.span_lifecycle_clock_uncertainty_ns << ",\n";
  out << nested << "\"causal_predecessor_span_present\": "
      << inferlab_plan::json_bool(
             record.causal_predecessor_span_present)
      << ",\n";
  out << nested << "\"causal_predecessor_span_join_token\": "
      << inferlab_plan::json_string(
             record.causal_predecessor_span_join_token)
      << ",\n";
  out << nested << "\"clock_domain\": "
      << inferlab_plan::json_string(record.clock_domain) << ",\n";
  out << nested << "\"timestamp_present\": "
      << inferlab_plan::json_bool(record.timestamp_present) << ",\n";
  out << nested << "\"timestamp_value_emitted\": false,\n";
  out << nested << "\"clock_calibration_status\": "
      << inferlab_plan::json_string(record.clock_calibration_status)
      << ",\n";
  out << nested << "\"relative_timestamp_present\": "
      << inferlab_plan::json_bool(record.relative_timestamp_present)
      << ",\n";
  out << nested << "\"trace_relative_timestamp_ns\": "
      << record.trace_relative_timestamp_ns << ",\n";
  out << nested << "\"clock_uncertainty_ns\": "
      << record.clock_uncertainty_ns << ",\n";
  out << nested << "\"source_line_number\": "
      << record.source_line_number << "\n";
  out << pad << "}";
  return out.str();
}

std::string records_json(const Projection& projection, const int indent) {
  const int item_indent = inferlab::internal::checked_add_int(
      indent, 2, "native trace context records JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < projection.records.size(); ++index) {
    out << record_json(projection.records[index], item_indent)
        << (index == projection.records.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

}  // namespace

std::string trace_context_projection_json(const Projection& projection,
                                          const int indent) {
  validate_trace_context_projection(projection);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "native trace context projection JSON indent");
  const int summary_indent = inferlab::internal::checked_add_int(
      nested_indent, 2, "native trace context summary JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const std::string summary_pad =
      inferlab_plan::json_indent(summary_indent);
  const auto& summary = projection.summary;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"schema_version\": 1,\n";
  out << nested << "\"domain\": "
      << inferlab_plan::json_string(domain_name(projection.domain))
      << ",\n";
  out << nested << "\"source_supplied\": "
      << inferlab_plan::json_bool(projection.source.supplied) << ",\n";
  out << nested << "\"source_path_label\": "
      << inferlab_plan::json_string(projection.source.source_path_label)
      << ",\n";
  out << nested << "\"source_path_redacted\": "
      << inferlab_plan::json_bool(projection.source.path_redacted)
      << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(projection.source.path_probe_status)
      << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(projection.source.text_read_status)
      << ",\n";
  out << nested << "\"source_contract_ready\": "
      << inferlab_plan::json_bool(projection.source.contract_ready)
      << ",\n";
  out << nested << "\"source_present\": "
      << inferlab_plan::json_bool(projection.source.present) << ",\n";
  out << nested << "\"source_readable\": "
      << inferlab_plan::json_bool(projection.source.readable) << ",\n";
  out << nested << "\"source_physical_line_count\": "
      << projection.source.physical_line_count << ",\n";
  out << nested << "\"identity_values_emitted\": false,\n";
  out << nested << "\"raw_identity_values_emitted\": false,\n";
  out << nested << "\"timestamp_values_emitted\": false,\n";
  out << nested << "\"anonymous_join_tokens_emitted\": "
      << inferlab_plan::json_bool(summary.trace_join_token_count > 0U)
      << ",\n";
  out << nested << "\"join_token_algorithm\": "
      << inferlab_plan::json_string(projection.join_token_algorithm)
      << ",\n";
  out << nested << "\"join_tokens_are_cryptographic\": false,\n";
  out << nested << "\"cross_artifact_join_performed\": false,\n";
  out << nested << "\"absolute_timestamp_values_emitted\": false,\n";
  out << nested << "\"relative_timestamp_values_emitted\": "
      << inferlab_plan::json_bool(
             summary.clock_calibrated_record_count > 0U)
      << ",\n";
  out << nested << "\"clock_normalization_performed\": "
      << inferlab_plan::json_bool(
             summary.clock_normalization_performed)
      << ",\n";
  out << nested << "\"span_parentage_proven\": false,\n";
  out << nested << "\"causal_relation_proven\": false,\n";
  out << nested << "\"explicit_span_lifecycle_supported\": true,\n";
  out << nested << "\"declared_causal_predecessor_tokens_emitted\": "
      << inferlab_plan::json_bool(
             summary.causal_predecessor_span_count > 0U)
      << ",\n";
  out << nested << "\"summary\": {\n";
  out << summary_pad << "\"record_count\": " << summary.record_count
      << ",\n";
  out << summary_pad << "\"source_record_count\": "
      << summary.source_record_count << ",\n";
  out << summary_pad << "\"context_present_count\": "
      << summary.context_present_count << ",\n";
  out << summary_pad << "\"context_missing_count\": "
      << summary.context_missing_count << ",\n";
  out << summary_pad << "\"trace_join_token_count\": "
      << summary.trace_join_token_count << ",\n";
  out << summary_pad << "\"distinct_trace_join_token_count\": "
      << summary.distinct_trace_join_token_count << ",\n";
  out << summary_pad << "\"span_join_token_count\": "
      << summary.span_join_token_count << ",\n";
  out << summary_pad << "\"parent_span_join_token_count\": "
      << summary.parent_span_join_token_count << ",\n";
  out << summary_pad << "\"clock_domain_count\": "
      << summary.clock_domain_count << ",\n";
  out << summary_pad << "\"clock_calibrated_record_count\": "
      << summary.clock_calibrated_record_count << ",\n";
  out << summary_pad << "\"clock_uncalibrated_record_count\": "
      << summary.clock_uncalibrated_record_count << ",\n";
  out << summary_pad << "\"span_lifecycle_supplied_record_count\": "
      << summary.span_lifecycle_supplied_record_count << ",\n";
  out << summary_pad
      << "\"span_lifecycle_contract_ready_record_count\": "
      << summary.span_lifecycle_contract_ready_record_count << ",\n";
  out << summary_pad << "\"span_lifecycle_calibrated_record_count\": "
      << summary.span_lifecycle_calibrated_record_count << ",\n";
  out << summary_pad << "\"span_lifecycle_uncalibrated_record_count\": "
      << summary.span_lifecycle_uncalibrated_record_count << ",\n";
  out << summary_pad << "\"causal_predecessor_span_count\": "
      << summary.causal_predecessor_span_count << ",\n";
  out << summary_pad << "\"span_lifecycle_normalization_performed\": "
      << inferlab_plan::json_bool(
             summary.span_lifecycle_normalization_performed)
      << ",\n";
  out << summary_pad << "\"span_lifecycle_decision\": "
      << inferlab_plan::json_string(summary.span_lifecycle_decision)
      << ",\n";
  out << summary_pad << "\"all_contexts_bound\": "
      << inferlab_plan::json_bool(summary.all_contexts_bound) << ",\n";
  out << summary_pad << "\"native_trace_context_decision\": "
      << inferlab_plan::json_string(summary.decision) << "\n";
  out << nested << "},\n";
  out << nested << "\"clock_calibration\": "
      << inferlab_trace_clock::clock_calibration_usage_json(
             projection.clock_calibration, nested_indent)
      << ",\n";
  out << nested << "\"span_lifecycle_clock_calibration\": "
      << inferlab_trace_clock::clock_calibration_usage_json(
             projection.span_lifecycle_clock_calibration,
             nested_indent)
      << ",\n";
  out << nested << "\"records\": "
      << records_json(projection, nested_indent) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(projection.claim_boundary) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_native_trace
