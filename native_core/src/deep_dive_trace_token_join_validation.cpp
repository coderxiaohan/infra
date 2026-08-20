#include "deep_dive_trace_token_join.hpp"

#include "checked_index.hpp"
#include "deep_dive_trace_span_lifecycle.hpp"
#include "deep_dive_trace_span_lineage.hpp"
#include "deep_dive_trace_time_window.hpp"
#include "native_trace_context.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_correlation_contract.hpp"

#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace inferlab_deep_dive_trace {

void validate_trace_token_join_projection(
    const TraceTokenJoinProjection& projection) {
  if (projection.join_token_algorithm !=
          inferlab_trace::trace_context_join_token_algorithm() ||
      projection.claim_boundary.empty() || projection.sources.empty() ||
      projection.summary.source_count != projection.sources.size() ||
      projection.summary.product_source_count != 1U ||
      projection.summary.native_source_count == 0U ||
      projection.summary.ready_source_count >
          projection.summary.source_count ||
      projection.summary.ready_native_source_count >
          projection.summary.native_source_count ||
      projection.summary.matched_trace_group_count !=
          projection.matched_groups.size() ||
      projection.summary.unmatched_product_request_count !=
          projection.unmatched_product_requests.size() ||
      projection.summary.unmatched_native_record_count !=
          projection.unmatched_native_records.size() ||
      projection.summary.blocker_count != projection.blockers.size() ||
      projection.summary.candidate_join_present !=
          !projection.matched_groups.empty()) {
    throw std::logic_error(
        "trace-token join projection top-level contract drift");
  }

  std::set<std::string> source_artifacts;
  std::size_t product_source_count = 0U;
  std::size_t ready_product_source_count = 0U;
  std::size_t native_source_count = 0U;
  std::size_t ready_source_count = 0U;
  std::size_t ready_native_source_count = 0U;
  std::size_t source_issue_count = 0U;
  std::size_t product_request_count = 0U;
  std::size_t product_eligible_count = 0U;
  std::size_t native_record_count = 0U;
  std::size_t native_eligible_count = 0U;
  for (const auto& source : projection.sources) {
    if (source.artifact.empty() || source.artifact_type.empty() ||
        source.refresh_command.empty() ||
        !source_artifacts.insert(source.artifact).second ||
        source.parsed_eligible_record_count > source.parsed_record_count ||
        source.parsed_trace_join_token_count !=
            source.parsed_eligible_record_count ||
        source.parsed_distinct_trace_join_token_count >
            source.parsed_trace_join_token_count ||
        (source.clock_calibration_contract_ready &&
         !source.clock_calibration_contract_evaluated) ||
        (source.clock_calibration_contract_evaluated &&
         (!inferlab_trace_clock::valid_clock_calibration_fingerprint(
              source.clock_calibration_fingerprint) ||
          source.clock_calibration_fingerprint_algorithm !=
              inferlab_trace_clock::
                  clock_calibration_fingerprint_algorithm() ||
          inferlab::internal::checked_add_size(
              source.declared_clock_calibrated_timestamp_count,
              source.declared_clock_uncalibrated_timestamp_count,
              "trace-token reconciled clock usage partition") !=
              source.declared_clock_calibration_lookup_count ||
          source.clock_calibration_all_lookups_calibrated !=
              (source.declared_clock_calibration_lookup_count > 0U &&
               source.declared_clock_calibrated_timestamp_count ==
                   source.declared_clock_calibration_lookup_count) ||
          source.clock_normalization_performed !=
              (source.declared_clock_calibrated_timestamp_count > 0U) ||
          source.clock_calibration_decision !=
              inferlab_trace_clock::clock_calibration_usage_decision(
                  source.clock_calibration_source_supplied,
                  source.declared_clock_calibration_lookup_count,
                  source.declared_clock_calibrated_timestamp_count) ||
          (source.clock_calibration_source_supplied
               ? source.clock_calibration_fingerprint == "none"
               : source.clock_calibration_fingerprint != "none"))) ||
        (source.token_contract_ready &&
         (!source.token_contract_evaluated || !source.structural_ready ||
          !source.document_available || !source.join_algorithm_matches ||
          !source.summary_matches_records ||
          !source.clock_calibration_contract_evaluated ||
          !source.clock_calibration_contract_ready ||
          !source.issues.empty()))) {
      throw std::logic_error(
          "trace-token join source observation contract drift");
    }
    source_issue_count = inferlab::internal::checked_add_size(
        source_issue_count, source.issues.size(),
        "trace-token reconciled source issue count");
    if (source.role == TraceTokenSourceRole::ProductReplay) {
      product_source_count = inferlab::internal::checked_add_size(
          product_source_count, 1U,
          "trace-token reconciled product source count");
      if (source.token_contract_ready) {
        ready_product_source_count =
            inferlab::internal::checked_add_size(
                ready_product_source_count, 1U,
                "trace-token reconciled ready product source count");
        product_request_count = source.parsed_record_count;
        product_eligible_count = source.parsed_eligible_record_count;
      }
    } else {
      native_source_count = inferlab::internal::checked_add_size(
          native_source_count, 1U,
          "trace-token reconciled native source count");
      if (source.token_contract_ready) {
        ready_native_source_count = inferlab::internal::checked_add_size(
            ready_native_source_count, 1U,
            "trace-token reconciled ready native source count");
        native_record_count = inferlab::internal::checked_add_size(
            native_record_count, source.parsed_record_count,
            "trace-token reconciled native record count");
        native_eligible_count = inferlab::internal::checked_add_size(
            native_eligible_count, source.parsed_eligible_record_count,
            "trace-token reconciled native eligible count");
      }
    }
    if (source.token_contract_ready) {
      ready_source_count = inferlab::internal::checked_add_size(
          ready_source_count, 1U,
          "trace-token reconciled ready source count");
    }
  }

  std::size_t matched_product_count = 0U;
  std::size_t matched_native_count = 0U;
  std::size_t unambiguous_group_count = 0U;
  std::size_t ambiguous_group_count = 0U;
  std::set<std::pair<std::size_t, std::size_t>> product_references;
  std::set<std::pair<std::string, std::string>> native_references;
  for (std::size_t index = 0U; index < projection.matched_groups.size();
       ++index) {
    const auto& group = projection.matched_groups[index];
    if (group.join_group_ordinal !=
            inferlab::internal::checked_add_size(
                index, 1U, "trace-token reconciled group ordinal") ||
        group.product_requests.empty() || group.native_records.empty() ||
        group.one_product_request !=
            (group.product_requests.size() == 1U) ||
        group.status !=
            (group.one_product_request
                 ? "trace_scope_candidate_match"
                 : "ambiguous_trace_scope_multiple_product_requests")) {
      throw std::logic_error(
          "trace-token candidate group contract drift");
    }
    for (const auto& request : group.product_requests) {
      if (request.request_ordinal == 0U ||
          request.product_profile_id.empty() ||
          !product_references
               .emplace(request.source_index, request.request_ordinal)
               .second) {
        throw std::logic_error(
            "trace-token candidate product reference is invalid or duplicated");
      }
    }
    for (const auto& record : group.native_records) {
      if (record.artifact.empty() ||
          !inferlab_native_trace::trace_domain_span_role_matches(
              record.domain, record.span_role) ||
          record.record_ordinal == 0U ||
          !inferlab_native_trace::valid_trace_record_key(record.record_key) ||
          !native_references.emplace(record.artifact, record.record_key)
               .second) {
        throw std::logic_error(
            "trace-token candidate native reference is invalid or duplicated");
      }
    }
    matched_product_count = inferlab::internal::checked_add_size(
        matched_product_count, group.product_requests.size(),
        "trace-token reconciled matched product count");
    matched_native_count = inferlab::internal::checked_add_size(
        matched_native_count, group.native_records.size(),
        "trace-token reconciled matched native count");
    if (group.one_product_request) {
      unambiguous_group_count = inferlab::internal::checked_add_size(
          unambiguous_group_count, 1U,
          "trace-token reconciled unambiguous group count");
    } else {
      ambiguous_group_count = inferlab::internal::checked_add_size(
          ambiguous_group_count, 1U,
          "trace-token reconciled ambiguous group count");
    }
  }
  for (const auto& item : projection.unmatched_product_requests) {
    const auto& request = item.request;
    if (item.reason !=
            "no_native_record_with_equal_anonymous_trace_token" ||
        request.request_ordinal == 0U ||
        request.product_profile_id.empty() ||
        !product_references
             .emplace(request.source_index, request.request_ordinal)
             .second) {
      throw std::logic_error(
          "trace-token unmatched product reference is invalid or duplicated");
    }
  }
  for (const auto& item : projection.unmatched_native_records) {
    const auto& record = item.record;
    if (item.reason !=
            "no_product_request_with_equal_anonymous_trace_token" ||
        record.artifact.empty() ||
        !inferlab_native_trace::trace_domain_span_role_matches(
            record.domain, record.span_role) ||
        record.record_ordinal == 0U ||
        !inferlab_native_trace::valid_trace_record_key(record.record_key) ||
        !native_references.emplace(record.artifact, record.record_key)
             .second) {
      throw std::logic_error(
          "trace-token unmatched native reference is invalid or duplicated");
    }
  }

  const bool expected_comparison = ready_product_source_count == 1U &&
                                   ready_native_source_count > 0U;
  if (product_source_count != projection.summary.product_source_count ||
      native_source_count != projection.summary.native_source_count ||
      ready_source_count != projection.summary.ready_source_count ||
      ready_native_source_count !=
          projection.summary.ready_native_source_count ||
      source_issue_count != projection.summary.source_contract_issue_count ||
      product_request_count != projection.summary.product_request_count ||
      product_eligible_count !=
          projection.summary.product_eligible_request_count ||
      native_record_count != projection.summary.native_record_count ||
      native_eligible_count !=
          projection.summary.native_eligible_record_count ||
      matched_product_count !=
          projection.summary.matched_product_request_count ||
      matched_native_count !=
          projection.summary.matched_native_record_count ||
      unambiguous_group_count !=
          projection.summary
              .single_product_request_matched_trace_group_count ||
      ambiguous_group_count !=
          projection.summary
              .multi_product_request_matched_trace_group_count ||
      inferlab::internal::checked_add_size(
          projection.summary
              .single_product_request_matched_trace_group_count,
          projection.summary
              .multi_product_request_matched_trace_group_count,
          "trace-token matched group partition") !=
          projection.summary.matched_trace_group_count ||
      projection.summary.comparison_performed != expected_comparison ||
      (projection.summary.comparison_performed &&
       (product_references.size() !=
            projection.summary.product_eligible_request_count ||
        native_references.size() !=
            projection.summary.native_eligible_record_count ||
        inferlab::internal::checked_add_size(
            projection.summary.matched_product_request_count,
            projection.summary.unmatched_product_request_count,
            "trace-token product match partition") !=
            projection.summary.product_eligible_request_count ||
        inferlab::internal::checked_add_size(
            projection.summary.matched_native_record_count,
            projection.summary.unmatched_native_record_count,
            "trace-token native match partition") !=
            projection.summary.native_eligible_record_count)) ||
      projection.summary.decision !=
          derive_trace_token_join_decision(projection)) {
    throw std::logic_error(
        "trace-token join summary reconciliation failed");
  }

  std::set<std::string> commands;
  for (const auto& command : projection.recommended_commands) {
    if (command.empty() || !commands.insert(command).second) {
      throw std::logic_error(
          "trace-token join recommended command is empty or duplicated");
    }
  }
  std::set<std::string> actions;
  for (const auto& action : projection.recommended_actions) {
    if (action.empty() || !actions.insert(action).second) {
      throw std::logic_error(
          "trace-token join recommended action is empty or duplicated");
    }
  }
  validate_trace_time_window_projection(projection);
  validate_trace_span_lineage_projection(projection);
  validate_trace_span_lifecycle_projection(projection);
}

}  // namespace inferlab_deep_dive_trace
