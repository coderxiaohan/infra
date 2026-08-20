#include "product_replay_semantic_events_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "trace_correlation_contract.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace inferlab::product_replay_semantic_events {
namespace {

struct RequestIdentityKey {
  std::size_t source_index = 0U;
  std::string product_profile_id;
  std::string request_id;

  bool operator<(const RequestIdentityKey& other) const noexcept {
    return std::tie(source_index, product_profile_id, request_id) <
           std::tie(other.source_index, other.product_profile_id,
                    other.request_id);
  }
};

struct GroupState {
  RequestTraceRecord record;
  inferlab_trace_lifecycle::LifecycleInput lifecycle_input;
  bool previous_timestamp_present = false;
  std::size_t previous_timestamp_ms = 0U;
  bool timestamp_bounds_initialized = false;
  std::size_t minimum_timestamp_ms = 0U;
  std::size_t maximum_timestamp_ms = 0U;
  bool trace_id_initialized = false;
  std::string trace_id;
  bool request_span_id_initialized = false;
  std::string request_span_id;
  bool terminal_status_initialized = false;
  std::set<std::pair<std::string, std::size_t>> timestamped_event_keys;
};

struct IdentityMembership {
  bool direct_request_identity = false;
  bool trace_context_present = false;
  std::string trace_id;
  bool request_span_context_present = false;
  std::string request_span_id;
};

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

bool schema_valid(const NormalizedEventRow& event) noexcept {
  return event.fixture_schema_valid || event.explicit_replay_valid;
}

void mark_event_type(RequestTraceRecord& record,
                     const std::string& event_type) {
  append_unique(record.event_types, event_type);
  if (event_type == "request_arrival") {
    record.request_arrival_present = true;
  } else if (event_type == "prefill_start") {
    record.prefill_start_present = true;
  } else if (event_type == "prefill_end") {
    record.prefill_end_present = true;
  } else if (event_type == "decode_step") {
    record.decode_step_present = true;
  } else if (event_type == "kv_transfer_start") {
    record.kv_transfer_start_present = true;
  } else if (event_type == "kv_transfer_end") {
    record.kv_transfer_end_present = true;
  }
}

void add_event_membership(GroupState& group,
                          const NormalizedEventRow& event,
                          const SampleInputOrigin origin,
                          const IdentityMembership& membership) {
  auto& record = group.record;
  increment(record.event_membership_count,
            "request trace event membership count");
  if (membership.direct_request_identity) {
    increment(record.direct_request_event_count,
              "request trace direct request event count");
    record.direct_request_identity_present = true;
  } else {
    increment(record.batch_membership_event_count,
              "request trace batch membership event count");
    record.batch_membership_identity_present = true;
  }
  if (membership.trace_context_present) {
    increment(record.trace_context_event_count,
              "request trace trace-context event count");
    if (!group.trace_id_initialized) {
      group.trace_id = membership.trace_id;
      group.trace_id_initialized = true;
    } else if (group.trace_id != membership.trace_id) {
      record.trace_context_consistent = false;
    }
  }
  if (membership.request_span_context_present) {
    increment(record.request_span_context_event_count,
              "request trace request-span event count");
    if (!group.request_span_id_initialized) {
      group.request_span_id = membership.request_span_id;
      group.request_span_id_initialized = true;
    } else if (group.request_span_id != membership.request_span_id) {
      record.request_span_context_consistent = false;
    }
  }
  if (origin == SampleInputOrigin::default_fixture) {
    increment(record.fixture_event_count,
              "request trace fixture event count");
  } else {
    increment(record.explicit_replay_event_count,
              "request trace explicit replay event count");
    if (event.profile_contract_ready) {
      increment(record.replay_ready_event_count,
                "request trace replay-ready event count");
    }
  }
  const std::string& event_type = semantic_event_rule(event).event_type;
  mark_event_type(record, event_type);
  if (event_type == "request_arrival") {
    increment(record.request_arrival_event_count,
              "request trace arrival event count");
    increment(group.lifecycle_input.start_event_count,
              "request lifecycle start event count");
    if (event.timestamp_value_present &&
        !group.lifecycle_input.start_timestamp_present) {
      group.lifecycle_input.start_timestamp_present = true;
      group.lifecycle_input.start_timestamp = event.timestamp_ms;
    }
  } else if (event_type == "request_terminal") {
    record.request_terminal_present = true;
    increment(record.request_terminal_event_count,
              "request trace terminal event count");
    increment(group.lifecycle_input.terminal_event_count,
              "request lifecycle terminal event count");
    if (event.timestamp_value_present &&
        !group.lifecycle_input.terminal_timestamp_present) {
      group.lifecycle_input.terminal_timestamp_present = true;
      group.lifecycle_input.terminal_timestamp = event.timestamp_ms;
    }
    if (!event.request_terminal_status_present ||
        !inferlab_trace_lifecycle::valid_terminal_status(
            event.request_terminal_status)) {
      throw std::logic_error(
          "schema-valid request terminal event has invalid status");
    }
    if (!group.terminal_status_initialized) {
      group.lifecycle_input.terminal_status =
          event.request_terminal_status;
      group.terminal_status_initialized = true;
    } else if (group.lifecycle_input.terminal_status !=
               event.request_terminal_status) {
      group.lifecycle_input.terminal_status = "conflicting";
    }
  }

  if (!event.timestamp_value_present) return;
  increment(record.timestamped_event_count,
            "request trace timestamped event count");
  increment(record.same_record_identity_timestamp_count,
            "request trace same-record identity timestamp count");
  if (group.previous_timestamp_present) {
    increment(record.comparable_order_pair_count,
              "request trace comparable order pair count");
    if (event.timestamp_ms >= group.previous_timestamp_ms) {
      increment(record.ordered_pair_count,
                "request trace ordered pair count");
    } else {
      increment(record.out_of_order_event_count,
                "request trace out-of-order event count");
    }
  }
  group.previous_timestamp_present = true;
  group.previous_timestamp_ms = event.timestamp_ms;
  if (!group.timestamp_bounds_initialized) {
    group.minimum_timestamp_ms = event.timestamp_ms;
    group.maximum_timestamp_ms = event.timestamp_ms;
    group.timestamp_bounds_initialized = true;
  } else {
    group.minimum_timestamp_ms =
        std::min(group.minimum_timestamp_ms, event.timestamp_ms);
    group.maximum_timestamp_ms =
        std::max(group.maximum_timestamp_ms, event.timestamp_ms);
  }

  const auto event_key =
      std::make_pair(semantic_event_rule(event).event_type,
                     event.timestamp_ms);
  if (!group.timestamped_event_keys.insert(event_key).second) {
    increment(record.duplicate_event_count,
              "request trace duplicate event count");
  }
}

void finalize_record(GroupState& group) {
  auto& record = group.record;
  inferlab_trace_lifecycle::LifecycleInput lifecycle_input;
  if (group.lifecycle_input.terminal_event_count > 0U) {
    lifecycle_input = group.lifecycle_input;
    lifecycle_input.observed_timestamp_bounds_present =
        group.timestamp_bounds_initialized;
    lifecycle_input.observed_minimum_timestamp =
        group.minimum_timestamp_ms;
    lifecycle_input.observed_maximum_timestamp =
        group.maximum_timestamp_ms;
  }
  const auto lifecycle =
      inferlab_trace_lifecycle::evaluate_lifecycle(lifecycle_input);
  record.span_lifecycle_supplied = lifecycle.supplied;
  record.span_lifecycle_interval_ordered = lifecycle.interval_ordered;
  record.span_lifecycle_events_contained =
      lifecycle.observed_events_contained;
  record.span_terminal_status = lifecycle.terminal_status;
  record.span_lifecycle_status = lifecycle.decision;
  for (const auto& issue : lifecycle.issues) {
    append_unique(record.issues, issue);
  }
  record.multi_event_join = record.event_membership_count > 1U;
  record.timestamp_complete =
      record.timestamped_event_count == record.event_membership_count;
  record.ordering_valid = record.out_of_order_event_count == 0U;
  record.duplicate_free = record.duplicate_event_count == 0U;
  record.trace_context_present = record.trace_context_event_count > 0U;
  record.trace_context_complete =
      record.trace_context_present &&
      record.trace_context_event_count == record.event_membership_count;
  record.trace_correlation_eligible =
      record.trace_context_complete && record.trace_context_consistent;
  record.request_span_context_present =
      record.request_span_context_event_count > 0U;
  record.request_span_context_complete =
      record.request_span_context_present &&
      record.request_span_context_event_count ==
          record.event_membership_count;
  record.request_span_lineage_eligible =
      record.trace_correlation_eligible &&
      record.request_span_context_complete &&
      record.request_span_context_consistent &&
      record.request_span_binding_unique;
  record.span_lifecycle_contract_ready =
      lifecycle.contract_ready && record.request_span_lineage_eligible;
  if (record.span_lifecycle_supplied && lifecycle.contract_ready &&
      !record.request_span_lineage_eligible) {
    record.span_lifecycle_status =
        "span_lifecycle_request_span_binding_pending";
    append_unique(record.issues,
                  "span_lifecycle_requires_unique_request_span_binding");
  }
  record.replay_join_ready =
      record.explicit_replay_event_count == record.event_membership_count &&
      record.replay_ready_event_count == record.event_membership_count &&
      record.multi_event_join && record.timestamp_complete &&
      record.ordering_valid && record.duplicate_free &&
      record.trace_context_consistent &&
      record.request_span_context_consistent &&
      record.request_span_binding_unique &&
      (!record.span_lifecycle_supplied ||
       record.span_lifecycle_contract_ready);

  if (!record.trace_context_consistent ||
      !record.request_span_context_consistent ||
      !record.request_span_binding_unique ||
      (record.span_lifecycle_supplied &&
       !record.span_lifecycle_contract_ready) ||
      (record.explicit_replay_event_count > 0U &&
       (!record.ordering_valid || !record.duplicate_free))) {
    record.status = "blocked_request_trace_evidence";
    if (!record.trace_context_consistent) {
      append_unique(record.issues,
                    "request_memberships_have_conflicting_trace_ids");
    }
    if (!record.request_span_context_consistent) {
      append_unique(record.issues,
                    "request_memberships_have_conflicting_request_span_ids");
    }
    if (!record.request_span_binding_unique) {
      append_unique(record.issues,
                    "request_span_is_bound_to_multiple_requests");
    }
    if (record.span_lifecycle_supplied &&
        !record.span_lifecycle_contract_ready) {
      append_unique(record.issues,
                    "request_span_lifecycle_contract_is_blocked");
    }
    if (!record.ordering_valid) {
      append_unique(record.issues,
                    "request_event_timestamps_are_out_of_source_order");
    }
    if (!record.duplicate_free) {
      append_unique(record.issues,
                    "duplicate_event_type_and_timestamp_for_request");
    }
  } else if (record.replay_join_ready) {
    record.status = "request_trace_replay_ready";
  } else if (record.explicit_replay_event_count > 0U) {
    record.status = "partial_request_trace_replay";
    if (!record.multi_event_join) {
      append_unique(record.issues,
                    "request_has_only_one_joinable_replay_event");
    }
    if (!record.timestamp_complete) {
      append_unique(record.issues,
                    "request_replay_event_timestamp_is_missing");
    }
    if (record.replay_ready_event_count != record.event_membership_count) {
      append_unique(record.issues,
                    "request_replay_event_profile_contract_is_pending");
    }
  } else if (record.fixture_event_count > 0U) {
    record.status = "fixture_request_trace_schema_ready";
  } else {
    throw std::logic_error(
        "request trace record has no fixture or replay event membership");
  }
}

void apply_clock_calibration(
    GroupState& group,
    inferlab_trace_clock::CalibrationUsageTracker& clock_usage) {
  auto& record = group.record;
  record.clock_calibration_eligible =
      record.trace_correlation_eligible && record.timestamp_complete;
  if (!record.clock_calibration_eligible) return;
  if (!group.trace_id_initialized ||
      !inferlab_trace::valid_w3c_trace_id(group.trace_id) ||
      !group.timestamp_bounds_initialized ||
      group.minimum_timestamp_ms > group.maximum_timestamp_ms) {
    throw std::logic_error(
        "clock-calibration-eligible request has invalid trace or timestamp bounds");
  }

  const std::string source_scope =
      inferlab_trace_clock::product_replay_clock_source_scope(
          record.source_index, record.product_profile_id);
  const auto start = clock_usage.normalize(
      group.trace_id, source_scope,
      inferlab_trace_clock::product_replay_clock_domain(),
      group.minimum_timestamp_ms);
  const auto end = clock_usage.normalize(
      group.trace_id, source_scope,
      inferlab_trace_clock::product_replay_clock_domain(),
      group.maximum_timestamp_ms);
  if (start.calibrated != end.calibrated ||
      start.status != end.status ||
      start.uncertainty_ns != end.uncertainty_ns ||
      (start.calibrated &&
       start.calibration_entry_ordinal != end.calibration_entry_ordinal)) {
    throw std::logic_error(
        "request trace clock calibration bounds disagree");
  }
  record.clock_calibration_status = start.status;
  record.relative_time_window_present = start.calibrated;
  if (record.relative_time_window_present) {
    if (start.trace_relative_timestamp_ns >
        end.trace_relative_timestamp_ns) {
      throw std::logic_error(
          "request trace calibrated time window is reversed");
    }
    record.trace_relative_window_start_ns =
        start.trace_relative_timestamp_ns;
    record.trace_relative_window_end_ns =
        end.trace_relative_timestamp_ns;
    record.clock_uncertainty_ns = start.uncertainty_ns;
  }
}

void aggregate_record(RequestTraceProjection& projection,
                      const RequestTraceRecord& record) {
  auto& summary = projection.summary;
  summary.event_membership_count = inferlab::internal::checked_add_size(
      summary.event_membership_count, record.event_membership_count,
      "request trace event membership total");
  summary.direct_request_event_membership_count =
      inferlab::internal::checked_add_size(
          summary.direct_request_event_membership_count,
          record.direct_request_event_count,
          "request trace direct event membership total");
  summary.batch_request_event_membership_count =
      inferlab::internal::checked_add_size(
          summary.batch_request_event_membership_count,
          record.batch_membership_event_count,
          "request trace batch event membership total");
  summary.comparable_order_pair_count =
      inferlab::internal::checked_add_size(
          summary.comparable_order_pair_count,
          record.comparable_order_pair_count,
          "request trace comparable order pair total");
  summary.ordered_pair_count = inferlab::internal::checked_add_size(
      summary.ordered_pair_count, record.ordered_pair_count,
      "request trace ordered pair total");
  summary.duplicate_event_count = inferlab::internal::checked_add_size(
      summary.duplicate_event_count, record.duplicate_event_count,
      "request trace duplicate event total");
  summary.out_of_order_event_count = inferlab::internal::checked_add_size(
      summary.out_of_order_event_count, record.out_of_order_event_count,
      "request trace out-of-order event total");
  summary.trace_context_event_membership_count =
      inferlab::internal::checked_add_size(
          summary.trace_context_event_membership_count,
          record.trace_context_event_count,
          "request trace trace-context membership total");
  summary.request_span_context_event_membership_count =
      inferlab::internal::checked_add_size(
          summary.request_span_context_event_membership_count,
          record.request_span_context_event_count,
          "request trace request-span membership total");
  if (record.multi_event_join) {
    increment(summary.multi_event_request_count,
              "request trace multi-event request count");
  } else {
    increment(summary.singleton_request_count,
              "request trace singleton request count");
  }
  if (record.fixture_event_count > 0U) {
    increment(summary.fixture_request_count,
              "request trace fixture request count");
  }
  if (record.explicit_replay_event_count > 0U) {
    increment(summary.explicit_replay_request_count,
              "request trace explicit replay request count");
  }
  if (record.replay_join_ready) {
    increment(summary.replay_ready_request_count,
              "request trace replay-ready request count");
  }
  if (record.timestamp_complete) {
    increment(summary.timestamp_complete_request_count,
              "request trace timestamp-complete request count");
  }
  if (record.trace_context_present) {
    increment(summary.trace_context_request_count,
              "request trace trace-context request count");
    if (record.trace_context_consistent) {
      increment(summary.trace_context_consistent_request_count,
                "request trace consistent trace-context request count");
    } else {
      increment(summary.trace_context_conflict_request_count,
                "request trace conflicting trace-context request count");
    }
  }
  if (record.trace_context_complete) {
    increment(summary.trace_context_complete_request_count,
              "request trace complete trace-context request count");
  }
  if (record.trace_correlation_eligible) {
    increment(summary.trace_correlation_eligible_request_count,
              "request trace correlation-eligible request count");
    increment(summary.trace_join_token_count,
              "request trace join-token count");
  }
  if (record.request_span_context_present) {
    increment(summary.request_span_context_request_count,
              "request trace request-span request count");
    if (record.request_span_context_consistent) {
      increment(summary.request_span_context_consistent_request_count,
                "request trace consistent request-span request count");
    } else {
      increment(summary.request_span_context_conflict_request_count,
                "request trace conflicting request-span request count");
    }
  }
  if (record.request_span_context_complete) {
    increment(summary.request_span_context_complete_request_count,
              "request trace complete request-span request count");
  }
  if (!record.request_span_binding_unique) {
    increment(summary.request_span_binding_conflict_request_count,
              "request trace request-span binding conflict count");
  }
  if (record.request_span_lineage_eligible) {
    increment(summary.request_span_lineage_eligible_request_count,
              "request trace lineage-eligible request count");
    increment(summary.request_span_join_token_count,
              "request trace request-span join-token count");
  }
  if (record.span_lifecycle_supplied) {
    increment(summary.span_lifecycle_supplied_request_count,
              "request trace lifecycle-supplied request count");
    if (record.span_lifecycle_contract_ready) {
      increment(summary.span_lifecycle_ready_request_count,
                "request trace lifecycle-ready request count");
      if (record.span_terminal_status == "completed") {
        increment(summary.span_lifecycle_completed_request_count,
                  "request trace completed lifecycle count");
      } else if (record.span_terminal_status == "error") {
        increment(summary.span_lifecycle_error_request_count,
                  "request trace error lifecycle count");
      } else if (record.span_terminal_status == "cancelled") {
        increment(summary.span_lifecycle_cancelled_request_count,
                  "request trace cancelled lifecycle count");
      } else if (record.span_terminal_status == "rejected") {
        increment(summary.span_lifecycle_rejected_request_count,
                  "request trace rejected lifecycle count");
      } else {
        throw std::logic_error(
            "lifecycle-ready request has invalid terminal status");
      }
    } else {
      increment(summary.span_lifecycle_blocked_request_count,
                "request trace lifecycle-blocked request count");
    }
  }
  if (record.clock_calibration_eligible) {
    increment(summary.clock_calibration_eligible_request_count,
              "request trace clock-calibration-eligible request count");
    if (record.relative_time_window_present) {
      increment(summary.clock_calibrated_request_count,
                "request trace clock-calibrated request count");
    } else {
      increment(summary.clock_uncalibrated_request_count,
                "request trace clock-uncalibrated request count");
    }
  }
  if (record.status == "blocked_request_trace_evidence") {
    append_unique(
        projection.blockers,
        "request_trace:source_" + std::to_string(record.source_index) +
            ":request_ordinal_" +
            std::to_string(record.request_ordinal) + ":" + record.status);
  }
}

std::string expected_projection_decision(
    const RequestTraceSummary& summary) {
  if (summary.blocker_count > 0U) {
    return "blocked_request_trace_evidence";
  }
  if (summary.explicit_replay_request_count > 0U &&
      summary.replay_ready_request_count ==
          summary.explicit_replay_request_count) {
    return "request_trace_replay_ready";
  }
  if (summary.explicit_replay_request_count > 0U) {
    return "partial_request_trace_replay";
  }
  if (summary.fixture_request_count > 0U) {
    return "fixture_request_trace_schema_ready";
  }
  return "pending_request_trace_evidence";
}

void validate_projection(
    const RequestTraceProjection& projection,
    const std::vector<std::string>& eligible_trace_ids,
    const std::vector<std::string>& eligible_trace_tokens,
    const std::vector<std::string>& eligible_request_span_tokens) {
  const auto& summary = projection.summary;
  const std::set<std::string> distinct_trace_ids(eligible_trace_ids.begin(),
                                                 eligible_trace_ids.end());
  const std::set<std::string> distinct_trace_tokens(
      eligible_trace_tokens.begin(), eligible_trace_tokens.end());
  const std::set<std::string> distinct_request_span_tokens(
      eligible_request_span_tokens.begin(),
      eligible_request_span_tokens.end());
  if (summary.request_count != projection.records.size() ||
      summary.blocker_count != projection.blockers.size() ||
      inferlab::internal::checked_add_size(
          summary.keyed_event_count, summary.unkeyed_event_count,
          "request trace schema event partition") !=
          summary.schema_valid_event_count ||
      inferlab::internal::checked_add_size(
          summary.multi_event_request_count, summary.singleton_request_count,
          "request trace request cardinality partition") !=
          summary.request_count ||
      inferlab::internal::checked_add_size(
          summary.fixture_request_count,
          summary.explicit_replay_request_count,
          "request trace evidence-origin partition") !=
          summary.request_count ||
      inferlab::internal::checked_add_size(
          summary.direct_request_event_membership_count,
          summary.batch_request_event_membership_count,
          "request trace membership-origin partition") !=
          summary.event_membership_count ||
      summary.replay_ready_request_count >
          summary.explicit_replay_request_count ||
      summary.timestamp_complete_request_count > summary.request_count ||
      summary.ordered_pair_count > summary.comparable_order_pair_count ||
      summary.trace_context_event_membership_count >
          summary.event_membership_count ||
      summary.request_span_context_event_membership_count >
          summary.event_membership_count ||
      ((summary.trace_context_event_membership_count == 0U) !=
       (summary.trace_context_request_count == 0U)) ||
      summary.trace_context_request_count > summary.request_count ||
      inferlab::internal::checked_add_size(
          summary.trace_context_consistent_request_count,
          summary.trace_context_conflict_request_count,
          "request trace trace-context consistency partition") !=
          summary.trace_context_request_count ||
      summary.trace_context_complete_request_count >
          summary.trace_context_request_count ||
      summary.trace_correlation_eligible_request_count >
          summary.trace_context_complete_request_count ||
      summary.trace_correlation_eligible_request_count >
          summary.trace_context_consistent_request_count ||
      summary.trace_context_distinct_id_count != distinct_trace_ids.size() ||
      summary.trace_context_distinct_id_count >
          summary.trace_correlation_eligible_request_count ||
      eligible_trace_ids.size() !=
          summary.trace_correlation_eligible_request_count ||
      eligible_trace_tokens.size() !=
          summary.trace_correlation_eligible_request_count ||
      summary.trace_join_token_count != eligible_trace_tokens.size() ||
      summary.distinct_trace_join_token_count !=
          distinct_trace_tokens.size() ||
      summary.distinct_trace_join_token_count !=
          summary.trace_context_distinct_id_count ||
      summary.trace_join_token_collision_count != 0U ||
      ((summary.request_span_context_event_membership_count == 0U) !=
       (summary.request_span_context_request_count == 0U)) ||
      summary.request_span_context_request_count > summary.request_count ||
      inferlab::internal::checked_add_size(
          summary.request_span_context_consistent_request_count,
          summary.request_span_context_conflict_request_count,
          "request trace request-span consistency partition") !=
          summary.request_span_context_request_count ||
      summary.request_span_context_complete_request_count >
          summary.request_span_context_request_count ||
      summary.request_span_lineage_eligible_request_count >
          summary.request_span_context_complete_request_count ||
      summary.request_span_lineage_eligible_request_count >
          summary.request_span_context_consistent_request_count ||
      summary.request_span_binding_conflict_request_count >
          summary.request_span_context_request_count ||
      summary.request_span_join_token_count !=
          summary.request_span_lineage_eligible_request_count ||
      eligible_request_span_tokens.size() !=
          summary.request_span_join_token_count ||
      summary.distinct_request_span_join_token_count !=
          distinct_request_span_tokens.size() ||
      summary.distinct_request_span_join_token_count !=
          summary.request_span_join_token_count ||
      inferlab::internal::checked_add_size(
          summary.span_lifecycle_ready_request_count,
          summary.span_lifecycle_blocked_request_count,
          "request trace lifecycle readiness partition") !=
          summary.span_lifecycle_supplied_request_count ||
      inferlab::internal::checked_sum_size(
          {summary.span_lifecycle_completed_request_count,
           summary.span_lifecycle_error_request_count,
           summary.span_lifecycle_cancelled_request_count,
           summary.span_lifecycle_rejected_request_count},
          "request trace lifecycle terminal-status partition") !=
          summary.span_lifecycle_ready_request_count ||
      summary.span_lifecycle_supplied_request_count >
          summary.request_count ||
      summary.span_lifecycle_contract !=
          "one_request_arrival_one_request_terminal_all_events_contained" ||
      inferlab::internal::checked_add_size(
          summary.clock_calibrated_request_count,
          summary.clock_uncalibrated_request_count,
          "request trace clock calibration request partition") !=
          summary.clock_calibration_eligible_request_count ||
      summary.clock_calibration_eligible_request_count >
          summary.trace_correlation_eligible_request_count ||
      projection.clock_calibration.lookup_count !=
          inferlab::internal::checked_product_size(
              summary.clock_calibration_eligible_request_count,
              std::size_t{2U},
              "request trace clock calibration lookup total") ||
      projection.clock_calibration.calibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              summary.clock_calibrated_request_count, std::size_t{2U},
              "request trace calibrated timestamp total") ||
      projection.clock_calibration.uncalibrated_timestamp_count !=
          inferlab::internal::checked_product_size(
              summary.clock_uncalibrated_request_count,
              std::size_t{2U},
              "request trace uncalibrated timestamp total") ||
      summary.clock_normalization_performed !=
          (summary.clock_calibrated_request_count > 0U) ||
      summary.normalized_timestamp_unit !=
          "trace_relative_nanoseconds" ||
      (summary.trace_context_conflict_request_count > 0U &&
       summary.blocker_count == 0U) ||
      ((summary.request_span_context_conflict_request_count > 0U ||
        summary.request_span_binding_conflict_request_count > 0U) &&
       summary.blocker_count == 0U) ||
      (summary.span_lifecycle_blocked_request_count > 0U &&
       summary.blocker_count == 0U) ||
      summary.trace_join_token_algorithm !=
          inferlab_trace::trace_context_join_token_algorithm() ||
      summary.request_span_join_token_algorithm !=
          inferlab_trace::trace_context_join_token_algorithm() ||
      summary.request_span_binding_scope !=
          "w3c_trace_id_plus_explicit_request_span_id" ||
      summary.trace_correlation_fingerprint_algorithm !=
          inferlab_trace::trace_context_fingerprint_algorithm() ||
      summary.trace_correlation_set_fingerprint !=
          inferlab_trace::trace_context_set_fingerprint(
              eligible_trace_ids) ||
      !inferlab_trace::valid_trace_context_set_fingerprint(
          summary.trace_correlation_set_fingerprint) ||
      summary.decision != expected_projection_decision(summary)) {
    throw std::logic_error(
        "product replay request trace summary is inconsistent");
  }
  inferlab_trace_clock::validate_clock_calibration_usage(
      projection.clock_calibration);
  RequestTraceSummary rebuilt;
  for (std::size_t index = 0U; index < projection.records.size(); ++index) {
    const auto& record = projection.records[index];
    const std::size_t expected_ordinal =
        inferlab::internal::checked_add_size(
            index, 1U, "request trace reconciled request ordinal");
    const std::string expected_status =
        !record.trace_context_consistent ||
                !record.request_span_context_consistent ||
                !record.request_span_binding_unique ||
                (record.span_lifecycle_supplied &&
                 !record.span_lifecycle_contract_ready) ||
                (record.explicit_replay_event_count > 0U &&
                 (!record.ordering_valid || !record.duplicate_free))
            ? "blocked_request_trace_evidence"
            : record.replay_join_ready
                  ? "request_trace_replay_ready"
                  : record.explicit_replay_event_count > 0U
                        ? "partial_request_trace_replay"
                        : "fixture_request_trace_schema_ready";
    const bool lifecycle_interval_contract_ready =
        record.span_lifecycle_supplied &&
        record.request_arrival_event_count == 1U &&
        record.request_terminal_event_count == 1U &&
        record.span_lifecycle_interval_ordered &&
        record.span_lifecycle_events_contained &&
        inferlab_trace_lifecycle::valid_terminal_status(
            record.span_terminal_status);
    const bool expected_lifecycle_ready =
        lifecycle_interval_contract_ready &&
        record.request_span_lineage_eligible;
    const std::string expected_lifecycle_status =
        !record.span_lifecycle_supplied
            ? "not_provided"
            : expected_lifecycle_ready
                  ? "span_lifecycle_ready"
                  : lifecycle_interval_contract_ready
                        ? "span_lifecycle_request_span_binding_pending"
                        : "span_lifecycle_contract_blocked";
    if (record.request_ordinal != expected_ordinal ||
        record.product_profile_id.empty() ||
        record.event_membership_count == 0U ||
        inferlab::internal::checked_add_size(
            record.direct_request_event_count,
            record.batch_membership_event_count,
            "request trace record membership-origin partition") !=
            record.event_membership_count ||
        inferlab::internal::checked_add_size(
            record.fixture_event_count, record.explicit_replay_event_count,
            "request trace record evidence-origin partition") !=
            record.event_membership_count ||
        record.replay_ready_event_count >
            record.explicit_replay_event_count ||
        record.timestamped_event_count > record.event_membership_count ||
        record.same_record_identity_timestamp_count !=
            record.timestamped_event_count ||
        record.ordered_pair_count > record.comparable_order_pair_count ||
        record.trace_context_event_count >
            record.event_membership_count ||
        record.request_span_context_event_count >
            record.event_membership_count ||
        record.trace_context_present !=
            (record.trace_context_event_count > 0U) ||
        record.trace_context_complete !=
            (record.trace_context_present &&
             record.trace_context_event_count ==
                 record.event_membership_count) ||
        record.trace_correlation_eligible !=
            (record.trace_context_complete &&
             record.trace_context_consistent) ||
        record.trace_join_token_present !=
            record.trace_correlation_eligible ||
        (record.trace_join_token_present
             ? !inferlab_trace::valid_trace_context_join_token(
                   record.trace_join_token)
             : record.trace_join_token != "none") ||
        record.request_span_context_present !=
            (record.request_span_context_event_count > 0U) ||
        record.request_span_context_complete !=
            (record.request_span_context_present &&
             record.request_span_context_event_count ==
                 record.event_membership_count) ||
        (!record.request_span_context_present &&
         !record.request_span_context_consistent) ||
        (!record.request_span_binding_unique &&
         !record.request_span_context_complete) ||
        record.request_span_lineage_eligible !=
            (record.trace_correlation_eligible &&
             record.request_span_context_complete &&
             record.request_span_context_consistent &&
             record.request_span_binding_unique) ||
        record.request_span_join_token_present !=
            record.request_span_lineage_eligible ||
        (record.request_span_join_token_present
             ? !inferlab_trace::valid_span_context_join_token(
                   record.request_span_join_token)
             : record.request_span_join_token != "none") ||
        record.request_arrival_present !=
            (record.request_arrival_event_count > 0U) ||
        record.request_terminal_present !=
            (record.request_terminal_event_count > 0U) ||
        record.span_lifecycle_supplied !=
            (record.request_terminal_event_count > 0U) ||
        record.span_lifecycle_contract_ready !=
            expected_lifecycle_ready ||
        (!record.span_lifecycle_supplied &&
         (record.span_lifecycle_contract_ready ||
          record.span_lifecycle_interval_ordered ||
          record.span_lifecycle_events_contained ||
          record.span_terminal_status != "none" ||
          record.span_lifecycle_status != "not_provided")) ||
        record.span_lifecycle_status != expected_lifecycle_status ||
        record.clock_domain !=
            inferlab_trace_clock::product_replay_clock_domain() ||
        record.clock_calibration_eligible !=
            (record.trace_correlation_eligible &&
             record.timestamp_complete) ||
        (record.relative_time_window_present !=
         (record.clock_calibration_status ==
          "trace_relative_timestamp_ready")) ||
        (!record.clock_calibration_eligible &&
         record.clock_calibration_status != "not_applicable") ||
        (record.clock_calibration_eligible &&
         !record.relative_time_window_present &&
         record.clock_calibration_status !=
             "clock_calibration_not_provided" &&
         record.clock_calibration_status !=
             "clock_calibration_entry_missing") ||
        (!record.relative_time_window_present &&
         (record.trace_relative_window_start_ns != 0U ||
          record.trace_relative_window_end_ns != 0U ||
          record.clock_uncertainty_ns != 0U)) ||
        (record.relative_time_window_present &&
         (record.trace_relative_window_start_ns >
              record.trace_relative_window_end_ns ||
          record.trace_relative_window_end_ns >
              inferlab_trace_clock::maximum_trace_relative_timestamp_ns() ||
          record.clock_uncertainty_ns >
              inferlab_trace_clock::maximum_trace_relative_timestamp_ns())) ||
        record.replay_join_ready !=
            (record.explicit_replay_event_count ==
                 record.event_membership_count &&
             record.replay_ready_event_count ==
                 record.event_membership_count &&
             record.multi_event_join && record.timestamp_complete &&
             record.ordering_valid && record.duplicate_free &&
             record.trace_context_consistent &&
             record.request_span_context_consistent &&
             record.request_span_binding_unique &&
             (!record.span_lifecycle_supplied ||
              record.span_lifecycle_contract_ready)) ||
        record.status != expected_status) {
      throw std::logic_error(
          "product replay request trace record is inconsistent");
    }
    rebuilt.event_membership_count = inferlab::internal::checked_add_size(
        rebuilt.event_membership_count, record.event_membership_count,
        "request trace reconciled event membership total");
    rebuilt.direct_request_event_membership_count =
        inferlab::internal::checked_add_size(
            rebuilt.direct_request_event_membership_count,
            record.direct_request_event_count,
            "request trace reconciled direct membership total");
    rebuilt.batch_request_event_membership_count =
        inferlab::internal::checked_add_size(
            rebuilt.batch_request_event_membership_count,
            record.batch_membership_event_count,
            "request trace reconciled batch membership total");
    rebuilt.comparable_order_pair_count =
        inferlab::internal::checked_add_size(
            rebuilt.comparable_order_pair_count,
            record.comparable_order_pair_count,
            "request trace reconciled comparable pair total");
    rebuilt.ordered_pair_count = inferlab::internal::checked_add_size(
        rebuilt.ordered_pair_count, record.ordered_pair_count,
        "request trace reconciled ordered pair total");
    rebuilt.duplicate_event_count = inferlab::internal::checked_add_size(
        rebuilt.duplicate_event_count, record.duplicate_event_count,
        "request trace reconciled duplicate event total");
    rebuilt.out_of_order_event_count =
        inferlab::internal::checked_add_size(
            rebuilt.out_of_order_event_count,
            record.out_of_order_event_count,
            "request trace reconciled out-of-order event total");
    rebuilt.trace_context_event_membership_count =
        inferlab::internal::checked_add_size(
            rebuilt.trace_context_event_membership_count,
            record.trace_context_event_count,
            "request trace reconciled trace-context membership total");
    rebuilt.request_span_context_event_membership_count =
        inferlab::internal::checked_add_size(
            rebuilt.request_span_context_event_membership_count,
            record.request_span_context_event_count,
            "request trace reconciled request-span membership total");
    if (record.multi_event_join) {
      increment(rebuilt.multi_event_request_count,
                "request trace reconciled multi-event request count");
    } else {
      increment(rebuilt.singleton_request_count,
                "request trace reconciled singleton request count");
    }
    if (record.fixture_event_count > 0U) {
      increment(rebuilt.fixture_request_count,
                "request trace reconciled fixture request count");
    }
    if (record.explicit_replay_event_count > 0U) {
      increment(rebuilt.explicit_replay_request_count,
                "request trace reconciled explicit request count");
    }
    if (record.replay_join_ready) {
      increment(rebuilt.replay_ready_request_count,
                "request trace reconciled ready request count");
    }
    if (record.timestamp_complete) {
      increment(rebuilt.timestamp_complete_request_count,
                "request trace reconciled timestamp-complete count");
    }
    if (record.trace_context_present) {
      increment(rebuilt.trace_context_request_count,
                "request trace reconciled trace-context request count");
      if (record.trace_context_consistent) {
        increment(
            rebuilt.trace_context_consistent_request_count,
            "request trace reconciled consistent trace-context count");
      } else {
        increment(
            rebuilt.trace_context_conflict_request_count,
            "request trace reconciled conflicting trace-context count");
      }
    }
    if (record.trace_context_complete) {
      increment(rebuilt.trace_context_complete_request_count,
                "request trace reconciled complete trace-context count");
    }
    if (record.trace_correlation_eligible) {
      increment(rebuilt.trace_correlation_eligible_request_count,
                "request trace reconciled correlation-eligible count");
      increment(rebuilt.trace_join_token_count,
                "request trace reconciled join-token count");
    }
    if (record.request_span_context_present) {
      increment(rebuilt.request_span_context_request_count,
                "request trace reconciled request-span request count");
      if (record.request_span_context_consistent) {
        increment(
            rebuilt.request_span_context_consistent_request_count,
            "request trace reconciled consistent request-span count");
      } else {
        increment(
            rebuilt.request_span_context_conflict_request_count,
            "request trace reconciled conflicting request-span count");
      }
    }
    if (record.request_span_context_complete) {
      increment(rebuilt.request_span_context_complete_request_count,
                "request trace reconciled complete request-span count");
    }
    if (!record.request_span_binding_unique) {
      increment(rebuilt.request_span_binding_conflict_request_count,
                "request trace reconciled request-span binding conflict count");
    }
    if (record.request_span_lineage_eligible) {
      increment(rebuilt.request_span_lineage_eligible_request_count,
                "request trace reconciled lineage-eligible count");
      increment(rebuilt.request_span_join_token_count,
                "request trace reconciled request-span token count");
    }
    if (record.span_lifecycle_supplied) {
      increment(rebuilt.span_lifecycle_supplied_request_count,
                "request trace reconciled lifecycle-supplied count");
      if (record.span_lifecycle_contract_ready) {
        increment(rebuilt.span_lifecycle_ready_request_count,
                  "request trace reconciled lifecycle-ready count");
        if (record.span_terminal_status == "completed") {
          increment(rebuilt.span_lifecycle_completed_request_count,
                    "request trace reconciled completed lifecycle count");
        } else if (record.span_terminal_status == "error") {
          increment(rebuilt.span_lifecycle_error_request_count,
                    "request trace reconciled error lifecycle count");
        } else if (record.span_terminal_status == "cancelled") {
          increment(rebuilt.span_lifecycle_cancelled_request_count,
                    "request trace reconciled cancelled lifecycle count");
        } else if (record.span_terminal_status == "rejected") {
          increment(rebuilt.span_lifecycle_rejected_request_count,
                    "request trace reconciled rejected lifecycle count");
        }
      } else {
        increment(rebuilt.span_lifecycle_blocked_request_count,
                  "request trace reconciled lifecycle-blocked count");
      }
    }
    if (record.clock_calibration_eligible) {
      increment(rebuilt.clock_calibration_eligible_request_count,
                "request trace reconciled clock-eligible count");
      if (record.relative_time_window_present) {
        increment(rebuilt.clock_calibrated_request_count,
                  "request trace reconciled clock-calibrated count");
      } else {
        increment(rebuilt.clock_uncalibrated_request_count,
                  "request trace reconciled clock-uncalibrated count");
      }
    }
    if (record.status == "blocked_request_trace_evidence") {
      increment(rebuilt.blocker_count,
                "request trace reconciled blocker count");
    }
  }
  if (rebuilt.event_membership_count != summary.event_membership_count ||
      rebuilt.direct_request_event_membership_count !=
          summary.direct_request_event_membership_count ||
      rebuilt.batch_request_event_membership_count !=
          summary.batch_request_event_membership_count ||
      rebuilt.multi_event_request_count !=
          summary.multi_event_request_count ||
      rebuilt.singleton_request_count != summary.singleton_request_count ||
      rebuilt.fixture_request_count != summary.fixture_request_count ||
      rebuilt.explicit_replay_request_count !=
          summary.explicit_replay_request_count ||
      rebuilt.replay_ready_request_count !=
          summary.replay_ready_request_count ||
      rebuilt.timestamp_complete_request_count !=
          summary.timestamp_complete_request_count ||
      rebuilt.comparable_order_pair_count !=
          summary.comparable_order_pair_count ||
      rebuilt.ordered_pair_count != summary.ordered_pair_count ||
      rebuilt.duplicate_event_count != summary.duplicate_event_count ||
      rebuilt.out_of_order_event_count !=
          summary.out_of_order_event_count ||
      rebuilt.trace_context_event_membership_count !=
          summary.trace_context_event_membership_count ||
      rebuilt.request_span_context_event_membership_count !=
          summary.request_span_context_event_membership_count ||
      rebuilt.trace_context_request_count !=
          summary.trace_context_request_count ||
      rebuilt.trace_context_complete_request_count !=
          summary.trace_context_complete_request_count ||
      rebuilt.trace_context_consistent_request_count !=
          summary.trace_context_consistent_request_count ||
      rebuilt.trace_context_conflict_request_count !=
          summary.trace_context_conflict_request_count ||
      rebuilt.trace_correlation_eligible_request_count !=
          summary.trace_correlation_eligible_request_count ||
      rebuilt.trace_join_token_count != summary.trace_join_token_count ||
      rebuilt.request_span_context_request_count !=
          summary.request_span_context_request_count ||
      rebuilt.request_span_context_complete_request_count !=
          summary.request_span_context_complete_request_count ||
      rebuilt.request_span_context_consistent_request_count !=
          summary.request_span_context_consistent_request_count ||
      rebuilt.request_span_context_conflict_request_count !=
          summary.request_span_context_conflict_request_count ||
      rebuilt.request_span_binding_conflict_request_count !=
          summary.request_span_binding_conflict_request_count ||
      rebuilt.request_span_lineage_eligible_request_count !=
          summary.request_span_lineage_eligible_request_count ||
      rebuilt.request_span_join_token_count !=
          summary.request_span_join_token_count ||
      rebuilt.span_lifecycle_supplied_request_count !=
          summary.span_lifecycle_supplied_request_count ||
      rebuilt.span_lifecycle_ready_request_count !=
          summary.span_lifecycle_ready_request_count ||
      rebuilt.span_lifecycle_blocked_request_count !=
          summary.span_lifecycle_blocked_request_count ||
      rebuilt.span_lifecycle_completed_request_count !=
          summary.span_lifecycle_completed_request_count ||
      rebuilt.span_lifecycle_error_request_count !=
          summary.span_lifecycle_error_request_count ||
      rebuilt.span_lifecycle_cancelled_request_count !=
          summary.span_lifecycle_cancelled_request_count ||
      rebuilt.span_lifecycle_rejected_request_count !=
          summary.span_lifecycle_rejected_request_count ||
      rebuilt.clock_calibration_eligible_request_count !=
          summary.clock_calibration_eligible_request_count ||
      rebuilt.clock_calibrated_request_count !=
          summary.clock_calibrated_request_count ||
      rebuilt.clock_uncalibrated_request_count !=
          summary.clock_uncalibrated_request_count ||
      rebuilt.blocker_count != summary.blocker_count) {
    throw std::logic_error(
        "product replay request trace reverse reconciliation failed");
  }
}

}  // namespace

