#pragma once

#include "artifact_metadata_reader.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct CatalogFallbackRemovalSignals {
  std::size_t fallback_removal_queue_item_count = 0;
  std::size_t fallback_removal_queue_blocked_count = 0;
  std::size_t fallback_removal_queue_parity_refresh_required_count = 0;
  std::size_t fallback_removal_queue_current_repo_action_count = 0;
  std::size_t fallback_removal_queue_target_machine_action_count = 0;
  bool fallback_removal_shortest_path_present = false;
  std::string fallback_removal_shortest_path_state = "not_reported";
  std::size_t fallback_removal_shortest_path_step_count = 0;
  std::size_t fallback_removal_shortest_path_current_repo_step_count = 0;
  std::size_t fallback_removal_shortest_path_target_machine_step_count = 0;
  std::size_t fallback_removal_shortest_path_source_location_gap_count = 0;
  std::string fallback_removal_shortest_path_primary_environment =
      "not_applicable";
  std::string fallback_removal_shortest_path_first_surface;
  bool fallback_removal_shortest_path_blocks_removal = false;
  bool fallback_removal_parity_gate_present = false;
  std::string fallback_removal_parity_gate_decision = "not_reported";
  bool fallback_removal_parity_gate_removal_allowed = false;
  std::size_t fallback_removal_parity_gate_blocker_count = 0;
  std::size_t fallback_removal_parity_gate_blocking_surface_count = 0;
  std::size_t fallback_removal_parity_gate_fallback_surface_count = 0;
  std::size_t fallback_removal_parity_gate_parity_refresh_required_count = 0;
  std::size_t fallback_removal_parity_gate_current_repo_action_count = 0;
  std::size_t fallback_removal_parity_gate_target_machine_action_count = 0;
  std::size_t fallback_removal_parity_gate_source_location_gap_count = 0;
  std::size_t
      fallback_removal_parity_gate_blocking_surface_source_location_gap_count =
          0;
  std::string fallback_removal_parity_gate_first_surface;
  bool fallback_removal_parity_gate_blocks_removal = false;
};

struct CatalogFallbackRemovalProjectionOptions {
  bool infer_catalog_section_presence = false;
  bool unknown_primary_environment_when_present = false;
};

inline bool catalog_flow_fallback_removal_queue_active(
    const CatalogFallbackRemovalSignals& signals) {
  return signals.fallback_removal_queue_blocked_count > 0 ||
         signals.fallback_removal_queue_parity_refresh_required_count > 0 ||
         signals.fallback_removal_queue_current_repo_action_count > 0 ||
         signals.fallback_removal_queue_target_machine_action_count > 0;
}

inline std::string catalog_flow_fallback_removal_queue_state(
    const CatalogFallbackRemovalSignals& signals) {
  if (signals.fallback_removal_queue_blocked_count > 0) return "blocked";
  if (signals.fallback_removal_queue_parity_refresh_required_count > 0) {
    return "parity_refresh_required";
  }
  if (signals.fallback_removal_queue_current_repo_action_count > 0) {
    return "current_repo_cleanup_available";
  }
  if (signals.fallback_removal_queue_target_machine_action_count > 0) {
    return "target_machine_refresh_required";
  }
  return signals.fallback_removal_queue_item_count > 0 ? "tracked_clear"
                                                       : "not_reported";
}

inline std::string catalog_flow_fallback_removal_queue_environment(
    const CatalogFallbackRemovalSignals& signals) {
  if (signals.fallback_removal_queue_blocked_count > 0 ||
      signals.fallback_removal_queue_current_repo_action_count > 0) {
    return "current_repo_then_validation_machine";
  }
  if (signals.fallback_removal_queue_parity_refresh_required_count > 0 ||
      signals.fallback_removal_queue_target_machine_action_count > 0) {
    return "validation_or_target_machine";
  }
  return signals.fallback_removal_queue_item_count > 0 ? "review_clear"
                                                       : "not_applicable";
}

inline std::string catalog_flow_fallback_removal_queue_next_action(
    const CatalogFallbackRemovalSignals& signals) {
  if (signals.fallback_removal_queue_blocked_count > 0) {
    return "keep legacy fallback lists retained, close current-repo catalog consumer blockers, then refresh parity evidence before removal";
  }
  if (signals.fallback_removal_queue_parity_refresh_required_count > 0) {
    return "refresh catalog, evidence query, release pipeline, summary, Prometheus, and final report on the validation or target machine before removing fallback lists";
  }
  if (signals.fallback_removal_queue_current_repo_action_count > 0) {
    return "finish repository-side catalog wiring cleanup and regenerate downstream evidence when validation is enabled";
  }
  if (signals.fallback_removal_queue_target_machine_action_count > 0) {
    return "rerun target-machine artifact refresh so fallback-removal parity can be trusted";
  }
  if (signals.fallback_removal_queue_item_count > 0) {
    return "queue is tracked with no active blocker; remove legacy fallback only after refreshed parity evidence is available";
  }
  return "no fallback-removal queue item is reported";
}

inline bool catalog_flow_fallback_removal_shortest_path_active(
    const CatalogFallbackRemovalSignals& signals) {
  return signals.fallback_removal_shortest_path_present &&
         (signals.fallback_removal_shortest_path_blocks_removal ||
          signals.fallback_removal_shortest_path_step_count > 0 ||
          signals.fallback_removal_shortest_path_source_location_gap_count >
              0 ||
          signals.fallback_removal_shortest_path_state ==
              "blocked_until_downstream_parity_refresh" ||
          signals.fallback_removal_shortest_path_state ==
              "fallback_removal_blocked");
}

