#pragma once

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"

#include <cstddef>
#include <string>

namespace inferlab_evidence {

struct EvidenceQuerySnapshotDiffSignals {
  bool artifact_present = false;
  bool artifact_readable = false;
  std::string artifact_read_status = "missing";
  bool query_present = false;
  bool deep_semantic_compared = false;
  bool deep_semantic_truncated = false;
  std::size_t added_deep_path_count = 0;
  std::size_t removed_deep_path_count = 0;
  std::size_t changed_deep_value_count = 0;
  std::string deep_semantic_decision;
  bool release_focus_present = false;
  bool release_focus_review_required = false;
  std::size_t release_focus_current_repo_action_count = 0;
  std::size_t release_focus_changed_deep_path_count = 0;
  std::string release_focus_link_status;
  std::string release_focus_blocker_reason;
};

struct EvidenceQueryBlockerDrilldownSignals {
  std::size_t record_count = 0;
  std::size_t category_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t affected_claim_count = 0;
  std::size_t exact_wiring_location_count = 0;
  std::size_t current_repo_action_count = 0;
  std::size_t current_server_action_count = 0;
  std::size_t target_machine_action_count = 0;
  std::size_t target_product_action_count = 0;
};

struct EvidenceQueryReadinessPathSignals {
  std::string target = "unknown";
  std::string state = "unknown";
  std::size_t blocking_repair_card_count = 0;
  std::size_t step_count = 0;
  std::size_t current_repo_step_count = 0;
  std::size_t target_or_validation_step_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t catalog_flow_discrepancy_count = 0;
  std::size_t fallback_removal_blocked_count = 0;
  std::size_t field_coverage_gap_count = 0;
  std::string actionability_status = "unknown";
  std::string primary_environment = "unknown";
  std::string blocker_reason = "unknown";
  bool current_repo_actionable = false;
  bool validation_or_target_required = false;
  bool source_location_metadata_required = false;
  bool next_action_present = false;
};

struct EvidenceQuerySourceLocationCategorySignals {
  std::size_t category_count = 0;
  std::size_t active_category_count = 0;
  std::size_t exact_category_count = 0;
  std::size_t gap_category_count = 0;
  std::size_t current_repo_gap_category_count = 0;
  std::size_t target_or_validation_gap_category_count = 0;
  std::size_t gap_repair_card_count = 0;
  std::size_t exactness_percent = 100;
};

struct EvidenceQuerySourceLocationWorklistSignals {
  std::size_t item_count = 0;
  std::size_t current_repo_item_count = 0;
  std::size_t target_or_validation_item_count = 0;
  std::size_t gap_count = 0;
  bool focus_present = false;
  bool first_item_present = false;
  bool first_gap_present = false;
  bool first_item_current_repo_action_required = false;
  bool first_item_target_or_validation_action_required = false;
  std::string first_item_environment;
  std::string first_item_artifact;
  std::string first_item_category;
  std::string first_item_source_location;
  std::string first_gap_environment;
  std::string first_gap_category;
  std::string next_action;
};

struct EvidenceQuerySourceLocationSignals {
  EvidenceQuerySourceLocationCategorySignals category_coverage;
  EvidenceQuerySourceLocationWorklistSignals worklist;
};

struct EvidenceQueryRepairNarrativeSignals {
  bool present = false;
  std::string decision = "unknown";
  std::string readiness_state = "unknown";
  std::size_t step_count = 0;
  std::size_t current_repo_action_count = 0;
  std::size_t target_or_validation_action_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t readiness_blocker_count = 0;
  std::size_t affected_claim_count = 0;
  std::size_t exact_wiring_location_count = 0;
};

struct EvidenceQueryRepairGroupSignals {
  std::size_t exact_wiring_location_count = 0;
  std::size_t affected_claim_count = 0;
  bool summary_present = false;
  bool primary_action_present = false;
  bool primary_source_location_present = false;
  bool primary_environment_present = false;
  std::string primary_environment;
  bool primary_category_present = false;
  std::string primary_category;
  bool primary_artifact_present = false;
  std::string primary_artifact;
  std::size_t navigation_step_count = 0;
  std::size_t navigation_current_repo_step_count = 0;
  std::size_t navigation_target_or_validation_step_count = 0;
};

struct EvidenceQueryRepairGuidanceSignals {
  EvidenceQueryRepairNarrativeSignals narrative;
  EvidenceQueryRepairGroupSignals group;
};

struct EvidenceQueryExplainTreeCoverageSignals {
  std::size_t node_count = 0;
  std::size_t source_exact_node_count = 0;
  std::size_t source_gap_node_count = 0;
  std::size_t current_repo_source_gap_count = 0;
  std::size_t target_or_validation_source_gap_count = 0;
  std::size_t source_exactness_percent = 100;
};

struct EvidenceQueryRepairCardGroupSignals {
  std::size_t group_count = 0;
  std::size_t card_count = 0;
  std::size_t current_repo_count = 0;
  std::size_t current_server_count = 0;
  std::size_t target_machine_count = 0;
  std::size_t target_product_count = 0;
  std::size_t source_location_gap_count = 0;
};

struct EvidenceQueryRepairCardPathRankSignals {
  std::size_t rank_count = 0;
  std::size_t current_repo_rank_count = 0;
  std::size_t target_or_validation_rank_count = 0;
  std::size_t source_location_gap_count = 0;
};

struct EvidenceQueryRepairCardRankReadinessSignals {
  std::size_t link_count = 0;
  std::size_t current_repo_link_count = 0;
  std::size_t target_or_validation_link_count = 0;
  std::size_t report_ready_link_count = 0;
  std::size_t replay_ready_link_count = 0;
  std::size_t execute_ready_link_count = 0;
};

struct EvidenceQueryRepairCardSourceExactnessSignals {
  std::size_t card_count = 0;
  std::size_t exact_card_count = 0;
  std::size_t gap_card_count = 0;
  std::size_t current_repo_gap_count = 0;
  std::size_t target_or_validation_gap_count = 0;
  std::size_t exactness_percent = 100;
};

struct EvidenceQueryRepairCardNavigationSignals {
  EvidenceQueryRepairCardGroupSignals group;
  EvidenceQueryRepairCardPathRankSignals path_rank;
  EvidenceQueryRepairCardRankReadinessSignals rank_readiness;
  EvidenceQueryRepairCardSourceExactnessSignals source_exactness;
};

struct EvidenceQuerySelectorResolutionSignals {
  bool present = false;
  bool resolution_status_present = false;
  std::string resolution_status;
  bool kind_present = false;
  std::string kind;
  std::size_t match_count = 0;
  bool ambiguous = false;
  std::size_t source_location_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t current_repo_action_count = 0;
  std::size_t current_server_action_count = 0;
  std::size_t target_machine_action_count = 0;
  std::size_t target_product_action_count = 0;
};

struct EvidenceQuerySelectorRepairCardExportSignals {
  std::size_t card_count = 0;
  std::size_t emitted_count = 0;
  bool truncated = false;
  bool export_status_present = false;
  std::string export_status;
  std::size_t embedded_limit = 0;
  std::size_t full_export_count = 0;
  bool external_export_configured = false;
  bool external_export_written = false;
  bool external_export_status_present = false;
  std::string external_export_status;
  std::size_t current_repo_count = 0;
  std::size_t current_server_count = 0;
  std::size_t target_machine_count = 0;
  std::size_t target_product_count = 0;
  std::size_t source_location_gap_card_count = 0;
  bool export_review_status_present = false;
  std::string export_review_status;
  bool export_blocker_reason_present = false;
  std::string export_blocker_reason;
  bool external_file_required = false;
};

struct EvidenceQuerySelectorActionPlanSignals {
  bool present = false;
  bool decision_present = false;
  std::string decision;
  std::size_t step_count = 0;
  std::size_t current_repo_step_count = 0;
  std::size_t target_or_validation_step_count = 0;
  std::size_t source_location_gap_count = 0;
  std::size_t readiness_blocker_count = 0;
  bool readiness_state_present = false;
  std::string readiness_state;
};

struct EvidenceQuerySelectorSignals {
  EvidenceQuerySelectorResolutionSignals resolution;
  EvidenceQuerySelectorRepairCardExportSignals repair_card_export;
  EvidenceQuerySelectorActionPlanSignals action_plan;
};

struct EvidenceQueryNavigationSignals {
  bool artifact_present = false;
  bool artifact_readable = false;
  std::string artifact_read_status = "missing";
  EvidenceQueryBlockerDrilldownSignals blocker_drilldown;
  EvidenceQueryReadinessPathSignals readiness_path;
  EvidenceQuerySourceLocationSignals source_location;
  EvidenceQueryRepairGuidanceSignals repair_guidance;
  EvidenceQueryExplainTreeCoverageSignals explain_tree_coverage;
  EvidenceQueryRepairCardNavigationSignals repair_card_navigation;
  EvidenceQuerySelectorSignals selector;
  std::size_t query_ux_summary_contract_issue_count = 0;
};

inline inferlab_artifact::StrictJsonObjectSectionOrRootResult
strict_evidence_query_summary_or_root(
    const inferlab_artifact::TextReadResult& evidence_query_read) {
  if (!evidence_query_read.readable) return {};
  return inferlab_artifact::strict_json_object_section_or_flattened_root(
      evidence_query_read.text, "summary");
}

inline std::string evidence_query_summary_or_root_text(
    const inferlab_artifact::TextReadResult& evidence_query_read) {
  return strict_evidence_query_summary_or_root(evidence_query_read).text;
}

inline EvidenceQuerySnapshotDiffSignals
project_evidence_query_snapshot_diff_signals(
    const inferlab_artifact::TextReadResult& evidence_query_read,
    const std::string& summary_text) {
  EvidenceQuerySnapshotDiffSignals signals;
  signals.artifact_present = evidence_query_read.present;
  signals.artifact_readable = evidence_query_read.readable;
  signals.artifact_read_status = evidence_query_read.status;
  if (summary_text.empty()) return signals;
  signals.query_present = inferlab_artifact::json_bool_value_or_false(
      summary_text, "snapshot_diff_query_present");
  signals.deep_semantic_compared =
      inferlab_artifact::json_bool_value_or_false(
          summary_text, "snapshot_diff_deep_semantic_compared");
  signals.deep_semantic_truncated =
      inferlab_artifact::json_bool_value_or_false(
          summary_text, "snapshot_diff_deep_semantic_truncated");
  signals.added_deep_path_count = inferlab_artifact::json_number_value_or_zero(
      summary_text, "snapshot_diff_added_deep_path_count");
  signals.removed_deep_path_count =
      inferlab_artifact::json_number_value_or_zero(
          summary_text, "snapshot_diff_removed_deep_path_count");
  signals.changed_deep_value_count =
      inferlab_artifact::json_number_value_or_zero(
          summary_text, "snapshot_diff_changed_deep_value_count");
  signals.deep_semantic_decision =
      inferlab_artifact::extract_semantic_string_key_fallback(
          summary_text, "snapshot_diff_deep_semantic_decision");
  signals.release_focus_present = inferlab_artifact::json_bool_value_or_false(
      summary_text, "snapshot_to_release_focus_present");
  signals.release_focus_review_required =
      inferlab_artifact::json_bool_value_or_false(
          summary_text, "snapshot_to_release_focus_review_required");
  signals.release_focus_current_repo_action_count =
      inferlab_artifact::json_number_value_or_zero(
          summary_text,
          "snapshot_to_release_focus_current_repo_action_count");
  signals.release_focus_changed_deep_path_count =
      inferlab_artifact::json_number_value_or_zero(
          summary_text, "snapshot_to_release_focus_changed_deep_path_count");
  signals.release_focus_link_status =
      inferlab_artifact::extract_semantic_string_key_fallback(
          summary_text, "snapshot_to_release_focus_link_status");
  signals.release_focus_blocker_reason =
      inferlab_artifact::extract_semantic_string_key_fallback(
          summary_text, "snapshot_to_release_focus_blocker_reason");
  return signals;
}

inline EvidenceQuerySnapshotDiffSignals
project_evidence_query_snapshot_diff_signals(
    const inferlab_artifact::TextReadResult& evidence_query_read) {
  return project_evidence_query_snapshot_diff_signals(
      evidence_query_read,
      evidence_query_summary_or_root_text(evidence_query_read));
}

inline std::string evidence_query_string_value_or_default(
    const std::string& text, const std::string& key,
    const std::string& fallback) {
  std::string value;
  return inferlab_artifact::extract_semantic_string_key_fallback(text, key,
                                                                 value)
             ? value
             : fallback;
}

inline std::size_t evidence_query_unsigned_value_or_default(
    const std::string& text, const std::string& key,
    const std::size_t fallback) {
  const std::string value = inferlab_artifact::json_number_or_null(text, key);
  return value == "null" ? fallback
                         : inferlab_artifact::unsigned_value_or_zero(value);
}

inline void add_evidence_query_contract_issue(
    std::size_t* issue_count, const char* label) {
  if (issue_count == nullptr) return;
  *issue_count = inferlab::internal::checked_add_size(*issue_count, 1U, label);
}

inline std::size_t required_evidence_query_unsigned(
    const std::string& text, const char* key, const std::size_t fallback,
    std::size_t* issue_count, const bool percentage = false) {
  const auto field = inferlab_artifact::project_unsigned_field(text, {key});
  if (field.present && !field.invalid &&
      (!percentage || field.value <= 100U)) {
    return field.value;
  }
  add_evidence_query_contract_issue(
      issue_count, "Evidence Query required unsigned field issue");
  return fallback;
}

inline std::string required_evidence_query_string(
    const std::string& text, const char* key, const std::string& fallback,
    std::size_t* issue_count) {
  const auto field = inferlab_artifact::project_string_field(text, {key});
  if (field.present && !field.invalid) return field.value;
  add_evidence_query_contract_issue(
      issue_count, "Evidence Query required string field issue");
  return fallback;
}

inline bool required_evidence_query_bool(const std::string& text,
                                         const char* key,
                                         std::size_t* issue_count) {
  const auto field = inferlab_artifact::project_bool_field(text, {key});
  if (field.present && !field.invalid) return field.value;
  add_evidence_query_contract_issue(
      issue_count, "Evidence Query required bool field issue");
  return false;
}

inline inferlab_artifact::StringFieldProjection
required_evidence_query_string_projection(const std::string& text,
                                          const char* key,
                                          std::size_t* issue_count) {
  auto field = inferlab_artifact::project_string_field(text, {key});
  if (!field.present || field.invalid) {
    add_evidence_query_contract_issue(
        issue_count, "Evidence Query required string field issue");
  }
  return field;
}

inline EvidenceQueryBlockerDrilldownSignals
project_evidence_query_blocker_drilldown_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryBlockerDrilldownSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.record_count = count("query_ux_blocker_drilldown_record_count");
  signals.category_count = count("query_ux_blocker_drilldown_category_count");
  signals.source_location_gap_count =
      count("query_ux_blocker_drilldown_source_location_gap_count");
  signals.affected_claim_count =
      count("query_ux_blocker_drilldown_affected_claim_count");
  signals.exact_wiring_location_count =
      count("query_ux_blocker_drilldown_exact_wiring_location_count");
  signals.current_repo_action_count =
      count("query_ux_blocker_drilldown_current_repo_action_count");
  signals.current_server_action_count =
      count("query_ux_blocker_drilldown_current_server_action_count");
  signals.target_machine_action_count =
      count("query_ux_blocker_drilldown_target_machine_action_count");
  signals.target_product_action_count =
      count("query_ux_blocker_drilldown_target_product_action_count");
  return signals;
}

