#include "product_replay_semantic_events_view.hpp"

#include "artifact_cli_path_view.hpp"
#include "plan_json_utils.hpp"
#include "product_replay_request_trace_view.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::product_replay_semantic_events {
namespace {

using inferlab_artifact::cli_path_field_json;
using inferlab_plan::json_bool;

std::string sample_path_list_json(const SemanticEventsPlan& plan,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.sample_sources.size(); ++index) {
    const auto& source = plan.sample_sources[index];
    out << nested << "{\n";
    out << cli_path_field_json("sample_path", source.request.path,
                               indent + 4)
        << ",\n";
    out << nested << "  \"origin\": "
        << inferlab_plan::json_string(
               sample_input_origin_name(source.request.origin))
        << ",\n";
    out << nested << "  \"expected_product_profile_id\": "
        << inferlab_plan::json_string(
               source.request.expected_product_profile_id)
        << "\n";
    out << nested << "}"
        << (index + 1U == plan.sample_sources.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string cli_context_json(
    const SemanticEventsPlan& plan,
    const SemanticEventsArtifactViewContext& context, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"format\": "
      << inferlab_plan::json_string(context.format) << ",\n";
  out << nested << "\"strict_cli_validation\": true,\n";
  out << nested << "\"unsupported_positionals_rejected\": true,\n";
  out << nested << "\"unsupported_format_rejected\": true,\n";
  out << nested << "\"supported_formats\": "
      << inferlab_plan::json_string_array({"json"}, indent + 2) << ",\n";
  out << nested << "\"sample_path_count\": "
      << plan.sample_sources.size() << ",\n";
  out << cli_path_field_json("repo_root", context.repo_root, indent + 2)
      << ",\n";
  out << cli_path_field_json("results_root", context.results_root,
                              indent + 2)
      << ",\n";
  out << cli_path_field_json("output", context.output, indent + 2)
      << ",\n";
  out << nested << "\"sample_paths\": "
      << sample_path_list_json(plan, indent + 2) << ",\n";
  out << nested
      << "\"claim_boundary\": \"default samples are repository fixtures; only explicit --sample rows that satisfy canonical product, timestamp, claim-scope and event-field contracts are replay-derived, and no sample is execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

std::string sample_source_json(const SampleSourceState& source,
                               const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << cli_path_field_json("path", source.request.path, indent + 2)
      << ",\n";
  out << nested << "\"origin\": "
      << inferlab_plan::json_string(
             sample_input_origin_name(source.request.origin))
      << ",\n";
  out << nested << "\"expected_product_profile_id\": "
      << inferlab_plan::json_string(
             source.request.expected_product_profile_id)
      << ",\n";
  out << nested << "\"present\": " << json_bool(source.present)
      << ",\n";
  out << nested << "\"regular_file\": "
      << json_bool(source.regular_file) << ",\n";
  out << nested << "\"readable\": " << json_bool(source.readable)
      << ",\n";
  out << nested << "\"size_allowed\": "
      << json_bool(source.size_allowed) << ",\n";
  out << nested << "\"source_contract_ready\": "
      << json_bool(source.source_contract_ready) << ",\n";
  out << nested << "\"read_issue\": " << json_bool(source.read_issue)
      << ",\n";
  out << nested << "\"byte_size\": " << source.byte_size << ",\n";
  out << nested << "\"path_probe_status\": "
      << inferlab_plan::json_string(source.path_probe_status) << ",\n";
  out << nested << "\"path_probe_error\": "
      << inferlab_plan::json_string(source.path_probe_error) << ",\n";
  out << nested << "\"text_read_status\": "
      << inferlab_plan::json_string(source.text_read_status) << ",\n";
  out << nested << "\"text_read_error\": "
      << inferlab_plan::json_string(source.text_read_error) << ",\n";
  out << nested << "\"event_count\": " << source.line_count << ",\n";
  out << nested << "\"valid_event_count\": "
      << source.valid_event_count << ",\n";
  out << nested << "\"fixture_event_count\": "
      << source.fixture_event_count << ",\n";
  out << nested << "\"explicit_replay_event_count\": "
      << source.explicit_replay_event_count << ",\n";
  out << nested << "\"invalid_event_count\": "
      << source.invalid_event_count << ",\n";
  out << nested << "\"unsupported_event_count\": "
      << source.unsupported_event_count << ",\n";
  out << nested << "\"event_types\": "
      << inferlab_plan::json_string_array(source.event_types, indent + 2)
      << ",\n";
  out << nested << "\"product_profile_ids\": "
      << inferlab_plan::json_string_array(source.product_profile_ids,
                                           indent + 2)
      << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(source.issues, indent + 2)
      << ",\n";
  out << nested
      << "\"claim_boundary\": \"sample rows are normalized without emitting raw payload values; fixture rows are schema-only and explicit replay rows are never execute evidence\"\n";
  out << pad << "}";
  return out.str();
}

std::string sample_sources_json(const SemanticEventsPlan& plan,
                                const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.sample_sources.size(); ++index) {
    out << sample_source_json(plan.sample_sources[index], indent + 2)
        << (index + 1U == plan.sample_sources.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string profile_source_json(const ProductProfileState& state,
                                const int indent) {
  const auto& profile = product_profile(state);
  const auto& contract = state.contract;
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"product_profile_id\": "
      << inferlab_plan::json_string(profile.id) << ",\n";
  out << nested << "\"product_name\": "
      << inferlab_plan::json_string(profile.display_name) << ",\n";
  out << nested << "\"category\": "
      << inferlab_plan::json_string(profile.category) << ",\n";
  out << nested << "\"source_artifact\": "
      << inferlab_plan::json_string(profile.artifact) << ",\n";
  out << nested << "\"path_label\": "
      << inferlab_plan::json_string(inferlab_plan::redacted_path_label(
             state.resolved_path.generic_string()))
      << ",\n";
  out << nested << "\"path_redacted\": true,\n";
  out << nested << "\"present\": " << json_bool(contract.present)
      << ",\n";
  out << nested << "\"regular_file\": "
      << json_bool(contract.regular_file) << ",\n";
  out << nested << "\"readable\": " << json_bool(contract.readable)
      << ",\n";
  out << nested << "\"parse_ok\": " << json_bool(contract.parse_ok)
      << ",\n";
  out << nested << "\"contract_ready\": "
      << json_bool(contract.contract_ready) << ",\n";
  out << nested << "\"mode\": "
      << inferlab_plan::json_string(contract.mode) << ",\n";
  out << nested << "\"executed\": " << json_bool(contract.executed)
      << ",\n";
  out << nested << "\"replayed\": " << json_bool(contract.replayed)
      << ",\n";
  out << nested << "\"product_evidence_eligible\": "
      << json_bool(contract.product_evidence_eligible) << ",\n";
  out << nested << "\"can_claim_product_performance\": "
      << json_bool(contract.can_claim_product_performance) << ",\n";
  out << nested << "\"contract_coverage\": "
      << inferlab_plan::fixed6(contract.contract_coverage) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(contract.issues, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string profile_sources_json(const SemanticEventsPlan& plan,
                                 const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.product_profiles.size(); ++index) {
    out << profile_source_json(plan.product_profiles[index], indent + 2)
        << (index + 1U == plan.product_profiles.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string normalized_event_json(const SemanticEventsPlan& plan,
                                  const NormalizedEventRow& row,
                                  const int indent) {
  const auto& source = plan.sample_sources.at(row.source_index);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"source_index\": " << row.source_index << ",\n";
  out << nested << "\"line_number\": " << row.line_number << ",\n";
  out << nested << "\"source_origin\": "
      << inferlab_plan::json_string(
             sample_input_origin_name(source.request.origin))
      << ",\n";
  out << nested << "\"event_type\": "
      << inferlab_plan::json_string(row.rule == nullptr
                                        ? "unsupported_or_invalid"
                                        : row.rule->event_type)
      << ",\n";
  out << nested << "\"event_domain\": "
      << inferlab_plan::json_string(row.rule == nullptr
                                        ? "unknown"
                                        : row.rule->event_domain)
      << ",\n";
  out << nested << "\"product_profile_id\": "
      << inferlab_plan::json_string(row.product_profile_id) << ",\n";
  out << nested << "\"source_artifact\": "
      << inferlab_plan::json_string(row.source_artifact) << ",\n";
  out << nested << "\"json_valid\": " << json_bool(row.json_valid)
      << ",\n";
  out << nested << "\"event_type_known\": "
      << json_bool(row.event_type_known) << ",\n";
  out << nested << "\"product_identity_valid\": "
      << json_bool(row.product_identity_valid) << ",\n";
  out << nested << "\"timestamp_present\": "
      << json_bool(row.timestamp_present) << ",\n";
  out << nested << "\"claim_scope_present\": "
      << json_bool(row.claim_scope_present) << ",\n";
  out << nested << "\"required_fields_valid\": "
      << json_bool(row.required_fields_valid) << ",\n";
  out << nested << "\"fixture_schema_valid\": "
      << json_bool(row.fixture_schema_valid) << ",\n";
  out << nested << "\"explicit_replay_valid\": "
      << json_bool(row.explicit_replay_valid) << ",\n";
  out << nested << "\"profile_contract_ready\": "
      << json_bool(row.profile_contract_ready) << ",\n";
  out << nested << "\"request_id_present\": "
      << json_bool(row.request_id_present) << ",\n";
  out << nested << "\"request_ids_present\": "
      << json_bool(row.request_ids_present) << ",\n";
  out << nested << "\"trace_context_valid\": "
      << json_bool(row.trace_context_valid) << ",\n";
  out << nested << "\"trace_id_present\": "
      << json_bool(row.trace_id_present) << ",\n";
  out << nested << "\"trace_ids_present\": "
      << json_bool(row.trace_ids_present) << ",\n";
  out << nested << "\"request_span_context_valid\": "
      << json_bool(row.request_span_context_valid) << ",\n";
  out << nested << "\"request_span_id_present\": "
      << json_bool(row.request_span_id_present) << ",\n";
  out << nested << "\"request_span_ids_present\": "
      << json_bool(row.request_span_ids_present) << ",\n";
  out << nested << "\"request_terminal_status_present\": "
      << json_bool(row.request_terminal_status_present) << ",\n";
  out << nested << "\"timestamp_value_present\": "
      << json_bool(row.timestamp_value_present) << ",\n";
  out << nested << "\"missing_fields\": "
      << inferlab_plan::json_string_array(row.missing_fields, indent + 2)
      << ",\n";
  out << nested << "\"invalid_fields\": "
      << inferlab_plan::json_string_array(row.invalid_fields, indent + 2)
      << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(row.issues, indent + 2) << ",\n";
  out << nested
      << "\"payload_values_emitted\": false,\n";
  out << nested << "\"request_identity_values_emitted\": false,\n";
  out << nested << "\"trace_identity_values_emitted\": false,\n";
  out << nested << "\"span_identity_values_emitted\": false\n";
  out << pad << "}";
  return out.str();
}

std::string normalized_events_json(const SemanticEventsPlan& plan,
                                   const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.normalized_events.size(); ++index) {
    out << normalized_event_json(plan, plan.normalized_events[index],
                                 indent + 2)
        << (index + 1U == plan.normalized_events.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string required_field_contract_json(
    const SemanticEventRuleSpec& rule, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < rule.required_fields.size(); ++index) {
    const auto& field = require_event_field(rule.required_fields[index]);
    out << nested << "{\"name\": "
        << inferlab_plan::json_string(field.name) << ", \"kind\": "
        << inferlab_plan::json_string(event_field_kind_name(field.kind))
        << "}"
        << (index + 1U == rule.required_fields.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string event_rule_json(
    const SemanticEventRuleSpec& rule,
    const product_replay_alignment::NativeArtifactIdentity& destination,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"event_type\": "
      << inferlab_plan::json_string(rule.event_type) << ",\n";
  out << nested << "\"product_family\": "
      << inferlab_plan::json_string(rule.event_domain) << ",\n";
  out << nested << "\"event_domain\": "
      << inferlab_plan::json_string(rule.event_domain) << ",\n";
  out << nested << "\"native_alignment\": "
      << inferlab_plan::json_string(destination.artifact) << ",\n";
  out << nested << "\"native_alignment_identity_owner\": "
      << inferlab_plan::json_string(destination.identity_owner) << ",\n";
  out << nested << "\"required_fields\": "
      << inferlab_plan::json_string_array(rule.required_fields, indent + 2)
      << ",\n";
  out << nested << "\"required_field_contracts\": "
      << required_field_contract_json(rule, indent + 2) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(rule.claim_boundary) << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const SemanticEventRecord& record,
                        const int indent) {
  const auto& rule = semantic_event_rule(record);
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"event_type\": "
      << inferlab_plan::json_string(rule.event_type) << ",\n";
  out << nested << "\"event_domain\": "
      << inferlab_plan::json_string(rule.event_domain) << ",\n";
  out << nested << "\"source_artifact\": "
      << inferlab_plan::json_string(record.source_artifacts.empty()
                                        ? ""
                                        : record.source_artifacts.front())
      << ",\n";
  out << nested << "\"source_artifacts\": "
      << inferlab_plan::json_string_array(record.source_artifacts,
                                           indent + 2)
      << ",\n";
  out << nested << "\"observed_product_profile_ids\": "
      << inferlab_plan::json_string_array(
             record.observed_product_profile_ids, indent + 2)
      << ",\n";
  out << nested << "\"native_alignment\": "
      << inferlab_plan::json_string(record.native_destination.artifact)
      << ",\n";
  out << nested << "\"native_alignment_target\": "
      << inferlab_plan::json_string(
             record.native_destination.producer_target)
      << ",\n";
  out << nested << "\"native_alignment_identity_owner\": "
      << inferlab_plan::json_string(
             record.native_destination.identity_owner)
      << ",\n";
  out << nested << "\"native_alignment_refresh_command\": "
      << inferlab_plan::json_string(
             record.native_destination.refresh_command)
      << ",\n";
  out << nested << "\"source_present\": "
      << json_bool(record.source_present) << ",\n";
  out << nested << "\"source_replayed\": "
      << json_bool(record.source_replayed) << ",\n";
  out << nested << "\"source_executed\": "
      << json_bool(record.source_executed) << ",\n";
  out << nested << "\"sample_observed\": "
      << json_bool(record.sample_observed) << ",\n";
  out << nested << "\"observed_signal\": "
      << json_bool(record.observed_signal) << ",\n";
  out << nested << "\"fixture_event_count\": "
      << record.fixture_event_count << ",\n";
  out << nested << "\"explicit_replay_event_count\": "
      << record.explicit_replay_event_count << ",\n";
  out << nested << "\"replay_ready_event_count\": "
      << record.replay_ready_event_count << ",\n";
  out << nested << "\"invalid_fixture_event_count\": "
      << record.invalid_fixture_event_count << ",\n";
  out << nested << "\"invalid_explicit_event_count\": "
      << record.invalid_explicit_event_count << ",\n";
  out << nested << "\"product_profile_present_count\": "
      << record.product_profile_present_count << ",\n";
  out << nested << "\"product_profile_contract_ready_count\": "
      << record.product_profile_contract_ready_count << ",\n";
  out << nested << "\"product_profile_replayed_count\": "
      << record.product_profile_replayed_count << ",\n";
  out << nested << "\"product_profile_executed_count\": "
      << record.product_profile_executed_count << ",\n";
  out << nested << "\"read_issue_count\": "
      << record.read_issue_count << ",\n";
  out << nested << "\"contract_check_count\": "
      << record.contract_check_count << ",\n";
  out << nested << "\"satisfied_contract_check_count\": "
      << record.satisfied_contract_check_count << ",\n";
  out << nested << "\"contract_coverage\": "
      << inferlab_plan::fixed6(record.contract_coverage) << ",\n";
  out << nested << "\"confidence\": "
      << inferlab_plan::fixed6(record.confidence) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(record.status) << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(record.recommendation) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(record.issues, indent + 2)
      << ",\n";
  out << nested << "\"rule\": "
      << event_rule_json(rule, record.native_destination, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

std::string records_json(const SemanticEventsPlan& plan,
                         const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << record_json(plan.records[index], indent + 2)
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string product_coverage_json(const SemanticEventsPlan& plan,
                                  const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.product_coverage.size(); ++index) {
    const auto& coverage = plan.product_coverage[index];
    const auto& state = plan.product_profiles.at(
        coverage.product_profile_index);
    const auto& profile = product_profile(state);
    out << nested << "{\n";
    out << nested << "  \"product_profile_id\": "
        << inferlab_plan::json_string(profile.id) << ",\n";
    out << nested << "  \"product_family\": "
        << inferlab_plan::json_string(profile.category) << ",\n";
    out << nested << "  \"product_name\": "
        << inferlab_plan::json_string(profile.display_name) << ",\n";
    out << nested << "  \"source_artifact\": "
        << inferlab_plan::json_string(profile.artifact) << ",\n";
    out << nested << "  \"sample_source_count\": "
        << coverage.sample_source_indices.size() << ",\n";
    out << nested << "  \"fixture_event_count\": "
        << coverage.fixture_event_count << ",\n";
    out << nested << "  \"explicit_replay_event_count\": "
        << coverage.explicit_replay_event_count << ",\n";
    out << nested << "  \"sample_event_count\": "
        << coverage.valid_event_count << ",\n";
    out << nested << "  \"product_profile_contract_ready\": "
        << json_bool(state.contract.contract_ready) << ",\n";
    out << nested << "  \"coverage_status\": "
        << inferlab_plan::json_string(coverage.coverage_status) << "\n";
    out << nested << "}"
        << (index + 1U == plan.product_coverage.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string event_schema_coverage_json(const SemanticEventsPlan& plan,
                                       const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    const auto& rule = semantic_event_rule(record);
    out << nested << "{\n";
    out << nested << "  \"event_type\": "
        << inferlab_plan::json_string(rule.event_type) << ",\n";
    out << nested << "  \"event_domain\": "
        << inferlab_plan::json_string(rule.event_domain) << ",\n";
    out << nested << "  \"required_field_count\": "
        << rule.required_fields.size() << ",\n";
    out << nested << "  \"schema_status\": "
        << inferlab_plan::json_string(
               record.fixture_event_count > 0U
                   ? "fixture_schema_available"
                   : "documented_schema_only")
        << ",\n";
    out << nested << "  \"claim_boundary\": "
        << inferlab_plan::json_string(rule.claim_boundary) << "\n";
    out << nested << "}"
        << (index + 1U == plan.records.size() ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string native_alignment_coverage_json(
    const SemanticEventsPlan& plan, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U;
       index < plan.native_alignment_coverage.size(); ++index) {
    const auto& coverage = plan.native_alignment_coverage[index];
    out << nested << "{\n";
    out << nested << "  \"native_artifact\": "
        << inferlab_plan::json_string(
               coverage.native_destination.artifact)
        << ",\n";
    out << nested << "  \"producer_target\": "
        << inferlab_plan::json_string(
               coverage.native_destination.producer_target)
        << ",\n";
    out << nested << "  \"identity_owner\": "
        << inferlab_plan::json_string(
               coverage.native_destination.identity_owner)
        << ",\n";
    out << nested << "  \"refresh_command\": "
        << inferlab_plan::json_string(
               coverage.native_destination.refresh_command)
        << ",\n";
    out << nested << "  \"event_types\": "
        << inferlab_plan::json_string_array(coverage.event_types,
                                             indent + 4)
        << ",\n";
    out << nested << "  \"event_count\": "
        << coverage.event_type_count << ",\n";
    out << nested << "  \"fixture_event_count\": "
        << coverage.fixture_event_type_count << ",\n";
    out << nested << "  \"observed_event_count\": "
        << coverage.replay_event_type_count << ",\n";
    out << nested << "  \"coverage_status\": "
        << inferlab_plan::json_string(coverage.coverage_status) << ",\n";
    out << nested
        << "  \"native_artifact_read\": false\n";
    out << nested << "}"
        << (index + 1U == plan.native_alignment_coverage.size() ? "\n"
                                                                : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string release_pipeline_linkage_json(const SemanticEventsPlan& plan,
                                          const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "[\n";
  for (std::size_t index = 0U;
       index < plan.release_pipeline_linkage.size(); ++index) {
    const auto& linkage = plan.release_pipeline_linkage[index];
    out << nested << "{\n";
    out << nested << "  \"event_type\": "
        << inferlab_plan::json_string(linkage.event_type) << ",\n";
    out << nested << "  \"event_status\": "
        << inferlab_plan::json_string(linkage.event_status) << ",\n";
    out << nested << "  \"release_pipeline_artifact\": "
        << inferlab_plan::json_string(plan.release_pipeline->artifact)
        << ",\n";
    out << nested << "  \"release_pipeline_target\": "
        << inferlab_plan::json_string(
               plan.release_pipeline->target)
        << ",\n";
    out << nested << "  \"producer_direction\": "
        << inferlab_plan::json_string(
               "semantic_events_to_release_pipeline_consumer")
        << ",\n";
    out << nested << "  \"release_pipeline_artifact_read\": false,\n";
    out << nested << "  \"release_impact\": "
        << inferlab_plan::json_string(linkage.release_impact) << ",\n";
    out << nested << "  \"claim_boundary\": "
        << inferlab_plan::json_string(linkage.claim_boundary) << "\n";
    out << nested << "}"
        << (index + 1U == plan.release_pipeline_linkage.size() ? "\n"
                                                               : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string summary_json(const SemanticEventsSummary& summary,
                         const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"available_event_count\": "
      << summary.available_event_count << ",\n";
  out << nested << "\"event_type_count\": "
      << summary.event_type_count << ",\n";
  out << nested << "\"pending_event_count\": "
      << summary.pending_event_count << ",\n";
  out << nested << "\"blocked_event_count\": "
      << summary.blocked_event_count << ",\n";
  out << nested << "\"sample_available_event_count\": "
      << summary.sample_available_event_count << ",\n";
  out << nested << "\"fixture_schema_event_count\": "
      << summary.fixture_schema_event_count << ",\n";
  out << nested << "\"replay_supported_event_count\": "
      << summary.replay_supported_event_count << ",\n";
  out << nested << "\"replay_ready_event_count\": "
      << summary.replay_ready_event_count << ",\n";
  out << nested << "\"sample_event_count\": "
      << summary.sample_event_count << ",\n";
  out << nested << "\"valid_sample_event_count\": "
      << summary.valid_sample_event_count << ",\n";
  out << nested << "\"fixture_sample_event_count\": "
      << summary.fixture_sample_event_count << ",\n";
  out << nested << "\"explicit_replay_sample_event_count\": "
      << summary.explicit_replay_sample_event_count << ",\n";
  out << nested << "\"invalid_sample_event_count\": "
      << summary.invalid_sample_event_count << ",\n";
  out << nested << "\"unsupported_sample_event_count\": "
      << summary.unsupported_sample_event_count << ",\n";
  out << nested << "\"sample_source_count\": "
      << summary.sample_source_count << ",\n";
  out << nested << "\"default_fixture_source_count\": "
      << summary.default_fixture_source_count << ",\n";
  out << nested << "\"explicit_replay_source_count\": "
      << summary.explicit_replay_source_count << ",\n";
  out << nested << "\"eventful_sample_source_count\": "
      << summary.eventful_sample_source_count << ",\n";
  out << nested << "\"present_sample_source_count\": "
      << summary.present_sample_source_count << ",\n";
  out << nested << "\"readable_sample_source_count\": "
      << summary.readable_sample_source_count << ",\n";
  out << nested << "\"source_contract_ready_count\": "
      << summary.source_contract_ready_count << ",\n";
  out << nested << "\"sample_present\": "
      << json_bool(summary.sample_present) << ",\n";
  out << nested << "\"sample_replay_present\": "
      << json_bool(summary.sample_replay_present) << ",\n";
  out << nested << "\"product_profile_count\": "
      << summary.product_profile_count << ",\n";
  out << nested << "\"product_profile_present_count\": "
      << summary.product_profile_present_count << ",\n";
  out << nested << "\"product_profile_contract_ready_count\": "
      << summary.product_profile_contract_ready_count << ",\n";
  out << nested << "\"product_profile_replayed_count\": "
      << summary.product_profile_replayed_count << ",\n";
  out << nested << "\"product_profile_executed_count\": "
      << summary.product_profile_executed_count << ",\n";
  out << nested << "\"read_issue_count\": "
      << summary.read_issue_count << ",\n";
  out << nested << "\"physical_source_capture_count\": "
      << summary.physical_source_capture_count << ",\n";
  out << nested << "\"blocker_count\": " << summary.blocker_count
      << ",\n";
  out << nested << "\"mode\": "
      << inferlab_plan::json_string(summary.mode) << ",\n";
  out << nested << "\"semantic_event_decision\": "
      << inferlab_plan::json_string(summary.semantic_event_decision)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string semantic_events_artifact_json(
    const SemanticEventsPlan& plan,
    const SemanticEventsArtifactViewContext& context) {
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, plan.artifact_identity.expected_artifact_type,
      plan.claim_boundary, plan.product_display_names,
      plan.artifact_identity.producer_target,
      "results/raw/" + plan.artifact_identity.artifact,
      plan.artifact_identity.refresh_command);
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(
             plan.artifact_identity.expected_artifact_type)
      << ",\n";
  out << "  \"mode\": "
      << inferlab_plan::json_string(plan.summary.mode) << ",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": "
      << json_bool(plan.summary.sample_replay_present) << ",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(plan.claim_boundary) << ",\n";
  out << "  \"cli_context\": "
      << cli_context_json(plan, context, 2) << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << ",\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array(plan.recommended_next_commands, 2)
      << ",\n";
  out << "  \"summary\": " << summary_json(plan.summary, 2) << ",\n";
  out << "  \"sample_replay_source\": "
      << (plan.sample_sources.empty()
              ? std::string("null")
              : sample_source_json(plan.sample_sources.front(), 2))
      << ",\n";
  out << "  \"sample_sources\": " << sample_sources_json(plan, 2)
      << ",\n";
  out << "  \"product_profile_sources\": "
      << profile_sources_json(plan, 2) << ",\n";
  out << "  \"normalized_events\": "
      << normalized_events_json(plan, 2) << ",\n";
  out << "  \"request_trace_join\": "
      << request_trace_projection_json(plan.request_trace, 2) << ",\n";
  out << "  \"product_family_coverage\": "
      << product_coverage_json(plan, 2) << ",\n";
  out << "  \"event_schema_coverage\": "
      << event_schema_coverage_json(plan, 2) << ",\n";
  out << "  \"native_alignment_coverage\": "
      << native_alignment_coverage_json(plan, 2) << ",\n";
  out << "  \"release_pipeline_linkage\": "
      << release_pipeline_linkage_json(plan, 2) << ",\n";
  out << "  \"blockers\": "
      << inferlab_plan::json_string_array(plan.blockers, 2) << ",\n";
  out << "  \"records\": " << records_json(plan, 2) << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::product_replay_semantic_events
