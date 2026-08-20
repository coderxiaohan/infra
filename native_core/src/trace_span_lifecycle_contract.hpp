#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_trace_lifecycle {

enum class TerminalStatus {
  completed,
  error,
  cancelled,
  rejected,
};

enum class TemporalRelation {
  declared_happens_before,
  temporal_order_indeterminate,
  temporal_order_reversed,
};

struct LifecycleInput {
  std::size_t start_event_count = 0U;
  std::size_t terminal_event_count = 0U;
  bool start_timestamp_present = false;
  bool terminal_timestamp_present = false;
  std::size_t start_timestamp = 0U;
  std::size_t terminal_timestamp = 0U;
  bool observed_timestamp_bounds_present = false;
  std::size_t observed_minimum_timestamp = 0U;
  std::size_t observed_maximum_timestamp = 0U;
  std::string terminal_status = "none";
};

struct LifecycleProjection {
  bool supplied = false;
  bool contract_ready = false;
  bool interval_ordered = false;
  bool observed_events_contained = false;
  std::size_t start_timestamp = 0U;
  std::size_t terminal_timestamp = 0U;
  std::string terminal_status = "none";
  std::string decision = "not_provided";
  std::vector<std::string> issues;
};

const char* terminal_status_name(TerminalStatus status);
bool valid_terminal_status(const std::string& status) noexcept;

const char* temporal_relation_name(TemporalRelation relation);

std::size_t uncertainty_lower_bound(std::size_t value,
                                    std::size_t uncertainty) noexcept;
std::size_t uncertainty_upper_bound(std::size_t value,
                                    std::size_t uncertainty);

LifecycleProjection evaluate_lifecycle(const LifecycleInput& input);
void validate_lifecycle_projection(const LifecycleInput& input,
                                   const LifecycleProjection& projection);

TemporalRelation evaluate_declared_temporal_relation(
    std::size_t predecessor_start, std::size_t predecessor_end,
    std::size_t predecessor_uncertainty, std::size_t successor_start,
    std::size_t successor_end, std::size_t successor_uncertainty);

}  // namespace inferlab_trace_lifecycle
