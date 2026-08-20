#pragma once

#include "artifact_semantic_index.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_deep_dive_trace {

enum class TraceTokenSourceRole {
  ProductReplay,
  NativeDeepDive,
};

enum class TraceTokenJoinDecision {
  PendingProductSource,
  PendingNativeSources,
  BlockedTokenContract,
  PendingComparableTokens,
  NoTraceTokenIntersection,
  PartialTraceTokenCandidateJoin,
  TraceTokenCandidateJoinReady,
};

enum class TraceTimeWindowDecision {
  PendingTraceCandidateGroups,
  PendingSingleProductRequestGroups,
  PendingClockCalibration,
  PartialTimeWindowEvidence,
  TimeWindowEvidenceReady,
};

enum class TraceSpanLineageDecision {
  PendingTraceCandidateGroups,
  PendingRequestSpanBindings,
  BlockedLineageContract,
  PartialSpanLineageEvidence,
  SameRequestLineageEvidenceReady,
};

enum class TraceSpanLifecycleDecision {
  PendingTraceCandidateGroups,
  PendingLifecycleEvidence,
  BlockedLifecycleContract,
  BlockedDeclaredTemporalOrder,
  PartialDeclaredTemporalEvidence,
  DeclaredHappensBeforeEvidenceReady,
};

struct TraceTokenArtifactInput {
  std::string artifact;
  std::string artifact_type;
  std::string refresh_command;
  std::vector<std::string> canonical_required_metrics;
  bool structural_ready = false;
  const inferlab_artifact::ArtifactSemanticIndex* semantic_index = nullptr;
  const std::string* document = nullptr;
};

struct TraceTokenSourceObservation {
  TraceTokenSourceRole role = TraceTokenSourceRole::NativeDeepDive;
  std::string artifact;
  std::string artifact_type;
  std::string domain = "not_applicable";
  std::string refresh_command;
  std::string source_decision = "not_evaluated";
  bool structural_ready = false;
  bool document_available = false;
  bool token_contract_evaluated = false;
  bool token_contract_ready = false;
  bool join_algorithm_matches = false;
  bool summary_matches_records = false;
  bool clock_calibration_contract_evaluated = false;
  bool clock_calibration_contract_ready = false;
  bool clock_calibration_source_supplied = false;
  bool clock_calibration_all_lookups_calibrated = false;
  bool clock_normalization_performed = false;
  bool span_lineage_contract_evaluated = false;
  bool span_lineage_contract_ready = false;
  bool span_lifecycle_contract_evaluated = false;
  bool span_lifecycle_contract_ready = false;
  std::size_t declared_record_count = 0U;
  std::size_t parsed_record_count = 0U;
  std::size_t declared_eligible_record_count = 0U;
  std::size_t parsed_eligible_record_count = 0U;
  std::size_t declared_trace_join_token_count = 0U;
  std::size_t parsed_trace_join_token_count = 0U;
  std::size_t declared_distinct_trace_join_token_count = 0U;
  std::size_t parsed_distinct_trace_join_token_count = 0U;
  std::size_t declared_clock_calibration_lookup_count = 0U;
  std::size_t declared_clock_calibrated_timestamp_count = 0U;
  std::size_t declared_clock_uncalibrated_timestamp_count = 0U;
  std::size_t declared_request_span_join_token_count = 0U;
  std::size_t parsed_request_span_join_token_count = 0U;
  std::size_t declared_span_join_token_count = 0U;
  std::size_t parsed_span_join_token_count = 0U;
  std::size_t declared_span_lifecycle_record_count = 0U;
  std::size_t parsed_span_lifecycle_record_count = 0U;
  std::size_t declared_causal_predecessor_count = 0U;
  std::size_t parsed_causal_predecessor_count = 0U;
  std::string clock_calibration_fingerprint = "none";
  std::string clock_calibration_fingerprint_algorithm;
  std::string clock_calibration_decision = "not_evaluated";
  std::vector<std::string> span_lineage_issues;
  std::vector<std::string> span_lifecycle_issues;
  std::vector<std::string> issues;
};