inline EvidenceQueryReadinessPathSignals
project_evidence_query_readiness_path_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryReadinessPathSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  const auto boolean = [&](const std::string& key) {
    return required_evidence_query_bool(
        text, key.c_str(), contract_issue_count);
  };
  signals.target = required_evidence_query_string(
      text, "query_ux_readiness_path_target", "unknown",
      contract_issue_count);
  signals.state = required_evidence_query_string(
      text, "query_ux_readiness_path_state", "unknown",
      contract_issue_count);
  signals.blocking_repair_card_count =
      count("query_ux_readiness_path_blocking_repair_card_count");
  signals.step_count = count("query_ux_readiness_path_step_count");
  signals.current_repo_step_count =
      count("query_ux_readiness_path_current_repo_step_count");
  signals.target_or_validation_step_count =
      count("query_ux_readiness_path_target_or_validation_step_count");
  signals.source_location_gap_count =
      count("query_ux_readiness_path_source_location_gap_count");
  signals.catalog_flow_discrepancy_count =
      count("query_ux_readiness_path_catalog_flow_discrepancy_count");
  signals.fallback_removal_blocked_count =
      count("query_ux_readiness_path_fallback_removal_blocked_count");
  signals.field_coverage_gap_count =
      count("query_ux_readiness_path_field_coverage_gap_count");
  signals.actionability_status = required_evidence_query_string(
      text, "query_ux_readiness_path_actionability_status", "unknown",
      contract_issue_count);
  signals.primary_environment = required_evidence_query_string(
      text, "query_ux_readiness_path_primary_environment", "unknown",
      contract_issue_count);
  signals.blocker_reason = required_evidence_query_string(
      text, "query_ux_readiness_path_blocker_reason", "unknown",
      contract_issue_count);
  signals.current_repo_actionable =
      boolean("query_ux_readiness_path_current_repo_actionable");
  signals.validation_or_target_required =
      boolean("query_ux_readiness_path_validation_or_target_required");
  signals.source_location_metadata_required =
      boolean("query_ux_readiness_path_source_location_metadata_required");
  signals.next_action_present =
      boolean("query_ux_readiness_path_next_action_present");
  return signals;
}

