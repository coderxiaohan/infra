#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct ConsumerFieldCoverageSignals {
  std::size_t consumer_field_coverage_surface_count = 0;
  std::size_t consumer_field_coverage_catalog_managed_surface_count = 0;
  std::size_t consumer_field_coverage_fallback_surface_count = 0;
  std::size_t consumer_field_coverage_required_field_count = 0;
  std::size_t consumer_field_coverage_covered_field_count = 0;
  std::size_t consumer_field_coverage_field_gap_count = 0;
  std::size_t consumer_field_coverage_parity_refresh_required_count = 0;
  std::size_t consumer_field_coverage_current_repo_action_count = 0;
  std::size_t consumer_field_coverage_target_machine_action_count = 0;
  std::size_t consumer_field_coverage_repair_card_count = 0;
  std::size_t consumer_field_coverage_current_repo_repair_card_count = 0;
  std::size_t consumer_field_coverage_target_machine_repair_card_count = 0;
};

inline ConsumerFieldCoverageSignals project_consumer_field_coverage_signals(
    const std::string& catalog_text) {
  ConsumerFieldCoverageSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(catalog_text, key);
  };
  signals.consumer_field_coverage_surface_count =
      count("consumer_field_coverage_surface_count");
  signals.consumer_field_coverage_catalog_managed_surface_count =
      count("consumer_field_coverage_catalog_managed_surface_count");
  signals.consumer_field_coverage_fallback_surface_count =
      count("consumer_field_coverage_fallback_surface_count");
  signals.consumer_field_coverage_required_field_count =
      count("consumer_field_coverage_required_field_count");
  signals.consumer_field_coverage_covered_field_count =
      count("consumer_field_coverage_covered_field_count");
  signals.consumer_field_coverage_field_gap_count =
      count("consumer_field_coverage_field_gap_count");
  signals.consumer_field_coverage_parity_refresh_required_count =
      count("consumer_field_coverage_parity_refresh_required_count");
  signals.consumer_field_coverage_current_repo_action_count =
      count("consumer_field_coverage_current_repo_action_count");
  signals.consumer_field_coverage_target_machine_action_count =
      count("consumer_field_coverage_target_machine_action_count");
  signals.consumer_field_coverage_current_repo_repair_card_count =
      signals.consumer_field_coverage_field_gap_count > 0 ||
              signals.consumer_field_coverage_current_repo_action_count > 0
          ? 1U
          : 0U;
  signals.consumer_field_coverage_target_machine_repair_card_count =
      signals.consumer_field_coverage_fallback_surface_count > 0 ||
              signals.consumer_field_coverage_parity_refresh_required_count >
                  0 ||
              signals.consumer_field_coverage_target_machine_action_count > 0
          ? 1U
          : 0U;
  signals.consumer_field_coverage_repair_card_count =
      inferlab::internal::checked_add_size(
          signals.consumer_field_coverage_current_repo_repair_card_count,
          signals.consumer_field_coverage_target_machine_repair_card_count,
          "consumer field coverage repair card count");
  return signals;
}

}  // namespace inferlab_evidence
