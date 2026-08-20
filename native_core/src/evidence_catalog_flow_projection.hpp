#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_catalog_flow_metric_projection.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_evidence {

struct CatalogFlowSignals : CatalogFlowMetricSignals {
  bool catalog_artifact_present = false;
  bool catalog_artifact_readable = false;
  bool catalog_artifact_usable = false;
  std::string catalog_artifact_read_status = "missing";
};

struct CatalogFlowProducerSignals : CatalogFlowMetricSignals {
  bool catalog_artifact_present = false;
  bool catalog_artifact_readable = false;
  std::string catalog_artifact_read_status = "missing";
  bool catalog_review_source_usable = false;
  std::string catalog_drift_decision;
  std::string single_source_parity_decision;
  std::string review_decision;
};

struct CatalogFlowProjectionOptions {
  CatalogFallbackRemovalProjectionOptions fallback_removal;
  bool promote_read_issue_to_input_gap = false;
};

namespace detail {

inline bool required_nonempty_string(const std::string& object,
                                     const std::string& key,
                                     std::string& value) {
  value.clear();
  return inferlab_artifact::contains_top_level_key_fallback(object, key) &&
         inferlab_artifact::extract_semantic_string_key_fallback(
             object, key, value) &&
         !value.empty();
}

inline bool extract_owned_or_flattened_decision(
    const std::string& document, const std::string& flattened_source,
    const std::string& owning_key, const std::string& decision_key,
    std::string& value) {
  value.clear();
  if (inferlab_artifact::contains_top_level_key_fallback(document,
                                                         owning_key)) {
    const std::string section =
        inferlab_artifact::json_object_section_or_empty(document, owning_key);
    return !section.empty() &&
           required_nonempty_string(section, decision_key, value);
  }
  return required_nonempty_string(flattened_source, decision_key, value);
}

}  // namespace detail

inline CatalogFlowSignals project_catalog_flow_signals(
    const inferlab_artifact::TextReadResult& catalog_read,
    const CatalogFlowProjectionOptions& options = {}) {
  CatalogFlowSignals signals;
  const auto catalog_source =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          catalog_read.readable ? catalog_read.text : "", "summary");
  const bool catalog_usable =
      catalog_read.present && catalog_read.readable &&
      catalog_source.usable;
  const std::string catalog_document =
      catalog_usable ? catalog_read.text : "";
  const std::string& catalog_text =
      catalog_usable ? catalog_source.text : catalog_document;

  static_cast<CatalogFlowMetricSignals&>(signals) =
      project_catalog_flow_metrics(catalog_text, options.fallback_removal);

  signals.catalog_artifact_present = catalog_read.present;
  signals.catalog_artifact_readable = catalog_read.readable;
  signals.catalog_artifact_usable = catalog_usable;
  signals.catalog_artifact_read_status = catalog_read.status;
  signals.read_issue_count =
      catalog_read.present && !catalog_usable ? 1U : 0U;
  if (options.fallback_removal.infer_catalog_section_presence) {
    if (!signals.fallback_removal_shortest_path_present &&
        !inferlab_artifact::json_object_section_or_empty(
             catalog_document, "fallback_removal_shortest_path")
             .empty()) {
      signals.fallback_removal_shortest_path_present = true;
      if (signals.fallback_removal_shortest_path_state == "not_reported") {
        signals.fallback_removal_shortest_path_state = "unknown";
      }
      if (options.fallback_removal
              .unknown_primary_environment_when_present &&
          signals.fallback_removal_shortest_path_primary_environment ==
              "not_applicable") {
        signals.fallback_removal_shortest_path_primary_environment =
            "unknown";
      }
    }
    if (!signals.fallback_removal_parity_gate_present &&
        !inferlab_artifact::json_object_section_or_empty(
             catalog_document, "fallback_removal_parity_gate")
             .empty()) {
      signals.fallback_removal_parity_gate_present = true;
      signals.fallback_removal_parity_gate_removal_allowed = false;
      if (signals.fallback_removal_parity_gate_decision == "not_reported") {
        signals.fallback_removal_parity_gate_decision = "unknown";
      }
    }
    if (catalog_flow_fallback_removal_shortest_path_active(signals)) {
      signals.fallback_removal_shortest_path_blocks_removal = true;
    }
    if (catalog_flow_fallback_removal_parity_gate_active(signals)) {
      signals.fallback_removal_parity_gate_blocks_removal = true;
    }
  }
  if (options.promote_read_issue_to_input_gap && signals.read_issue_count > 0) {
    signals.input_gap_count =
        std::max<std::size_t>(signals.input_gap_count, 1U);
    signals.discrepancy_count = std::max(
        signals.discrepancy_count, catalog_flow_discrepancy_floor(signals));
  }
  return signals;
}

inline CatalogFlowProducerSignals project_catalog_flow_producer_signals(
    const inferlab_artifact::TextReadResult& catalog_read) {
  CatalogFlowProducerSignals signals;
  const auto catalog_summary =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          catalog_read.readable ? catalog_read.text : "", "summary");
  const bool catalog_document_usable =
      catalog_read.present && catalog_read.readable && catalog_summary.usable;

  bool catalog_decisions_valid = false;
  if (catalog_document_usable) {
    const bool drift_decision_valid =
        detail::extract_owned_or_flattened_decision(
            catalog_read.text, catalog_summary.text, "catalog_drift_review",
            "drift_decision", signals.catalog_drift_decision);
    const bool parity_decision_valid =
        detail::extract_owned_or_flattened_decision(
            catalog_read.text, catalog_summary.text,
            "single_source_parity_review", "parity_decision",
            signals.single_source_parity_decision);
    catalog_decisions_valid =
        drift_decision_valid && parity_decision_valid;
  }
  signals.catalog_review_source_usable =
      catalog_document_usable && catalog_decisions_valid;

  const std::string catalog_text =
      signals.catalog_review_source_usable ? catalog_summary.text : "";
  static_cast<CatalogFlowMetricSignals&>(signals) =
      project_catalog_flow_metrics(catalog_text);

  signals.catalog_artifact_present = catalog_read.present;
  signals.catalog_artifact_readable = catalog_read.readable;
  signals.catalog_artifact_read_status = catalog_read.status;
  signals.read_issue_count =
      catalog_read.present && !signals.catalog_review_source_usable ? 1U : 0U;
  signals.input_gap_count =
      signals.catalog_review_source_usable ? 0U : 1U;
  signals.discrepancy_count = catalog_flow_discrepancy_floor(signals);
  const bool physically_unreadable =
      catalog_read.present && !catalog_read.readable;
  const bool invalid_source =
      catalog_read.present && !signals.catalog_review_source_usable;
  if (physically_unreadable) {
    signals.review_decision = "catalog_artifact_unreadable";
  } else if (invalid_source) {
    signals.review_decision = "catalog_artifact_invalid";
  } else if (signals.input_gap_count > 0U) {
    signals.review_decision = "catalog_artifact_missing";
  } else if (signals.missing_surface_count > 0U ||
             signals.missing_step_count > 0U ||
             signals.consumer_field_coverage_field_gap_count > 0U) {
    signals.review_decision = "catalog_flow_repair_required";
  } else if (signals.fallback_retained_surface_count > 0U ||
             signals.consumer_field_coverage_parity_refresh_required_count >
                 0U) {
    signals.review_decision = "fallback_parity_refresh_required";
  } else {
    signals.review_decision = "catalog_flow_review_ready";
  }
  return signals;
}

}  // namespace inferlab_evidence