inline EvidenceQuerySourceLocationCategorySignals
project_evidence_query_source_location_category_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQuerySourceLocationCategorySignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.category_count = count("query_ux_source_location_category_count");
  signals.active_category_count =
      count("query_ux_source_location_active_category_count");
  signals.exact_category_count =
      count("query_ux_source_location_exact_category_count");
  signals.gap_category_count =
      count("query_ux_source_location_gap_category_count");
  signals.current_repo_gap_category_count =
      count("query_ux_source_location_current_repo_gap_category_count");
  signals.target_or_validation_gap_category_count = count(
      "query_ux_source_location_target_or_validation_gap_category_count");
  signals.gap_repair_card_count =
      count("query_ux_source_location_gap_repair_card_count");
  signals.exactness_percent = required_evidence_query_unsigned(
      text, "query_ux_source_location_category_exactness_percent", 100U,
      contract_issue_count, true);
  return signals;
}

inline EvidenceQuerySourceLocationWorklistSignals
project_evidence_query_source_location_worklist_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQuerySourceLocationWorklistSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  const auto boolean = [&](const std::string& key) {
    return required_evidence_query_bool(
        text, key.c_str(), contract_issue_count);
  };
  signals.item_count = count("query_ux_source_location_worklist_item_count");
  signals.current_repo_item_count =
      count("query_ux_source_location_worklist_current_repo_item_count");
  signals.target_or_validation_item_count = count(
      "query_ux_source_location_worklist_target_or_validation_item_count");
  signals.gap_count = count("query_ux_source_location_worklist_gap_count");
  signals.focus_present =
      boolean("query_ux_source_location_worklist_focus_present");
  signals.first_item_present =
      boolean("query_ux_source_location_worklist_focus_first_item_present");
  signals.first_gap_present =
      boolean("query_ux_source_location_worklist_focus_first_gap_present");
  signals.first_item_current_repo_action_required = boolean(
      "query_ux_source_location_worklist_focus_first_item_current_repo_action_required");
  signals.first_item_target_or_validation_action_required = boolean(
      "query_ux_source_location_worklist_focus_first_item_target_or_validation_action_required");
  signals.first_item_environment = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_first_item_environment",
      "", contract_issue_count);
  signals.first_item_artifact = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_first_item_artifact", "",
      contract_issue_count);
  signals.first_item_category = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_first_item_category", "",
      contract_issue_count);
  signals.first_item_source_location = required_evidence_query_string(
      text,
      "query_ux_source_location_worklist_focus_first_item_source_location", "",
      contract_issue_count);
  signals.first_gap_environment = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_first_gap_environment", "",
      contract_issue_count);
  signals.first_gap_category = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_first_gap_category", "",
      contract_issue_count);
  signals.next_action = required_evidence_query_string(
      text, "query_ux_source_location_worklist_focus_next_action", "",
      contract_issue_count);
  return signals;
}

