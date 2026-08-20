#pragma once

#include "product_profile_artifact_contract.hpp"
#include "product_replay_semantic_events_fixture.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab::product_replay_semantic_events {

struct ReplaySampleRequest {
  std::filesystem::path path;
  SampleInputOrigin origin = SampleInputOrigin::default_fixture;
  std::string expected_product_profile_id;
};

struct ProductProfileState {
  const mature_product_profiles::ProductProfileSpec* profile = nullptr;
  std::filesystem::path resolved_path;
  mature_product_profiles::ProductProfileArtifactContract contract;
  bool read_issue = false;
};

struct NormalizedEventRow {
  std::size_t source_index = 0U;
  std::size_t line_number = 0U;
  const SemanticEventRuleSpec* rule = nullptr;
  std::string product_profile_id;
  std::string source_artifact;
  bool json_valid = false;
  bool event_type_known = false;
  bool product_identity_valid = false;
  bool timestamp_present = false;
  bool claim_scope_present = false;
  bool required_fields_valid = false;
  bool fixture_schema_valid = false;
  bool explicit_replay_valid = false;
  bool profile_contract_ready = false;
  bool request_id_present = false;
  bool request_ids_present = false;
  bool trace_context_valid = true;
  bool request_span_context_valid = true;
  bool trace_id_present = false;
  bool trace_ids_present = false;
  bool request_span_id_present = false;
  bool request_span_ids_present = false;
  bool request_terminal_status_present = false;
  bool timestamp_value_present = false;
  std::size_t timestamp_ms = 0U;
  std::string request_id;
  std::vector<std::string> request_ids;
  std::string trace_id;
  std::vector<std::string> trace_ids;
  std::string request_span_id;
  std::vector<std::string> request_span_ids;
  std::string request_terminal_status = "none";
  std::vector<std::string> missing_fields;
  std::vector<std::string> invalid_fields;
  std::vector<std::string> issues;
};

struct SampleSourceState {
  ReplaySampleRequest request;
  bool present = false;
  bool regular_file = false;
  bool readable = false;
  bool size_allowed = false;
  bool source_contract_ready = false;
  bool read_issue = false;
  std::uintmax_t byte_size = 0U;
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string text_read_status = "not_checked";
  std::string text_read_error;
  std::size_t line_count = 0U;
  std::size_t valid_event_count = 0U;
  std::size_t fixture_event_count = 0U;
  std::size_t explicit_replay_event_count = 0U;
  std::size_t invalid_event_count = 0U;
  std::size_t unsupported_event_count = 0U;
  std::vector<std::size_t> event_indices;
  std::vector<std::string> event_types;
  std::vector<std::string> product_profile_ids;
  std::vector<std::string> issues;
};

struct SemanticEventRecord {
  const SemanticEventRuleSpec* rule = nullptr;
  product_replay_alignment::NativeArtifactIdentity native_destination;
  std::vector<std::size_t> product_profile_indices;
  std::vector<std::size_t> sample_source_indices;
  std::vector<std::string> source_artifacts;
  std::vector<std::string> observed_product_profile_ids;
  std::size_t fixture_event_count = 0U;
  std::size_t explicit_replay_event_count = 0U;
  std::size_t replay_ready_event_count = 0U;
  std::size_t invalid_fixture_event_count = 0U;
  std::size_t invalid_explicit_event_count = 0U;
  std::size_t product_profile_present_count = 0U;
  std::size_t product_profile_contract_ready_count = 0U;
  std::size_t product_profile_replayed_count = 0U;
  std::size_t product_profile_executed_count = 0U;
  std::size_t read_issue_count = 0U;
  std::size_t contract_check_count = 0U;
  std::size_t satisfied_contract_check_count = 0U;
  bool source_present = false;
  bool source_replayed = false;
  bool source_executed = false;
  bool sample_observed = false;
  bool observed_signal = false;
  double contract_coverage = 0.0;
  double confidence = 0.0;
  std::string status;
  std::string recommendation;
  std::vector<std::string> issues;
};

struct ProductCoverageRecord {
  std::size_t product_profile_index = 0U;
  std::vector<std::size_t> sample_source_indices;
  std::size_t fixture_event_count = 0U;
  std::size_t explicit_replay_event_count = 0U;
  std::size_t valid_event_count = 0U;
  std::string coverage_status;
};

struct NativeAlignmentCoverageRecord {
  product_replay_alignment::NativeArtifactIdentity native_destination;
  std::vector<std::string> event_types;
  std::size_t event_type_count = 0U;
  std::size_t fixture_event_type_count = 0U;
  std::size_t replay_event_type_count = 0U;
  std::string coverage_status;
};

struct ReleasePipelineLinkageRecord {
  std::string event_type;
  std::string event_status;
  std::string release_impact;
  std::string claim_boundary;
};

