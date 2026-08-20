#include "trace_span_lifecycle_contract.hpp"

#include "plan_json_utils.hpp"
#include "trace_clock_calibration.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace inferlab_trace_lifecycle {
namespace {

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

bool lifecycle_supplied(const LifecycleInput& input) noexcept {
  return input.start_event_count > 0U || input.terminal_event_count > 0U ||
         input.start_timestamp_present || input.terminal_timestamp_present ||
         input.observed_timestamp_bounds_present ||
         input.terminal_status != "none";
}

std::string expected_decision(const LifecycleProjection& projection) {
  if (!projection.supplied) return "not_provided";
  return projection.contract_ready ? "span_lifecycle_ready"
                                   : "span_lifecycle_contract_blocked";
}

}  // namespace

const char* terminal_status_name(const TerminalStatus status) {
  switch (status) {
    case TerminalStatus::completed:
      return "completed";
    case TerminalStatus::error:
      return "error";
    case TerminalStatus::cancelled:
      return "cancelled";
    case TerminalStatus::rejected:
      return "rejected";
  }
  throw std::invalid_argument("unknown trace span terminal status");
}

bool valid_terminal_status(const std::string& status) noexcept {
  return status == terminal_status_name(TerminalStatus::completed) ||
         status == terminal_status_name(TerminalStatus::error) ||
         status == terminal_status_name(TerminalStatus::cancelled) ||
         status == terminal_status_name(TerminalStatus::rejected);
}

const char* temporal_relation_name(const TemporalRelation relation) {
  switch (relation) {
    case TemporalRelation::declared_happens_before:
      return "declared_happens_before";
    case TemporalRelation::temporal_order_indeterminate:
      return "temporal_order_indeterminate";
    case TemporalRelation::temporal_order_reversed:
      return "temporal_order_reversed";
  }
  throw std::invalid_argument("unknown declared temporal relation");
}

std::size_t uncertainty_lower_bound(const std::size_t value,
                                    const std::size_t uncertainty) noexcept {
  return value > uncertainty ? value - uncertainty : 0U;
}

std::size_t uncertainty_upper_bound(const std::size_t value,
                                    const std::size_t uncertainty) {
  const std::size_t maximum =
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns();
  if (value > maximum || uncertainty > maximum) {
    throw std::invalid_argument(
        "trace lifecycle value exceeds the relative timestamp contract");
  }
  return value > maximum - uncertainty ? maximum : value + uncertainty;
}

LifecycleProjection evaluate_lifecycle(const LifecycleInput& input) {
  LifecycleProjection projection;
  projection.supplied = lifecycle_supplied(input);
  if (!projection.supplied) return projection;

  if (input.start_event_count != 1U) {
    append_unique(projection.issues,
                  "span_lifecycle_requires_exactly_one_start");
  }
  if (input.terminal_event_count != 1U) {
    append_unique(projection.issues,
                  "span_lifecycle_requires_exactly_one_terminal");
  }
  if (!input.start_timestamp_present) {
    append_unique(projection.issues,
                  "span_lifecycle_start_timestamp_missing");
  }
  if (!input.terminal_timestamp_present) {
    append_unique(projection.issues,
                  "span_lifecycle_terminal_timestamp_missing");
  }
  if (!valid_terminal_status(input.terminal_status)) {
    append_unique(projection.issues,
                  "span_lifecycle_terminal_status_invalid");
  }
  if (!input.observed_timestamp_bounds_present) {
    append_unique(projection.issues,
                  "span_lifecycle_observed_timestamp_bounds_missing");
  }

  projection.start_timestamp = input.start_timestamp;
  projection.terminal_timestamp = input.terminal_timestamp;
  projection.terminal_status = input.terminal_status;
  projection.interval_ordered =
      input.start_timestamp_present && input.terminal_timestamp_present &&
      input.start_timestamp <= input.terminal_timestamp;
  if (!projection.interval_ordered) {
    append_unique(projection.issues, "span_lifecycle_interval_reversed");
  }

  const bool observed_bounds_ordered =
      input.observed_timestamp_bounds_present &&
      input.observed_minimum_timestamp <= input.observed_maximum_timestamp;
  if (input.observed_timestamp_bounds_present && !observed_bounds_ordered) {
    append_unique(projection.issues,
                  "span_lifecycle_observed_bounds_reversed");
  }
  projection.observed_events_contained =
      projection.interval_ordered && observed_bounds_ordered &&
      input.start_timestamp <= input.observed_minimum_timestamp &&
      input.observed_maximum_timestamp <= input.terminal_timestamp;
  if (observed_bounds_ordered &&
      !projection.observed_events_contained) {
    append_unique(projection.issues,
                  "span_lifecycle_observed_event_outside_interval");
  }

  projection.contract_ready = projection.issues.empty();
  projection.decision = expected_decision(projection);
  validate_lifecycle_projection(input, projection);
  return projection;
}