inline EvidenceQueryRepairNarrativeSignals
project_evidence_query_repair_narrative_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairNarrativeSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.present = required_evidence_query_bool(
      text, "query_ux_repair_narrative_present", contract_issue_count);
  signals.decision = required_evidence_query_string(
      text, "query_ux_repair_narrative_decision", "unknown",
      contract_issue_count);
  signals.readiness_state = required_evidence_query_string(
      text, "query_ux_repair_narrative_readiness_state", "unknown",
      contract_issue_count);
  signals.step_count = count("query_ux_repair_narrative_step_count");
  signals.current_repo_action_count =
      count("query_ux_repair_narrative_current_repo_action_count");
  signals.target_or_validation_action_count =
      count("query_ux_repair_narrative_target_or_validation_action_count");
  signals.source_location_gap_count =
      count("query_ux_repair_narrative_source_location_gap_count");
  signals.readiness_blocker_count =
      count("query_ux_repair_narrative_readiness_blocker_count");
  signals.affected_claim_count =
      count("query_ux_repair_narrative_affected_claim_count");
  signals.exact_wiring_location_count =
      count("query_ux_repair_narrative_exact_wiring_location_count");
  return signals;
}

inline EvidenceQueryRepairGroupSignals
project_evidence_query_repair_group_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairGroupSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.exact_wiring_location_count =
      count("query_ux_repair_group_exact_wiring_location_count");
  signals.affected_claim_count =
      count("query_ux_repair_group_affected_claim_count");
  signals.summary_present = required_evidence_query_bool(
      text, "query_ux_repair_group_summary_present", contract_issue_count);
  signals.primary_action_present = required_evidence_query_bool(
      text, "query_ux_repair_group_primary_action_present",
      contract_issue_count);
  signals.primary_source_location_present = required_evidence_query_bool(
      text, "query_ux_repair_group_primary_source_location_present",
      contract_issue_count);
  const auto primary_environment = required_evidence_query_string_projection(
      text, "query_ux_repair_group_primary_environment",
      contract_issue_count);
  signals.primary_environment_present =
      primary_environment.present && !primary_environment.invalid;
  signals.primary_environment = primary_environment.value;
  const auto primary_category = required_evidence_query_string_projection(
      text, "query_ux_repair_group_primary_category", contract_issue_count);
  signals.primary_category_present =
      primary_category.present && !primary_category.invalid;
  signals.primary_category = primary_category.value;
  const auto primary_artifact = required_evidence_query_string_projection(
      text, "query_ux_repair_group_primary_artifact", contract_issue_count);
  signals.primary_artifact_present =
      primary_artifact.present && !primary_artifact.invalid;
  signals.primary_artifact = primary_artifact.value;
  signals.navigation_step_count =
      count("query_ux_repair_group_navigation_step_count");
  signals.navigation_current_repo_step_count =
      count("query_ux_repair_group_navigation_current_repo_step_count");
  signals.navigation_target_or_validation_step_count = count(
      "query_ux_repair_group_navigation_target_or_validation_step_count");
  return signals;
}