struct RequestTraceRecord {
  std::size_t source_index = 0U;
  std::size_t request_ordinal = 0U;
  std::string product_profile_id;
  std::vector<std::string> event_types;
  std::size_t event_membership_count = 0U;
  std::size_t direct_request_event_count = 0U;
  std::size_t batch_membership_event_count = 0U;
  std::size_t timestamped_event_count = 0U;
  std::size_t same_record_identity_timestamp_count = 0U;
  std::size_t fixture_event_count = 0U;
  std::size_t explicit_replay_event_count = 0U;
  std::size_t replay_ready_event_count = 0U;
  std::size_t comparable_order_pair_count = 0U;
  std::size_t ordered_pair_count = 0U;
  std::size_t duplicate_event_count = 0U;
  std::size_t out_of_order_event_count = 0U;
  std::size_t trace_context_event_count = 0U;
  std::size_t request_span_context_event_count = 0U;
  std::size_t request_arrival_event_count = 0U;
  std::size_t request_terminal_event_count = 0U;
  std::size_t trace_relative_window_start_ns = 0U;
  std::size_t trace_relative_window_end_ns = 0U;
  std::size_t clock_uncertainty_ns = 0U;
  bool direct_request_identity_present = false;
  bool batch_membership_identity_present = false;
  bool request_arrival_present = false;
  bool request_terminal_present = false;
  bool prefill_start_present = false;
  bool prefill_end_present = false;
  bool decode_step_present = false;
  bool kv_transfer_start_present = false;
  bool kv_transfer_end_present = false;
  bool multi_event_join = false;
  bool timestamp_complete = false;
  bool ordering_valid = false;
  bool duplicate_free = false;
  bool trace_context_present = false;
  bool trace_context_complete = false;
  bool trace_context_consistent = true;
  bool trace_correlation_eligible = false;
  bool trace_join_token_present = false;
  bool request_span_context_present = false;
  bool request_span_context_complete = false;
  bool request_span_context_consistent = true;
  bool request_span_binding_unique = true;
  bool request_span_lineage_eligible = false;
  bool request_span_join_token_present = false;
  bool span_lifecycle_supplied = false;
  bool span_lifecycle_contract_ready = false;
  bool span_lifecycle_interval_ordered = false;
  bool span_lifecycle_events_contained = false;
  bool clock_calibration_eligible = false;
  bool relative_time_window_present = false;
  bool replay_join_ready = false;
  std::string trace_join_token = "none";
  std::string request_span_join_token = "none";
  std::string span_terminal_status = "none";
  std::string span_lifecycle_status = "not_provided";
  std::string clock_domain = "product_log_ms";
  std::string clock_calibration_status = "not_applicable";
  std::string status = "not_evaluated";
  std::vector<std::string> issues;
};

struct RequestTraceSummary {
  std::size_t schema_valid_event_count = 0U;
  std::size_t keyed_event_count = 0U;
  std::size_t unkeyed_event_count = 0U;
  std::size_t event_membership_count = 0U;
  std::size_t direct_request_event_membership_count = 0U;
  std::size_t batch_request_event_membership_count = 0U;
  std::size_t request_count = 0U;
  std::size_t multi_event_request_count = 0U;
  std::size_t singleton_request_count = 0U;
  std::size_t fixture_request_count = 0U;
  std::size_t explicit_replay_request_count = 0U;
  std::size_t replay_ready_request_count = 0U;
  std::size_t timestamp_complete_request_count = 0U;
  std::size_t comparable_order_pair_count = 0U;
  std::size_t ordered_pair_count = 0U;
  std::size_t duplicate_event_count = 0U;
  std::size_t out_of_order_event_count = 0U;
  std::size_t trace_context_event_membership_count = 0U;
  std::size_t request_span_context_event_membership_count = 0U;
  std::size_t trace_context_request_count = 0U;
  std::size_t trace_context_complete_request_count = 0U;
  std::size_t trace_context_consistent_request_count = 0U;
  std::size_t trace_context_conflict_request_count = 0U;
  std::size_t trace_correlation_eligible_request_count = 0U;
  std::size_t trace_context_distinct_id_count = 0U;
  std::size_t trace_join_token_count = 0U;
  std::size_t distinct_trace_join_token_count = 0U;
  std::size_t trace_join_token_collision_count = 0U;
  std::size_t request_span_context_request_count = 0U;
  std::size_t request_span_context_complete_request_count = 0U;
  std::size_t request_span_context_consistent_request_count = 0U;
  std::size_t request_span_context_conflict_request_count = 0U;
  std::size_t request_span_binding_conflict_request_count = 0U;
  std::size_t request_span_lineage_eligible_request_count = 0U;
  std::size_t request_span_join_token_count = 0U;
  std::size_t distinct_request_span_join_token_count = 0U;
  std::size_t span_lifecycle_supplied_request_count = 0U;
  std::size_t span_lifecycle_ready_request_count = 0U;
  std::size_t span_lifecycle_blocked_request_count = 0U;
  std::size_t span_lifecycle_completed_request_count = 0U;
  std::size_t span_lifecycle_error_request_count = 0U;
  std::size_t span_lifecycle_cancelled_request_count = 0U;
  std::size_t span_lifecycle_rejected_request_count = 0U;
  std::size_t clock_calibration_eligible_request_count = 0U;
  std::size_t clock_calibrated_request_count = 0U;
  std::size_t clock_uncalibrated_request_count = 0U;
  std::size_t blocker_count = 0U;
  bool clock_normalization_performed = false;
  std::string identity_scope = "source_product_request_id";
  std::string trace_context_scope =
      "w3c_trace_id_anonymous_per_request_tokens_and_set_summary";
  std::string trace_join_token_algorithm;
  std::string request_span_binding_scope =
      "w3c_trace_id_plus_explicit_request_span_id";
  std::string request_span_join_token_algorithm;
  std::string span_lifecycle_contract =
      "one_request_arrival_one_request_terminal_all_events_contained";
  std::string trace_correlation_fingerprint_algorithm;
  std::string trace_correlation_set_fingerprint = "none";
  std::string timestamp_unit = "milliseconds";
  std::string normalized_timestamp_unit = "trace_relative_nanoseconds";
  std::string decision = "pending_request_trace_evidence";
};