void validate_lifecycle_projection(
    const LifecycleInput& input,
    const LifecycleProjection& projection) {
  const bool supplied = lifecycle_supplied(input);
  const bool ordered =
      input.start_timestamp_present && input.terminal_timestamp_present &&
      input.start_timestamp <= input.terminal_timestamp;
  const bool bounds_ordered =
      input.observed_timestamp_bounds_present &&
      input.observed_minimum_timestamp <= input.observed_maximum_timestamp;
  const bool contained =
      ordered && bounds_ordered &&
      input.start_timestamp <= input.observed_minimum_timestamp &&
      input.observed_maximum_timestamp <= input.terminal_timestamp;
  const bool contract_ready = supplied && projection.issues.empty();
  if (projection.supplied != supplied ||
      projection.interval_ordered != ordered ||
      projection.observed_events_contained != contained ||
      projection.contract_ready != contract_ready ||
      projection.start_timestamp != input.start_timestamp ||
      projection.terminal_timestamp != input.terminal_timestamp ||
      projection.terminal_status != input.terminal_status ||
      projection.decision != expected_decision(projection) ||
      (!projection.supplied &&
       (projection.contract_ready || projection.interval_ordered ||
        projection.observed_events_contained ||
        projection.start_timestamp != 0U ||
        projection.terminal_timestamp != 0U ||
        projection.terminal_status != "none" ||
        !projection.issues.empty()))) {
    throw std::logic_error(
        "trace span lifecycle projection is inconsistent");
  }
}

TemporalRelation evaluate_declared_temporal_relation(
    const std::size_t predecessor_start,
    const std::size_t predecessor_end,
    const std::size_t predecessor_uncertainty,
    const std::size_t successor_start,
    const std::size_t successor_end,
    const std::size_t successor_uncertainty) {
  const std::size_t maximum =
      inferlab_trace_clock::maximum_trace_relative_timestamp_ns();
  if (predecessor_start > predecessor_end ||
      successor_start > successor_end || predecessor_end > maximum ||
      successor_end > maximum || predecessor_uncertainty > maximum ||
      successor_uncertainty > maximum) {
    throw std::invalid_argument(
        "declared temporal relation interval contract is invalid");
  }

  const std::size_t predecessor_lower = uncertainty_lower_bound(
      predecessor_start, predecessor_uncertainty);
  const std::size_t predecessor_upper = uncertainty_upper_bound(
      predecessor_end, predecessor_uncertainty);
  const std::size_t successor_lower = uncertainty_lower_bound(
      successor_start, successor_uncertainty);
  const std::size_t successor_upper = uncertainty_upper_bound(
      successor_end, successor_uncertainty);
  if (predecessor_upper <= successor_lower) {
    return TemporalRelation::declared_happens_before;
  }
  if (successor_upper <= predecessor_lower) {
    return TemporalRelation::temporal_order_reversed;
  }
  return TemporalRelation::temporal_order_indeterminate;
}

}  // namespace inferlab_trace_lifecycle