inline EvidenceQueryExplainTreeCoverageSignals
project_evidence_query_explain_tree_coverage_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryExplainTreeCoverageSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.node_count = count("query_ux_explain_tree_node_count");
  signals.source_exact_node_count =
      count("query_ux_explain_tree_source_exact_node_count");
  signals.source_gap_node_count =
      count("query_ux_explain_tree_source_gap_node_count");
  signals.current_repo_source_gap_count =
      count("query_ux_explain_tree_current_repo_source_gap_count");
  signals.target_or_validation_source_gap_count =
      count("query_ux_explain_tree_target_or_validation_source_gap_count");
  signals.source_exactness_percent = required_evidence_query_unsigned(
      text, "query_ux_explain_tree_source_exactness_percent", 100U,
      contract_issue_count, true);
  return signals;
}

inline EvidenceQueryRepairCardGroupSignals
project_evidence_query_repair_card_group_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairCardGroupSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.group_count = count("query_ux_repair_card_group_count");
  signals.card_count = count("query_ux_repair_card_group_card_count");
  signals.current_repo_count =
      count("query_ux_repair_card_group_current_repo_count");
  signals.current_server_count =
      count("query_ux_repair_card_group_current_server_count");
  signals.target_machine_count =
      count("query_ux_repair_card_group_target_machine_count");
  signals.target_product_count =
      count("query_ux_repair_card_group_target_product_count");
  signals.source_location_gap_count =
      count("query_ux_repair_card_group_source_location_gap_count");
  return signals;
}