struct RequestTraceProjection {
  std::vector<RequestTraceRecord> records;
  std::vector<std::string> blockers;
  RequestTraceSummary summary;
  inferlab_trace_clock::CalibrationUsageProjection clock_calibration;
  std::string claim_boundary;
};

struct SemanticEventsSummary {
  std::size_t event_type_count = 0U;
  std::size_t available_event_count = 0U;
  std::size_t sample_available_event_count = 0U;
  std::size_t pending_event_count = 0U;
  std::size_t blocked_event_count = 0U;
  std::size_t fixture_schema_event_count = 0U;
  std::size_t replay_supported_event_count = 0U;
  std::size_t replay_ready_event_count = 0U;
  std::size_t sample_source_count = 0U;
  std::size_t default_fixture_source_count = 0U;
  std::size_t explicit_replay_source_count = 0U;
  std::size_t present_sample_source_count = 0U;
  std::size_t readable_sample_source_count = 0U;
  std::size_t eventful_sample_source_count = 0U;
  std::size_t source_contract_ready_count = 0U;
  std::size_t sample_event_count = 0U;
  std::size_t valid_sample_event_count = 0U;
  std::size_t fixture_sample_event_count = 0U;
  std::size_t explicit_replay_sample_event_count = 0U;
  std::size_t invalid_sample_event_count = 0U;
  std::size_t unsupported_sample_event_count = 0U;
  std::size_t product_profile_count = 0U;
  std::size_t product_profile_present_count = 0U;
  std::size_t product_profile_contract_ready_count = 0U;
  std::size_t product_profile_replayed_count = 0U;
  std::size_t product_profile_executed_count = 0U;
  std::size_t read_issue_count = 0U;
  std::size_t physical_source_capture_count = 0U;
  std::size_t blocker_count = 0U;
  bool sample_present = false;
  bool sample_replay_present = false;
  std::string mode = "dry_run";
  std::string semantic_event_decision = "pending_replay_evidence";
};

struct SemanticEventsPlan {
  product_replay_alignment::NativeArtifactIdentity artifact_identity;
  const inferlab_high_value::ManagedArtifact* release_pipeline = nullptr;
  std::vector<ProductProfileState> product_profiles;
  std::vector<SampleSourceState> sample_sources;
  std::vector<NormalizedEventRow> normalized_events;
  std::vector<SemanticEventRecord> records;
  std::vector<ProductCoverageRecord> product_coverage;
  std::vector<NativeAlignmentCoverageRecord> native_alignment_coverage;
  std::vector<ReleasePipelineLinkageRecord> release_pipeline_linkage;
  RequestTraceProjection request_trace;
  std::vector<std::string> product_display_names;
  std::vector<std::string> recommended_next_commands;
  std::vector<std::string> blockers;
  SemanticEventsSummary summary;
  std::string claim_boundary;
};

const mature_product_profiles::ProductProfileSpec& product_profile(
    const ProductProfileState& state);
const SemanticEventRuleSpec& semantic_event_rule(
    const NormalizedEventRow& row);
const SemanticEventRuleSpec& semantic_event_rule(
    const SemanticEventRecord& record);
RequestTraceProjection build_request_trace_projection(
    const std::vector<NormalizedEventRow>& events,
    const std::vector<SampleSourceState>& sources);
RequestTraceProjection build_request_trace_projection(
    const std::vector<NormalizedEventRow>& events,
    const std::vector<SampleSourceState>& sources,
    inferlab_trace_clock::CalibrationSource clock_calibration);
SemanticEventsPlan build_semantic_events_plan(
    const std::filesystem::path& results_root,
    const std::vector<ReplaySampleRequest>& sample_requests);
SemanticEventsPlan build_semantic_events_plan(
    const std::filesystem::path& results_root,
    const std::vector<ReplaySampleRequest>& sample_requests,
    inferlab_trace_clock::CalibrationSource clock_calibration);

}  // namespace inferlab::product_replay_semantic_events