RequestTraceProjection build_request_trace_projection(
    const std::vector<NormalizedEventRow>& events,
    const std::vector<SampleSourceState>& sources) {
  return build_request_trace_projection(
      events, sources, inferlab_trace_clock::CalibrationSource{});
}

RequestTraceProjection build_request_trace_projection(
    const std::vector<NormalizedEventRow>& events,
    const std::vector<SampleSourceState>& sources,
    inferlab_trace_clock::CalibrationSource clock_calibration) {
  RequestTraceProjection projection;
  projection.claim_boundary =
      "request, W3C trace and explicit request-span identities, absolute timestamps and affine calibration anchors are retained only in invocation-local memory; output contains anonymous request ordinals, per-request non-cryptographic trace and request-span join tokens, aggregate replay ordering facts, an optional explicit request-arrival-to-terminal lifecycle with canonical terminal status, optional trace-relative request windows with uncertainty, and non-cryptographic set/calibration fingerprints for controlled downstream equality, lineage and temporal checks; this producer proves only its supplied request lifecycle contract and does not itself prove cross-artifact native same-request lineage, inferred causality, error or cancellation propagation, benchmark latency, execute behavior, or release approval";

  inferlab_trace_clock::CalibrationUsageTracker clock_usage(
      clock_calibration);

  std::map<RequestIdentityKey, std::size_t> group_indices;
  std::vector<GroupState> groups;
  for (const auto& event : events) {
    if (!schema_valid(event)) continue;
    increment(projection.summary.schema_valid_event_count,
              "request trace schema-valid event count");
    if (event.source_index >= sources.size() ||
        event.product_profile_id.empty() || event.rule == nullptr) {
      throw std::logic_error(
          "schema-valid request trace event has invalid source identity");
    }

    std::map<std::string, IdentityMembership> request_identities;
    if (event.request_id_present) {
      auto& membership = request_identities[event.request_id];
      membership.direct_request_identity = true;
      if (event.trace_id_present) {
        membership.trace_context_present = true;
        membership.trace_id = event.trace_id;
      }
      if (event.request_span_id_present) {
        membership.request_span_context_present = true;
        membership.request_span_id = event.request_span_id;
      }
    }
    if (event.request_ids_present) {
      for (std::size_t index = 0U; index < event.request_ids.size();
           ++index) {
        auto& membership = request_identities[event.request_ids[index]];
        if (event.trace_ids_present) {
          if (index >= event.trace_ids.size()) {
            throw std::logic_error(
                "schema-valid trace-id array is not aligned with request ids");
          }
          if (membership.trace_context_present &&
              membership.trace_id != event.trace_ids[index]) {
            throw std::logic_error(
                "one event assigns conflicting trace ids to one request");
          }
          membership.trace_context_present = true;
          membership.trace_id = event.trace_ids[index];
        }
        if (event.request_span_ids_present) {
          if (index >= event.request_span_ids.size()) {
            throw std::logic_error(
                "schema-valid request-span array is not aligned with request ids");
          }
          if (membership.request_span_context_present &&
              membership.request_span_id !=
                  event.request_span_ids[index]) {
            throw std::logic_error(
                "one event assigns conflicting request spans to one request");
          }
          membership.request_span_context_present = true;
          membership.request_span_id = event.request_span_ids[index];
        }
      }
    }
    if (request_identities.empty()) {
      increment(projection.summary.unkeyed_event_count,
                "request trace unkeyed event count");
      continue;
    }
    increment(projection.summary.keyed_event_count,
              "request trace keyed event count");

    const auto origin = sources[event.source_index].request.origin;
    for (const auto& identity : request_identities) {
      RequestIdentityKey key{event.source_index,
                             event.product_profile_id, identity.first};
      auto found = group_indices.find(key);
      if (found == group_indices.end()) {
        GroupState group;
        group.record.source_index = event.source_index;
        group.record.request_ordinal = inferlab::internal::checked_add_size(
            groups.size(), 1U, "request trace request ordinal");
        group.record.product_profile_id = event.product_profile_id;
        const std::size_t group_index = groups.size();
        groups.push_back(std::move(group));
        found = group_indices.emplace(std::move(key), group_index).first;
      }
      add_event_membership(groups.at(found->second), event, origin,
                           identity.second);
    }
  }

  std::vector<std::string> eligible_trace_ids;
  std::vector<std::string> eligible_trace_tokens;
  std::vector<std::string> eligible_request_span_tokens;
  std::map<std::string, std::string> trace_id_by_join_token;
  std::map<std::string, std::pair<std::string, std::string>>
      span_context_by_join_token;
  std::map<std::string, std::vector<std::size_t>>
      request_groups_by_span_token;
  for (std::size_t group_index = 0U; group_index < groups.size();
       ++group_index) {
    auto& group = groups[group_index];
    finalize_record(group);
    if (group.record.trace_correlation_eligible) {
      if (!group.trace_id_initialized ||
          !inferlab_trace::valid_w3c_trace_id(group.trace_id)) {
        throw std::logic_error(
            "trace-correlation-eligible request has no valid trace id");
      }
      group.record.trace_join_token =
          inferlab_trace::trace_context_join_token(group.trace_id);
      group.record.trace_join_token_present = true;
      if (!inferlab_trace::valid_trace_context_join_token(
              group.record.trace_join_token)) {
        throw std::logic_error(
            "trace-correlation-eligible request produced an invalid join token");
      }
      const auto inserted = trace_id_by_join_token.emplace(
          group.record.trace_join_token, group.trace_id);
      if (!inserted.second && inserted.first->second != group.trace_id) {
        throw std::logic_error(
            "distinct trace ids produced one non-cryptographic join token");
      }
      eligible_trace_ids.push_back(group.trace_id);
      eligible_trace_tokens.push_back(group.record.trace_join_token);
    }
    if (group.record.request_span_lineage_eligible) {
      if (!group.trace_id_initialized ||
          !group.request_span_id_initialized ||
          !inferlab_trace::valid_w3c_trace_id(group.trace_id) ||
          !inferlab_trace::valid_w3c_span_id(group.request_span_id)) {
        throw std::logic_error(
            "request-span-lineage-eligible request has invalid context");
      }
      group.record.request_span_join_token =
          inferlab_trace::span_context_join_token(
              group.trace_id, group.request_span_id);
      group.record.request_span_join_token_present = true;
      const auto context =
          std::make_pair(group.trace_id, group.request_span_id);
      const auto inserted = span_context_by_join_token.emplace(
          group.record.request_span_join_token, context);
      if (!inserted.second && inserted.first->second != context) {
        throw std::logic_error(
            "distinct request span contexts produced one non-cryptographic token");
      }
      request_groups_by_span_token[group.record.request_span_join_token]
          .push_back(group_index);
    }
  }
  for (const auto& entry : request_groups_by_span_token) {
    if (entry.second.size() < 2U) continue;
    for (const std::size_t group_index : entry.second) {
      auto& group = groups.at(group_index);
      auto& record = group.record;
      record.request_span_binding_unique = false;
      record.request_span_lineage_eligible = false;
      record.request_span_join_token_present = false;
      record.request_span_join_token = "none";
      finalize_record(group);
    }
  }

  projection.records.reserve(groups.size());
  for (auto& group : groups) {
    if (group.record.request_span_lineage_eligible) {
      eligible_request_span_tokens.push_back(
          group.record.request_span_join_token);
    }
    apply_clock_calibration(group, clock_usage);
    aggregate_record(projection, group.record);
    projection.records.push_back(std::move(group.record));
  }
  projection.summary.request_count = projection.records.size();
  projection.summary.trace_context_distinct_id_count =
      std::set<std::string>(eligible_trace_ids.begin(),
                            eligible_trace_ids.end())
          .size();
  projection.summary.trace_join_token_count = eligible_trace_tokens.size();
  projection.summary.distinct_trace_join_token_count =
      std::set<std::string>(eligible_trace_tokens.begin(),
                            eligible_trace_tokens.end())
          .size();
  projection.summary.distinct_request_span_join_token_count =
      std::set<std::string>(eligible_request_span_tokens.begin(),
                            eligible_request_span_tokens.end())
          .size();
  projection.summary.trace_join_token_algorithm =
      inferlab_trace::trace_context_join_token_algorithm();
  projection.summary.request_span_join_token_algorithm =
      inferlab_trace::trace_context_join_token_algorithm();
  projection.summary.trace_correlation_fingerprint_algorithm =
      inferlab_trace::trace_context_fingerprint_algorithm();
  projection.summary.trace_correlation_set_fingerprint =
      inferlab_trace::trace_context_set_fingerprint(eligible_trace_ids);
  projection.clock_calibration = clock_usage.finalize();
  projection.summary.clock_normalization_performed =
      projection.summary.clock_calibrated_request_count > 0U;
  projection.summary.blocker_count = projection.blockers.size();
  projection.summary.decision =
      expected_projection_decision(projection.summary);
  validate_projection(projection, eligible_trace_ids,
                      eligible_trace_tokens,
                      eligible_request_span_tokens);
  return projection;
}

}  // namespace inferlab::product_replay_semantic_events