inline EvidenceQueryRepairCardPathRankSignals
project_evidence_query_repair_card_path_rank_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairCardPathRankSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.rank_count = count("query_ux_repair_card_path_rank_count");
  signals.current_repo_rank_count =
      count("query_ux_repair_card_path_current_repo_rank_count");
  signals.target_or_validation_rank_count =
      count("query_ux_repair_card_path_target_or_validation_rank_count");
  signals.source_location_gap_count =
      count("query_ux_repair_card_path_source_location_gap_count");
  return signals;
}

inline EvidenceQueryRepairCardRankReadinessSignals
project_evidence_query_repair_card_rank_readiness_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairCardRankReadinessSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.link_count = count("query_ux_repair_card_rank_readiness_link_count");
  signals.current_repo_link_count =
      count("query_ux_repair_card_rank_readiness_current_repo_link_count");
  signals.target_or_validation_link_count = count(
      "query_ux_repair_card_rank_readiness_target_or_validation_link_count");
  signals.report_ready_link_count =
      count("query_ux_repair_card_rank_readiness_report_ready_link_count");
  signals.replay_ready_link_count =
      count("query_ux_repair_card_rank_readiness_replay_ready_link_count");
  signals.execute_ready_link_count =
      count("query_ux_repair_card_rank_readiness_execute_ready_link_count");
  return signals;
}

