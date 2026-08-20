#pragma once

#include "checked_index.hpp"
#include "evidence_catalog_flow_projection.hpp"
#include "native_core_audit_projection.hpp"
#include "product_replay_semantic_linkage_projection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

namespace inferlab_evidence_query {

struct CatalogFlowFilterSignals
    : inferlab_evidence::CatalogFlowSignals {
  std::string filter_decision;
};

inline std::string catalog_flow_fallback_removal_shortest_path_next_action(
    const CatalogFlowFilterSignals& signals) {
  if (!signals.fallback_removal_shortest_path_present) {
    return "refresh native_artifact_catalog_single_source so fallback-removal shortest-path metadata is available";
  }
  if (signals.fallback_removal_shortest_path_source_location_gap_count > 0) {
    return "add exact fallback source-location metadata before relying on shortest-path cleanup guidance";
  }
  if (signals.fallback_removal_shortest_path_current_repo_step_count > 0) {
    return "follow the first current-repo catalog fallback cleanup step, then wait for downstream parity refresh before deleting fallback lists";
  }
  if (signals.fallback_removal_shortest_path_target_machine_step_count > 0) {
    return "refresh downstream catalog consumers on the validation or target machine and record parity before fallback removal";
  }
  if (signals.fallback_removal_shortest_path_state ==
      "no_catalog_fallback_retained") {
    return "no catalog fallback shortest-path cleanup remains";
  }
  return "inspect catalog fallback-removal shortest-path state before cleanup";
}

inline std::string catalog_flow_fallback_removal_parity_gate_next_action(
    const CatalogFlowFilterSignals& signals) {
  if (!signals.fallback_removal_parity_gate_present) {
    return "refresh native_artifact_catalog_single_source so fallback-removal parity-gate metadata is available";
  }
  if (signals.fallback_removal_shortest_path_state ==
      "no_catalog_fallback_retained") {
    return "no catalog fallback is retained; normal consumer refresh does not gate source deletion";
  }
  if (signals.fallback_removal_parity_gate_removal_allowed) {
    return "fallback removal is allowed only after human review confirms refreshed downstream parity evidence";
  }
  if (signals.fallback_removal_parity_gate_source_location_gap_count > 0) {
    return "add exact fallback source-location metadata before using the parity gate to delete fallback lists";
  }
  if (signals.fallback_removal_parity_gate_current_repo_action_count > 0) {
    return "keep fallback lists retained, close current-repo catalog cleanup actions, and then refresh parity evidence";
  }
  if (signals.fallback_removal_parity_gate_parity_refresh_required_count > 0 ||
      signals.fallback_removal_parity_gate_target_machine_action_count > 0) {
    return "refresh downstream catalog consumers on a validation or target machine and record parity before fallback deletion";
  }
  if (signals.fallback_removal_parity_gate_blocker_count > 0) {
    return "inspect fallback-removal parity-gate blockers before cleanup";
  }
  if (signals.fallback_removal_parity_gate_blocks_removal) {
    return "inspect the explicit fallback-removal parity-gate block before cleanup";
  }
  return "fallback-removal parity gate is reported with no active blocker";
}

struct NativeCoreAuditFilterSignals
    : inferlab_evidence::NativeCoreAuditProjectionSignals {
  std::string filter_decision;
};

using ProductReplaySemanticLinkageSignals =
    inferlab_evidence::ProductReplaySemanticLinkageSignals;

inline std::size_t native_core_audit_blocker_count(
    const NativeCoreAuditFilterSignals& signals) {
  if (signals.filter_decision == "native_core_audit_ready") return 0U;
  return std::max<std::size_t>(
      1U, inferlab_evidence::native_core_audit_core_blocker_count(signals));
}

}  // namespace inferlab_evidence_query
