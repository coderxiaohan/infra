#include "deep_dive_trace_time_window.hpp"

#include "checked_index.hpp"
#include "trace_clock_calibration.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>

namespace inferlab_deep_dive_trace {
namespace {

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

bool windows_overlap(const ProductTraceReference& request,
                     const NativeTraceReference& record) {
  const std::size_t request_lower =
      inferlab_trace_lifecycle::uncertainty_lower_bound(
      request.trace_relative_window_start_ns,
      request.clock_uncertainty_ns);
  const std::size_t request_upper =
      inferlab_trace_lifecycle::uncertainty_upper_bound(
      request.trace_relative_window_end_ns,
      request.clock_uncertainty_ns);
  const std::size_t native_lower =
      inferlab_trace_lifecycle::uncertainty_lower_bound(
      record.trace_relative_timestamp_ns, record.clock_uncertainty_ns);
  const std::size_t native_upper =
      inferlab_trace_lifecycle::uncertainty_upper_bound(
      record.trace_relative_timestamp_ns, record.clock_uncertainty_ns);
  return std::max(request_lower, native_lower) <=
         std::min(request_upper, native_upper);
}

void validate_product_time_reference(
    const ProductTraceReference& request) {
  if (request.relative_time_window_present) {
    if (request.trace_relative_window_start_ns >
            request.trace_relative_window_end_ns ||
        request.trace_relative_window_end_ns >
            inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
        request.clock_uncertainty_ns >
            inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
        !inferlab_trace_clock::valid_clock_calibration_fingerprint(
            request.clock_calibration_fingerprint) ||
        request.clock_calibration_fingerprint == "none") {
      throw std::logic_error(
          "product trace relative time-window contract drift");
    }
  } else if (request.trace_relative_window_start_ns != 0U ||
             request.trace_relative_window_end_ns != 0U ||
             request.clock_uncertainty_ns != 0U ||
             request.clock_calibration_fingerprint != "none") {
    throw std::logic_error(
        "uncalibrated product trace reference exposes time values");
  }
}

void validate_native_time_reference(
    const NativeTraceReference& record) {
  if (record.relative_timestamp_present) {
    if (record.trace_relative_timestamp_ns >
            inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
        record.clock_uncertainty_ns >
            inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
        !inferlab_trace_clock::valid_clock_calibration_fingerprint(
            record.clock_calibration_fingerprint) ||
        record.clock_calibration_fingerprint == "none") {
      throw std::logic_error(
          "native trace relative timestamp contract drift");
    }
  } else if (record.trace_relative_timestamp_ns != 0U ||
             record.clock_uncertainty_ns != 0U ||
             record.clock_calibration_fingerprint != "none") {
    throw std::logic_error(
        "uncalibrated native trace reference exposes time values");
  }
}

TraceTimeWindowDecision derive_time_window_decision(
    const TraceTokenJoinProjection& projection) {
  if (projection.matched_groups.empty()) {
    return TraceTimeWindowDecision::PendingTraceCandidateGroups;
  }
  if (projection.summary.single_request_time_window_group_count == 0U) {
    return TraceTimeWindowDecision::PendingSingleProductRequestGroups;
  }
  if (projection.summary.time_window_comparison_count == 0U) {
    return TraceTimeWindowDecision::PendingClockCalibration;
  }
  if (projection.summary.time_window_indeterminate_native_record_count > 0U ||
      projection.summary.ambiguous_time_window_native_record_count > 0U) {
    return TraceTimeWindowDecision::PartialTimeWindowEvidence;
  }
  return TraceTimeWindowDecision::TimeWindowEvidenceReady;
}

}  // namespace

const char* trace_time_window_decision_name(
    const TraceTimeWindowDecision decision) {
  switch (decision) {
    case TraceTimeWindowDecision::PendingTraceCandidateGroups:
      return "pending_trace_candidate_groups";
    case TraceTimeWindowDecision::PendingSingleProductRequestGroups:
      return "pending_single_product_request_trace_groups";
    case TraceTimeWindowDecision::PendingClockCalibration:
      return "pending_clock_calibration";
    case TraceTimeWindowDecision::PartialTimeWindowEvidence:
      return "partial_trace_time_window_evidence";
    case TraceTimeWindowDecision::TimeWindowEvidenceReady:
      return "trace_time_window_evidence_ready";
  }
  throw std::invalid_argument("unknown trace time-window decision");
}

void evaluate_trace_time_windows(TraceTokenJoinProjection& projection) {
  projection.summary.single_request_time_window_group_count = 0U;
  projection.summary.ambiguous_time_window_native_record_count = 0U;
  projection.summary.time_window_comparison_count = 0U;
  projection.summary.time_window_compatible_native_record_count = 0U;
  projection.summary.time_window_excluded_native_record_count = 0U;
  projection.summary.time_window_indeterminate_native_record_count = 0U;
  projection.summary.time_window_filter_performed = false;
  projection.time_window_limitations.clear();
  projection.time_window_recommended_actions.clear();

  for (auto& group : projection.matched_groups) {
    group.clock_calibration_fingerprint_matches = false;
    group.time_window_comparison_performed = false;
    group.time_window_compatible_native_record_count = 0U;
    group.time_window_excluded_native_record_count = 0U;
    group.time_window_indeterminate_native_record_count = 0U;
    if (!group.one_product_request) {
      group.time_window_status =
          "ambiguous_multiple_product_request_windows";
      for (auto& record : group.native_records) {
        record.time_window_relation =
            "ambiguous_multiple_product_request_windows";
      }
      projection.summary.ambiguous_time_window_native_record_count =
          inferlab::internal::checked_add_size(
              projection.summary.ambiguous_time_window_native_record_count,
              group.native_records.size(),
              "ambiguous trace time-window native record count");
      continue;
    }

    projection.summary.single_request_time_window_group_count =
        inferlab::internal::checked_add_size(
            projection.summary.single_request_time_window_group_count, 1U,
            "single-request trace time-window group count");
    const auto& request = group.product_requests.front();
    for (auto& record : group.native_records) {
      if (!request.relative_time_window_present) {
        record.time_window_relation =
            "product_request_time_window_not_calibrated";
      } else if (!record.relative_timestamp_present) {
        record.time_window_relation =
            "native_record_timestamp_not_calibrated";
      } else if (request.clock_calibration_fingerprint == "none" ||
                 record.clock_calibration_fingerprint == "none" ||
                 request.clock_calibration_fingerprint !=
                     record.clock_calibration_fingerprint) {
        record.time_window_relation =
            "clock_calibration_fingerprint_mismatch";
      } else {
        group.clock_calibration_fingerprint_matches = true;
        group.time_window_comparison_performed = true;
        projection.summary.time_window_comparison_count =
            inferlab::internal::checked_add_size(
                projection.summary.time_window_comparison_count, 1U,
                "trace time-window comparison count");
        if (windows_overlap(request, record)) {
          record.time_window_relation = "time_window_compatible_candidate";
          group.time_window_compatible_native_record_count =
              inferlab::internal::checked_add_size(
                  group.time_window_compatible_native_record_count, 1U,
                  "group compatible time-window count");
          projection.summary.time_window_compatible_native_record_count =
              inferlab::internal::checked_add_size(
                  projection.summary
                      .time_window_compatible_native_record_count,
                  1U, "compatible trace time-window count");
        } else {
          record.time_window_relation = "time_window_excluded_candidate";
          group.time_window_excluded_native_record_count =
              inferlab::internal::checked_add_size(
                  group.time_window_excluded_native_record_count, 1U,
                  "group excluded time-window count");
          projection.summary.time_window_excluded_native_record_count =
              inferlab::internal::checked_add_size(
                  projection.summary
                      .time_window_excluded_native_record_count,
                  1U, "excluded trace time-window count");
        }
        continue;
      }
      group.time_window_indeterminate_native_record_count =
          inferlab::internal::checked_add_size(
              group.time_window_indeterminate_native_record_count, 1U,
              "group indeterminate time-window count");
      projection.summary.time_window_indeterminate_native_record_count =
          inferlab::internal::checked_add_size(
              projection.summary
                  .time_window_indeterminate_native_record_count,
              1U, "indeterminate trace time-window count");
    }

    if (!group.time_window_comparison_performed) {
      group.time_window_status = "time_window_comparison_pending";
    } else if (group.time_window_indeterminate_native_record_count > 0U) {
      group.time_window_status = "partial_time_window_comparison";
    } else if (group.time_window_excluded_native_record_count ==
               group.native_records.size()) {
      group.time_window_status =
          "all_native_candidates_excluded_by_time_window";
    } else if (group.time_window_compatible_native_record_count ==
               group.native_records.size()) {
      group.time_window_status =
          "all_native_candidates_time_window_compatible";
    } else {
      group.time_window_status = "mixed_time_window_candidate_partition";
    }
  }

  projection.summary.time_window_filter_performed =
      projection.summary.time_window_comparison_count > 0U;
  projection.summary.time_window_decision =
      derive_time_window_decision(projection);
  if (projection.summary.time_window_indeterminate_native_record_count > 0U) {
    append_unique(projection.time_window_limitations,
                  "some_single_request_trace_candidates_lack_comparable_clock_calibration");
    append_unique(projection.time_window_recommended_actions,
                  "refresh_product_and_native_sources_with_one_shared_trace_clock_calibration_file");
  }
  if (projection.summary.ambiguous_time_window_native_record_count > 0U) {
    append_unique(projection.time_window_limitations,
                  "multiple_product_requests_share_one_trace_scope");
    append_unique(projection.time_window_recommended_actions,
                  "add_request_to_span_lineage_before_using_time_windows_for_same_request_selection");
  }
}

void validate_trace_time_window_projection(
    const TraceTokenJoinProjection& projection) {
  std::size_t single_group_count = 0U;
  std::size_t ambiguous_native_count = 0U;
  std::size_t comparison_count = 0U;
  std::size_t compatible_count = 0U;
  std::size_t excluded_count = 0U;
  std::size_t indeterminate_count = 0U;
  for (const auto& group : projection.matched_groups) {
    std::size_t group_compatible = 0U;
    std::size_t group_excluded = 0U;
    std::size_t group_indeterminate = 0U;
    for (const auto& request : group.product_requests) {
      validate_product_time_reference(request);
    }
    for (const auto& record : group.native_records) {
      validate_native_time_reference(record);
    }
    if (!group.one_product_request) {
      ambiguous_native_count = inferlab::internal::checked_add_size(
          ambiguous_native_count, group.native_records.size(),
          "reconciled ambiguous time-window native count");
      if (group.time_window_status !=
              "ambiguous_multiple_product_request_windows" ||
          group.clock_calibration_fingerprint_matches ||
          group.time_window_comparison_performed) {
        throw std::logic_error(
            "ambiguous trace time-window group contract drift");
      }
      for (const auto& record : group.native_records) {
        if (record.time_window_relation !=
            "ambiguous_multiple_product_request_windows") {
          throw std::logic_error(
              "ambiguous trace time-window record contract drift");
        }
      }
      continue;
    }
    single_group_count = inferlab::internal::checked_add_size(
        single_group_count, 1U,
        "reconciled single-request time-window group count");
    const auto& request = group.product_requests.front();
    for (const auto& record : group.native_records) {
      if (record.time_window_relation ==
          "time_window_compatible_candidate") {
        group_compatible = inferlab::internal::checked_add_size(
            group_compatible, 1U,
            "reconciled group compatible time-window count");
      } else if (record.time_window_relation ==
                 "time_window_excluded_candidate") {
        group_excluded = inferlab::internal::checked_add_size(
            group_excluded, 1U,
            "reconciled group excluded time-window count");
      } else if (record.time_window_relation ==
                     "product_request_time_window_not_calibrated" ||
                 record.time_window_relation ==
                     "native_record_timestamp_not_calibrated" ||
                 record.time_window_relation ==
                     "clock_calibration_fingerprint_mismatch") {
        group_indeterminate = inferlab::internal::checked_add_size(
            group_indeterminate, 1U,
            "reconciled group indeterminate time-window count");
      } else {
        throw std::logic_error(
            "single-request trace time-window relation is invalid");
      }
    }
    const std::size_t group_comparison = inferlab::internal::checked_add_size(
        group_compatible, group_excluded,
        "reconciled group time-window comparison count");
    if (group.time_window_compatible_native_record_count !=
            group_compatible ||
        group.time_window_excluded_native_record_count != group_excluded ||
        group.time_window_indeterminate_native_record_count !=
            group_indeterminate ||
        group.time_window_comparison_performed !=
            (group_comparison > 0U) ||
        group.clock_calibration_fingerprint_matches !=
            (group_comparison > 0U) ||
        inferlab::internal::checked_add_size(
            group_comparison, group_indeterminate,
            "single-request time-window native partition") !=
            group.native_records.size()) {
      throw std::logic_error(
          "trace time-window group reconciliation failed");
    }
    const std::string expected_status =
        group_comparison == 0U
            ? "time_window_comparison_pending"
            : group_indeterminate > 0U
                  ? "partial_time_window_comparison"
                  : group_excluded == group.native_records.size()
                        ? "all_native_candidates_excluded_by_time_window"
                        : group_compatible == group.native_records.size()
                              ? "all_native_candidates_time_window_compatible"
                              : "mixed_time_window_candidate_partition";
    if (group.time_window_status != expected_status) {
      throw std::logic_error(
          "trace time-window group status reconciliation failed");
    }
    comparison_count = inferlab::internal::checked_add_size(
        comparison_count, group_comparison,
        "reconciled trace time-window comparison count");
    compatible_count = inferlab::internal::checked_add_size(
        compatible_count, group_compatible,
        "reconciled compatible trace time-window count");
    excluded_count = inferlab::internal::checked_add_size(
        excluded_count, group_excluded,
        "reconciled excluded trace time-window count");
    indeterminate_count = inferlab::internal::checked_add_size(
        indeterminate_count, group_indeterminate,
        "reconciled indeterminate trace time-window count");
  }
  for (const auto& item : projection.unmatched_product_requests) {
    validate_product_time_reference(item.request);
  }
  for (const auto& item : projection.unmatched_native_records) {
    validate_native_time_reference(item.record);
    if (item.record.time_window_relation != "not_evaluated") {
      throw std::logic_error(
          "unmatched native trace record has a time-window relation");
    }
  }

  const auto& summary = projection.summary;
  if (summary.single_request_time_window_group_count != single_group_count ||
      summary.ambiguous_time_window_native_record_count !=
          ambiguous_native_count ||
      summary.time_window_comparison_count != comparison_count ||
      summary.time_window_compatible_native_record_count !=
          compatible_count ||
      summary.time_window_excluded_native_record_count != excluded_count ||
      summary.time_window_indeterminate_native_record_count !=
          indeterminate_count ||
      summary.time_window_filter_performed != (comparison_count > 0U) ||
      summary.time_window_decision != derive_time_window_decision(projection)) {
    throw std::logic_error(
        "trace time-window summary reconciliation failed");
  }

  std::set<std::string> limitations;
  for (const auto& value : projection.time_window_limitations) {
    if (value.empty() || !limitations.insert(value).second) {
      throw std::logic_error(
          "trace time-window limitation is empty or duplicated");
    }
  }
  std::set<std::string> actions;
  for (const auto& value : projection.time_window_recommended_actions) {
    if (value.empty() || !actions.insert(value).second) {
      throw std::logic_error(
          "trace time-window action is empty or duplicated");
    }
  }
}

}  // namespace inferlab_deep_dive_trace