inline EvidenceQueryRepairCardSourceExactnessSignals
project_evidence_query_repair_card_source_exactness_signals(
    const std::string& text, std::size_t* contract_issue_count = nullptr) {
  EvidenceQueryRepairCardSourceExactnessSignals signals;
  const auto count = [&](const std::string& key) {
    return required_evidence_query_unsigned(
        text, key.c_str(), 0U, contract_issue_count);
  };
  signals.card_count = count("query_ux_repair_card_source_exactness_card_count");
  signals.exact_card_count =
      count("query_ux_repair_card_source_exactness_exact_card_count");
  signals.gap_card_count =
      count("query_ux_repair_card_source_exactness_gap_card_count");
  signals.current_repo_gap_count =
      count("query_ux_repair_card_source_exactness_current_repo_gap_count");
  signals.target_or_validation_gap_count = count(
      "query_ux_repair_card_source_exactness_target_or_validation_gap_count");
  signals.exactness_percent = required_evidence_query_unsigned(
      text, "query_ux_repair_card_source_exactness_percent", 100U,
      contract_issue_count, true);
  return signals;
}

inline EvidenceQuerySelectorResolutionSignals
project_evidence_query_selector_resolution_signals(const std::string& text) {
  EvidenceQuerySelectorResolutionSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(text, key);
  };
  signals.present =
      inferlab_artifact::contains_key(text, "selector_resolution");
  signals.resolution_status_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_resolution_status",
          signals.resolution_status);
  signals.kind_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_kind", signals.kind);
  signals.match_count = count("query_operation_selector_match_count");
  signals.ambiguous = inferlab_artifact::json_bool_value_or_false(
      text, "query_operation_selector_ambiguous");
  signals.source_location_count =
      count("query_operation_selector_source_location_count");
  signals.source_location_gap_count =
      count("query_operation_selector_source_location_gap_count");
  signals.current_repo_action_count =
      count("query_operation_selector_current_repo_action_count");
  signals.current_server_action_count =
      count("query_operation_selector_current_server_action_count");
  signals.target_machine_action_count =
      count("query_operation_selector_target_machine_action_count");
  signals.target_product_action_count =
      count("query_operation_selector_target_product_action_count");
  return signals;
}

inline EvidenceQuerySelectorRepairCardExportSignals
project_evidence_query_selector_repair_card_export_signals(
    const std::string& text) {
  EvidenceQuerySelectorRepairCardExportSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(text, key);
  };
  signals.card_count = count("query_operation_selector_repair_card_count");
  signals.emitted_count =
      count("query_operation_selector_repair_card_emitted_count");
  signals.truncated = inferlab_artifact::json_bool_value_or_false(
      text, "query_operation_selector_repair_card_truncated");
  signals.export_status_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_repair_card_export_status",
          signals.export_status);
  signals.embedded_limit =
      count("query_operation_selector_repair_card_embedded_limit");
  signals.full_export_count =
      count("query_operation_selector_repair_card_full_export_count");
  signals.external_export_configured =
      inferlab_artifact::json_bool_value_or_false(
          text,
          "query_operation_selector_repair_card_external_export_configured");
  signals.external_export_written =
      inferlab_artifact::json_bool_value_or_false(
          text,
          "query_operation_selector_repair_card_external_export_written");
  signals.external_export_status_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text,
          "query_operation_selector_repair_card_external_export_status",
          signals.external_export_status);
  signals.current_repo_count =
      count("query_operation_selector_repair_card_current_repo_count");
  signals.current_server_count =
      count("query_operation_selector_repair_card_current_server_count");
  signals.target_machine_count =
      count("query_operation_selector_repair_card_target_machine_count");
  signals.target_product_count =
      count("query_operation_selector_repair_card_target_product_count");
  signals.source_location_gap_card_count = count(
      "query_operation_selector_repair_card_source_location_gap_card_count");
  signals.export_review_status_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_repair_card_export_review_status",
          signals.export_review_status);
  signals.export_blocker_reason_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_repair_card_export_blocker_reason",
          signals.export_blocker_reason);
  signals.external_file_required =
      inferlab_artifact::json_bool_value_or_false(
          text,
          "query_operation_selector_repair_card_external_file_required");
  return signals;
}