inline bool catalog_flow_fallback_removal_parity_gate_active(
    const CatalogFallbackRemovalSignals& signals) {
  return signals.fallback_removal_parity_gate_present &&
         !signals.fallback_removal_parity_gate_removal_allowed;
}

inline CatalogFallbackRemovalSignals project_catalog_fallback_removal_signals(
    const std::string& catalog_text,
    const CatalogFallbackRemovalProjectionOptions& options = {}) {
  CatalogFallbackRemovalSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(catalog_text, key);
  };
  const auto flag = [&](const std::string& key) {
    return inferlab_artifact::json_bool_value_or_false(catalog_text, key);
  };
  const auto string_value = [&](const std::string& key) {
    return inferlab_artifact::extract_semantic_string_key_fallback(
        catalog_text, key);
  };

  signals.fallback_removal_queue_item_count =
      count("fallback_removal_queue_item_count");
  signals.fallback_removal_queue_blocked_count =
      count("fallback_removal_queue_blocked_count");
  signals.fallback_removal_queue_parity_refresh_required_count =
      count("fallback_removal_queue_parity_refresh_required_count");
  signals.fallback_removal_queue_current_repo_action_count =
      count("fallback_removal_queue_current_repo_action_count");
  signals.fallback_removal_queue_target_machine_action_count =
      count("fallback_removal_queue_target_machine_action_count");

  signals.fallback_removal_shortest_path_present =
      flag("fallback_removal_shortest_path_present") ||
      (options.infer_catalog_section_presence &&
       inferlab_artifact::contains_key(catalog_text,
                                       "fallback_removal_shortest_path"));
  signals.fallback_removal_shortest_path_state =
      string_value("fallback_removal_shortest_path_state");
  if (signals.fallback_removal_shortest_path_state.empty()) {
    signals.fallback_removal_shortest_path_state =
        signals.fallback_removal_shortest_path_present ? "unknown"
                                                       : "not_reported";
  }
  signals.fallback_removal_shortest_path_step_count =
      count("fallback_removal_shortest_path_step_count");
  signals.fallback_removal_shortest_path_current_repo_step_count =
      count("fallback_removal_shortest_path_current_repo_step_count");
  signals.fallback_removal_shortest_path_target_machine_step_count =
      count("fallback_removal_shortest_path_target_machine_step_count");
  signals.fallback_removal_shortest_path_source_location_gap_count =
      count("fallback_removal_shortest_path_source_location_gap_count");
  signals.fallback_removal_shortest_path_primary_environment =
      string_value("fallback_removal_shortest_path_primary_environment");
  if (signals.fallback_removal_shortest_path_primary_environment.empty()) {
    signals.fallback_removal_shortest_path_primary_environment =
        options.unknown_primary_environment_when_present &&
                signals.fallback_removal_shortest_path_present
            ? "unknown"
            : "not_applicable";
  }
  signals.fallback_removal_shortest_path_first_surface =
      string_value("fallback_removal_shortest_path_first_surface");
  signals.fallback_removal_shortest_path_blocks_removal =
      flag("fallback_removal_shortest_path_blocks_removal");
  if (catalog_flow_fallback_removal_shortest_path_active(signals)) {
    signals.fallback_removal_shortest_path_blocks_removal = true;
  }

  signals.fallback_removal_parity_gate_present =
      flag("fallback_removal_parity_gate_present") ||
      (options.infer_catalog_section_presence &&
       inferlab_artifact::contains_key(catalog_text,
                                       "fallback_removal_parity_gate"));
  signals.fallback_removal_parity_gate_decision =
      string_value("fallback_removal_parity_gate_decision");
  if (signals.fallback_removal_parity_gate_decision.empty()) {
    signals.fallback_removal_parity_gate_decision =
        signals.fallback_removal_parity_gate_present ? "unknown"
                                                     : "not_reported";
  }
  signals.fallback_removal_parity_gate_removal_allowed =
      signals.fallback_removal_parity_gate_present &&
      flag("fallback_removal_parity_gate_removal_allowed");
  signals.fallback_removal_parity_gate_blocker_count =
      count("fallback_removal_parity_gate_blocker_count");
  signals.fallback_removal_parity_gate_blocking_surface_count = std::max(
      count("fallback_removal_parity_gate_blocking_surface_count"),
      signals.fallback_removal_parity_gate_blocker_count);
  signals.fallback_removal_parity_gate_fallback_surface_count =
      count("fallback_removal_parity_gate_fallback_surface_count");
  signals.fallback_removal_parity_gate_parity_refresh_required_count =
      count("fallback_removal_parity_gate_parity_refresh_required_count");
  signals.fallback_removal_parity_gate_current_repo_action_count =
      count("fallback_removal_parity_gate_current_repo_action_count");
  signals.fallback_removal_parity_gate_target_machine_action_count =
      count("fallback_removal_parity_gate_target_machine_action_count");
  signals.fallback_removal_parity_gate_source_location_gap_count =
      count("fallback_removal_parity_gate_source_location_gap_count");
  signals
      .fallback_removal_parity_gate_blocking_surface_source_location_gap_count =
      std::max(
          count("fallback_removal_parity_gate_blocking_surface_source_location_gap_count"),
          signals.fallback_removal_parity_gate_source_location_gap_count);
  signals.fallback_removal_parity_gate_first_surface =
      string_value("fallback_removal_parity_gate_first_surface");
  signals.fallback_removal_parity_gate_blocks_removal =
      flag("fallback_removal_parity_gate_blocks_removal");
  if (catalog_flow_fallback_removal_parity_gate_active(signals)) {
    signals.fallback_removal_parity_gate_blocks_removal = true;
  }
  return signals;
}

}  // namespace inferlab_evidence