struct ProductTraceReference {
  std::size_t source_index = 0U;
  std::size_t request_ordinal = 0U;
  std::string product_profile_id;
  bool request_span_binding_present = false;
  std::string request_span_join_token = "none";
  bool relative_time_window_present = false;
  std::size_t trace_relative_window_start_ns = 0U;
  std::size_t trace_relative_window_end_ns = 0U;
  std::size_t clock_uncertainty_ns = 0U;
  std::string clock_calibration_fingerprint = "none";
  bool span_lifecycle_supplied = false;
  bool span_lifecycle_contract_ready = false;
  std::string span_terminal_status = "none";
  std::string span_lifecycle_status = "not_provided";
};

struct NativeTraceReference {
  std::string artifact;
  std::string domain;
  std::size_t record_ordinal = 0U;
  std::string record_key;
  std::string span_role;
  bool parent_span_present = false;
  std::string span_join_token = "none";
  std::string parent_span_join_token = "none";
  bool same_request_lineage_proven = false;
  std::size_t matched_product_source_index = 0U;
  std::size_t matched_product_request_ordinal = 0U;
  std::size_t request_lineage_depth = 0U;
  std::string request_lineage_relation = "not_evaluated";
  bool relative_timestamp_present = false;
  std::size_t trace_relative_timestamp_ns = 0U;
  std::size_t clock_uncertainty_ns = 0U;
  std::string clock_calibration_fingerprint = "none";
  std::string time_window_relation = "not_evaluated";
  bool span_lifecycle_supplied = false;
  bool span_lifecycle_contract_ready = false;
  bool relative_span_lifecycle_present = false;
  std::size_t trace_relative_span_start_ns = 0U;
  std::size_t trace_relative_span_end_ns = 0U;
  std::size_t span_lifecycle_clock_uncertainty_ns = 0U;
  std::string span_terminal_status = "none";
  std::string span_lifecycle_status = "not_provided";
  bool causal_predecessor_span_present = false;
  std::string causal_predecessor_span_join_token = "none";
  std::string declared_temporal_relation = "not_evaluated";
};

struct TraceTokenCandidateGroup {
  std::size_t join_group_ordinal = 0U;
  std::vector<ProductTraceReference> product_requests;
  std::vector<NativeTraceReference> native_records;
  bool one_product_request = false;
  bool same_request_proven = false;
  bool span_lineage_evaluated = false;
  std::size_t request_span_bound_product_request_count = 0U;
  std::size_t lineage_resolved_native_record_count = 0U;
  std::size_t lineage_unresolved_native_record_count = 0U;
  std::size_t lineage_conflict_native_record_count = 0U;
  std::string span_lineage_status = "not_evaluated";
  bool span_lifecycle_evaluated = false;
  std::size_t lifecycle_ready_product_request_count = 0U;
  std::size_t lifecycle_ready_native_record_count = 0U;
  std::size_t declared_causal_edge_count = 0U;
  std::size_t resolved_causal_edge_count = 0U;
  std::size_t declared_happens_before_edge_count = 0U;
  std::size_t indeterminate_causal_edge_count = 0U;
  std::size_t conflict_causal_edge_count = 0U;
  std::string span_lifecycle_status = "not_evaluated";
  bool clock_calibration_fingerprint_matches = false;
  bool time_window_comparison_performed = false;
  std::size_t time_window_compatible_native_record_count = 0U;
  std::size_t time_window_excluded_native_record_count = 0U;
  std::size_t time_window_indeterminate_native_record_count = 0U;
  std::string time_window_status = "not_evaluated";
  std::string status;
};

struct UnmatchedProductTrace {
  ProductTraceReference request;
  std::string reason;
};

struct UnmatchedNativeTrace {
  NativeTraceReference record;
  std::string reason;
};

