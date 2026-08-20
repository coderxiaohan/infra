#include "deep_dive_trace_token_join_policy.hpp"

#include "plan_json_utils.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>

namespace inferlab_deep_dive_trace {
namespace {

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

TraceTokenJoinDecision derive_decision_impl(
    const TraceTokenJoinProjection& projection) {
  const TraceTokenSourceObservation* product = nullptr;
  bool invalid_evaluated_contract = false;
  for (const auto& source : projection.sources) {
    if (source.role == TraceTokenSourceRole::ProductReplay) {
      product = &source;
    }
    if (source.token_contract_evaluated && !source.token_contract_ready) {
      invalid_evaluated_contract = true;
    }
  }
  if (product == nullptr || !product->structural_ready ||
      !product->document_available) {
    return TraceTokenJoinDecision::PendingProductSource;
  }
  if (invalid_evaluated_contract) {
    return TraceTokenJoinDecision::BlockedTokenContract;
  }
  if (projection.summary.ready_native_source_count == 0U) {
    return TraceTokenJoinDecision::PendingNativeSources;
  }
  if (projection.summary.ready_native_source_count <
          projection.summary.native_source_count &&
      projection.summary.matched_trace_group_count == 0U) {
    return TraceTokenJoinDecision::PendingNativeSources;
  }
  if (projection.summary.product_eligible_request_count == 0U ||
      projection.summary.native_eligible_record_count == 0U) {
    return TraceTokenJoinDecision::PendingComparableTokens;
  }
  if (projection.summary.matched_trace_group_count == 0U) {
    return TraceTokenJoinDecision::NoTraceTokenIntersection;
  }
  if (projection.summary.ready_native_source_count <
          projection.summary.native_source_count ||
      projection.summary.unmatched_product_request_count > 0U ||
      projection.summary.unmatched_native_record_count > 0U ||
      projection.summary.multi_product_request_matched_trace_group_count >
          0U) {
    return TraceTokenJoinDecision::PartialTraceTokenCandidateJoin;
  }
  return TraceTokenJoinDecision::TraceTokenCandidateJoinReady;
}

}  // namespace

bool trace_token_input_contains_metric(
    const TraceTokenArtifactInput& input, const std::string& metric) {
  return std::find(input.canonical_required_metrics.begin(),
                   input.canonical_required_metrics.end(), metric) !=
         input.canonical_required_metrics.end();
}

void initialize_trace_token_source(
    TraceTokenSourceObservation& source,
    const TraceTokenArtifactInput& input,
    const TraceTokenSourceRole role) {
  source.role = role;
  source.artifact = input.artifact;
  source.artifact_type = input.artifact_type;
  source.refresh_command = input.refresh_command;
  source.structural_ready = input.structural_ready;
  source.document_available =
      input.document != nullptr && input.semantic_index != nullptr;
  if (!source.structural_ready || !source.document_available) {
    source.source_decision = "artifact_not_structurally_ready";
    append_unique(source.issues, "artifact_not_structurally_ready");
  }
}

void append_trace_token_source_blockers_and_actions(
    TraceTokenJoinProjection& projection,
    const TraceTokenSourceObservation& source) {
  if (source.token_contract_ready &&
      source.parsed_trace_join_token_count > 0U) {
    return;
  }
  if (!source.refresh_command.empty()) {
    append_unique(projection.recommended_commands, source.refresh_command);
  }
  for (const auto& issue : source.issues) {
    append_unique(projection.blockers, source.artifact + ":" + issue);
  }
  if (!source.structural_ready || !source.document_available) {
    append_unique(projection.recommended_actions,
                  "refresh_structural_artifact:" + source.artifact);
  } else if (!source.token_contract_ready) {
    append_unique(projection.recommended_actions,
                  "repair_trace_token_contract:" + source.artifact);
  } else if (source.role == TraceTokenSourceRole::ProductReplay) {
    append_unique(projection.blockers,
                  "product_replay_trace_tokens_unavailable");
    append_unique(projection.recommended_actions,
                  "import_product_replay_rows_with_canonical_trace_id_or_trace_ids");
  } else {
    append_unique(projection.blockers,
                  source.artifact + ":native_trace_tokens_unavailable");
    append_unique(projection.recommended_actions,
                  "refresh_native_artifact_with_trace_context_sidecar:" +
                      source.artifact);
  }
}

const char* trace_token_source_role_name(const TraceTokenSourceRole role) {
  switch (role) {
    case TraceTokenSourceRole::ProductReplay:
      return "product_replay";
    case TraceTokenSourceRole::NativeDeepDive:
      return "native_deep_dive";
  }
  throw std::invalid_argument("unknown trace-token source role");
}

const char* trace_token_join_decision_name(
    const TraceTokenJoinDecision decision) {
  switch (decision) {
    case TraceTokenJoinDecision::PendingProductSource:
      return "pending_product_trace_source";
    case TraceTokenJoinDecision::PendingNativeSources:
      return "pending_native_trace_sources";
    case TraceTokenJoinDecision::BlockedTokenContract:
      return "blocked_trace_token_contract";
    case TraceTokenJoinDecision::PendingComparableTokens:
      return "pending_comparable_trace_tokens";
    case TraceTokenJoinDecision::NoTraceTokenIntersection:
      return "no_trace_token_intersection";
    case TraceTokenJoinDecision::PartialTraceTokenCandidateJoin:
      return "partial_trace_token_candidate_join";
    case TraceTokenJoinDecision::TraceTokenCandidateJoinReady:
      return "trace_token_candidate_join_ready";
  }
  throw std::invalid_argument("unknown trace-token join decision");
}

TraceTokenJoinDecision derive_trace_token_join_decision(
    const TraceTokenJoinProjection& projection) {
  return derive_decision_impl(projection);
}

}  // namespace inferlab_deep_dive_trace