inline EvidenceQuerySelectorActionPlanSignals
project_evidence_query_selector_action_plan_signals(const std::string& text) {
  EvidenceQuerySelectorActionPlanSignals signals;
  const auto count = [&](const std::string& key) {
    return inferlab_artifact::json_number_value_or_zero(text, key);
  };
  signals.present = inferlab_artifact::json_bool_value_or_false(
      text, "query_operation_selector_action_plan_present");
  signals.decision_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_action_plan_decision",
          signals.decision);
  signals.step_count =
      count("query_operation_selector_action_plan_step_count");
  signals.current_repo_step_count =
      count("query_operation_selector_action_plan_current_repo_step_count");
  signals.target_or_validation_step_count = count(
      "query_operation_selector_action_plan_target_or_validation_step_count");
  signals.source_location_gap_count =
      count("query_operation_selector_action_plan_source_location_gap_count");
  signals.readiness_blocker_count = count(
      "query_operation_selector_action_plan_readiness_blocker_count");
  signals.readiness_state_present =
      inferlab_artifact::extract_semantic_string_key_fallback(
          text, "query_operation_selector_action_plan_readiness_state",
          signals.readiness_state);
  return signals;
}

inline EvidenceQueryNavigationSignals project_evidence_query_navigation_signals(
    const inferlab_artifact::TextReadResult& evidence_query_read,
    const std::string& summary_text) {
  EvidenceQueryNavigationSignals signals;
  signals.artifact_present = evidence_query_read.present;
  signals.artifact_readable = evidence_query_read.readable;
  signals.artifact_read_status = evidence_query_read.status;
  if (summary_text.empty()) return signals;
  signals.blocker_drilldown =
      project_evidence_query_blocker_drilldown_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.readiness_path =
      project_evidence_query_readiness_path_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.source_location.category_coverage =
      project_evidence_query_source_location_category_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.source_location.worklist =
      project_evidence_query_source_location_worklist_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_guidance.narrative =
      project_evidence_query_repair_narrative_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_guidance.group =
      project_evidence_query_repair_group_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.explain_tree_coverage =
      project_evidence_query_explain_tree_coverage_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_card_navigation.group =
      project_evidence_query_repair_card_group_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_card_navigation.path_rank =
      project_evidence_query_repair_card_path_rank_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_card_navigation.rank_readiness =
      project_evidence_query_repair_card_rank_readiness_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.repair_card_navigation.source_exactness =
      project_evidence_query_repair_card_source_exactness_signals(
          summary_text, &signals.query_ux_summary_contract_issue_count);
  signals.selector.resolution =
      project_evidence_query_selector_resolution_signals(summary_text);
  signals.selector.resolution.present =
      !inferlab_artifact::json_object_section_or_empty(
           evidence_query_read.text, "selector_resolution")
           .empty();
  signals.selector.repair_card_export =
      project_evidence_query_selector_repair_card_export_signals(summary_text);
  signals.selector.action_plan =
      project_evidence_query_selector_action_plan_signals(summary_text);
  return signals;
}

inline EvidenceQueryNavigationSignals project_evidence_query_navigation_signals(
    const inferlab_artifact::TextReadResult& evidence_query_read) {
  return project_evidence_query_navigation_signals(
      evidence_query_read,
      evidence_query_summary_or_root_text(evidence_query_read));
}

}  // namespace inferlab_evidence