struct TraceTokenJoinSummary {
  std::size_t source_count = 0U;
  std::size_t ready_source_count = 0U;
  std::size_t product_source_count = 0U;
  std::size_t native_source_count = 0U;
  std::size_t ready_native_source_count = 0U;
  std::size_t product_request_count = 0U;
  std::size_t product_eligible_request_count = 0U;
  std::size_t native_record_count = 0U;
  std::size_t native_eligible_record_count = 0U;
  std::size_t matched_trace_group_count = 0U;
  std::size_t single_product_request_matched_trace_group_count = 0U;
  std::size_t multi_product_request_matched_trace_group_count = 0U;
  std::size_t matched_product_request_count = 0U;
  std::size_t unmatched_product_request_count = 0U;
  std::size_t matched_native_record_count = 0U;
  std::size_t unmatched_native_record_count = 0U;
  std::size_t source_contract_issue_count = 0U;
  std::size_t span_lineage_source_contract_issue_count = 0U;
  std::size_t span_lineage_ready_source_count = 0U;
  std::size_t request_span_bound_product_request_count = 0U;
  std::size_t span_lineage_evaluated_group_count = 0U;
  std::size_t same_request_lineage_proven_group_count = 0U;
  std::size_t lineage_resolved_native_record_count = 0U;
  std::size_t lineage_unresolved_native_record_count = 0U;
  std::size_t lineage_conflict_native_record_count = 0U;
  std::size_t span_lifecycle_source_contract_issue_count = 0U;
  std::size_t span_lifecycle_ready_source_count = 0U;
  std::size_t lifecycle_ready_product_request_count = 0U;
  std::size_t lifecycle_ready_native_record_count = 0U;
  std::size_t span_lifecycle_evaluated_group_count = 0U;
  std::size_t declared_causal_edge_count = 0U;
  std::size_t resolved_causal_edge_count = 0U;
  std::size_t declared_happens_before_edge_count = 0U;
  std::size_t indeterminate_causal_edge_count = 0U;
  std::size_t conflict_causal_edge_count = 0U;
  std::size_t single_request_time_window_group_count = 0U;
  std::size_t ambiguous_time_window_native_record_count = 0U;
  std::size_t time_window_comparison_count = 0U;
  std::size_t time_window_compatible_native_record_count = 0U;
  std::size_t time_window_excluded_native_record_count = 0U;
  std::size_t time_window_indeterminate_native_record_count = 0U;
  std::size_t blocker_count = 0U;
  bool comparison_performed = false;
  bool candidate_join_present = false;
  bool time_window_filter_performed = false;
  bool span_lineage_evaluation_performed = false;
  bool span_lifecycle_evaluation_performed = false;
  TraceTokenJoinDecision decision =
      TraceTokenJoinDecision::PendingProductSource;
  TraceTimeWindowDecision time_window_decision =
      TraceTimeWindowDecision::PendingTraceCandidateGroups;
  TraceSpanLineageDecision span_lineage_decision =
      TraceSpanLineageDecision::PendingTraceCandidateGroups;
  TraceSpanLifecycleDecision span_lifecycle_decision =
      TraceSpanLifecycleDecision::PendingTraceCandidateGroups;
};

struct TraceTokenJoinProjection {
  std::vector<TraceTokenSourceObservation> sources;
  std::vector<TraceTokenCandidateGroup> matched_groups;
  std::vector<UnmatchedProductTrace> unmatched_product_requests;
  std::vector<UnmatchedNativeTrace> unmatched_native_records;
  std::vector<std::string> blockers;
  std::vector<std::string> recommended_commands;
  std::vector<std::string> recommended_actions;
  std::vector<std::string> time_window_limitations;
  std::vector<std::string> time_window_recommended_actions;
  std::vector<std::string> span_lineage_limitations;
  std::vector<std::string> span_lineage_recommended_actions;
  std::vector<std::string> span_lifecycle_limitations;
  std::vector<std::string> span_lifecycle_recommended_actions;
  TraceTokenJoinSummary summary;
  std::string join_token_algorithm;
  std::string claim_boundary;
};

const char* trace_token_source_role_name(TraceTokenSourceRole role);
const char* trace_token_join_decision_name(TraceTokenJoinDecision decision);
const char* trace_time_window_decision_name(TraceTimeWindowDecision decision);
const char* trace_span_lineage_decision_name(
    TraceSpanLineageDecision decision);
const char* trace_span_lifecycle_decision_name(
    TraceSpanLifecycleDecision decision);

TraceTokenJoinDecision derive_trace_token_join_decision(
    const TraceTokenJoinProjection& projection);

TraceTokenJoinProjection build_trace_token_join_projection(
    const std::vector<TraceTokenArtifactInput>& inputs);

void validate_trace_token_join_projection(
    const TraceTokenJoinProjection& projection);

}  // namespace inferlab_deep_dive_trace
