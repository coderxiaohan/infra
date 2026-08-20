#include "native_core_audit_register_file_policy.hpp"

#include "checked_index.hpp"
#include "native_core_audit_register_source.hpp"

#include <algorithm>
#include <stdexcept>

namespace inferlab_native_audit {
namespace {

std::vector<IssueSpec> make_issue_specs() {
  return {
      {"source_unreadable",
       "Source file could not be read",
       "current_repo_fixable",
       "high",
       "inspect path permissions and repo checkout state, then refresh the audit register",
       "rerun native audit register on the target validation machine after repo checkout is stable"},
      {"path_output_needs_redaction_review",
       "Path output needs redaction review",
       "current_repo_fixable",
       "medium",
       "route path/string output through plan_json_utils or artifact_metadata_reader redaction helpers",
       "confirm refreshed artifacts only expose stable path labels on the target machine"},
      {"numeric_hot_path_needs_checked_arithmetic_review",
       "Numeric hot path needs checked arithmetic review",
       "current_repo_fixable",
       "high",
       "replace unchecked size/index/count arithmetic with checked_index helpers or record a narrow deferral",
       "run target native/CUDA correctness after current-repo arithmetic review is closed"},
      {"artifact_main_needs_cli_or_contract_review",
       "Artifact main needs CLI or contract review",
       "current_repo_fixable",
       "high",
       "standardize --repo-root/--output validation, dry-run metadata, claim_scope, mode, executed, and replayed fields",
       "refresh contract/freshness/query/release artifacts after the entrypoint contract is updated"},
      {"artifact_main_should_emit_not_run_boundary",
       "Artifact main should emit not-run boundary",
       "current_repo_fixable",
       "medium",
       "add explicit not_run or common dry-run metadata so reports cannot imply validation was executed",
       "verify final report still distinguishes dry-run/replay/execute after refresh"},
      {"artifact_main_should_emit_executed_flag",
       "Artifact main should emit executed flag",
       "current_repo_fixable",
       "high",
       "emit executed=false for dry-run artifacts or executed=true only for real target-machine execute artifacts",
       "confirm target-machine execute artifacts flip executed only when the producer actually ran"},
      {"artifact_main_should_emit_replayed_flag",
       "Artifact main should emit replayed flag",
       "current_repo_fixable",
       "high",
       "emit replayed=false for dry-run artifacts or replayed=true only for imported product/native replay evidence",
       "confirm replay artifacts remain separate from execute artifacts in release gate output"},
  };
}

void validate_issue_specs(const std::vector<IssueSpec>& specs) {
  for (std::size_t index = 0; index < specs.size(); ++index) {
    const auto& spec = specs[index];
    if (spec.id.empty() || spec.title.empty() ||
        spec.repair_environment.empty() || spec.severity.empty() ||
        spec.current_repo_action.empty() || spec.target_machine_action.empty()) {
      throw std::logic_error("native audit issue spec fields must be nonempty");
    }
    for (std::size_t previous = 0; previous < index; ++previous) {
      if (specs[previous].id == spec.id) {
        throw std::logic_error("native audit issue spec ids must be unique");
      }
    }
  }
}

bool is_artifact_entrypoint(const FileAudit& audit) {
  return has_suffix(audit.file, "_main.cpp");
}

}  // namespace

const std::vector<IssueSpec>& issue_specs() {
  static const std::vector<IssueSpec> specs = [] {
    auto values = make_issue_specs();
    validate_issue_specs(values);
    return values;
  }();
  return specs;
}

bool audit_has_issue(const FileAudit& audit, const std::string& issue) {
  return std::find(audit.issues_found.begin(), audit.issues_found.end(),
                   issue) != audit.issues_found.end();
}

std::size_t file_completion_blocker_count_for(const FileAudit& audit) {
  std::size_t blockers = audit.issues_found.size();
  if (audit.claim_boundary_risk == "claim_scope_boundary_needs_review") {
    blockers = inferlab::internal::checked_add_size(
        blockers, 1U, "file completion claim boundary blocker count");
  }
  if (audit.review_status == "deferred_with_reason" &&
      audit.issues_found.empty()) {
    blockers = inferlab::internal::checked_add_size(
        blockers, 1U, "file completion deferred blocker count");
  }
  return blockers;
}

std::string file_completion_state_for(const FileAudit& audit) {
  if (audit_has_issue(audit, "source_unreadable")) {
    return "source_unreadable";
  }
  return file_completion_blocker_count_for(audit) > 0
             ? "current_repo_review_required"
             : "static_file_closed";
}

std::string file_completion_blocker_category_for(const FileAudit& audit) {
  const std::string primary_issue = primary_issue_for(audit);
  if (primary_issue != "none") return primary_issue;
  if (audit.claim_boundary_risk == "claim_scope_boundary_needs_review") {
    return "claim_scope_boundary_needs_review";
  }
  if (audit.review_status == "deferred_with_reason") {
    return "deferred_review_backlog";
  }
  return "none";
}

int review_priority_rank(const FileAudit& audit) {
  if (audit_has_issue(audit, "source_unreadable")) return 0;
  if (audit_has_issue(audit,
                      "numeric_hot_path_needs_checked_arithmetic_review") ||
      audit_has_issue(audit, "artifact_main_needs_cli_or_contract_review") ||
      audit.claim_boundary_risk == "claim_scope_boundary_needs_review") {
    return 1;
  }
  if (!audit.issues_found.empty()) return 2;
  if (audit.review_status == "deferred_with_reason") return 3;
  return 4;
}

std::string review_priority_label(const int rank) {
  if (rank <= 0) return "p0_scope_or_readability";
  if (rank == 1) return "p0_contract_or_hot_path";
  if (rank == 2) return "p1_static_issue";
  if (rank == 3) return "p1_human_review_backlog";
  return "closed_or_low";
}

std::string review_lane_for(const FileAudit& audit) {
  if (audit.subsystem == "shared_infrastructure") {
    return "shared_infrastructure_first";
  }
  if (audit.subsystem == "kv_cache" ||
      audit.subsystem == "scheduler_batching" ||
      audit.subsystem == "attention_paged_kv" ||
      audit.subsystem == "matrix_ops" ||
      audit.subsystem == "normalization" || audit.subsystem == "rope" ||
      audit.subsystem == "quantization" || audit.subsystem == "decoder") {
    return "core_mechanism_hot_path";
  }
  if (audit.subsystem == "evidence_release_governance") {
    return "evidence_query_release_gate";
  }
  if (is_artifact_entrypoint(audit)) return "artifact_entrypoint_contract";
  return "remaining_native_core_review";
}

std::string audit_phase_for(const FileAudit& audit) {
  if (audit.subsystem == "shared_infrastructure") {
    return "phase_1_shared_infrastructure";
  }
  if (audit.subsystem == "evidence_release_governance") {
    return "phase_4_evidence_release_query";
  }
  if (is_artifact_entrypoint(audit)) return "phase_3_artifact_probe_main";
  return "phase_2_core_mechanism";
}

std::string primary_issue_for(const FileAudit& audit) {
  if (!audit.issues_found.empty()) return audit.issues_found.front();
  if (audit.review_status == "deferred_with_reason") {
    return "deferred_review_backlog";
  }
  return "none";
}

std::string current_repo_action_for_file(const FileAudit& audit) {
  const std::string primary_issue = primary_issue_for(audit);
  for (const auto& spec : issue_specs()) {
    if (spec.id == primary_issue) return spec.current_repo_action;
  }
  if (primary_issue == "deferred_review_backlog") {
    return "perform source review for this file and update the audit row to reviewed, reviewed_with_fix, or a precise deferral";
  }
  return "keep this file covered by the audit register when native_core changes";
}

std::string target_machine_action_for_file(const FileAudit& audit) {
  const std::string primary_issue = primary_issue_for(audit);
  for (const auto& spec : issue_specs()) {
    if (spec.id == primary_issue) return spec.target_machine_action;
  }
  return "after current-repo audit closure, include this subsystem in target CMake/native/CUDA validation as applicable";
}

bool is_open_review_item(const FileAudit& audit) {
  return audit.review_status == "deferred_with_reason" ||
         !audit.issues_found.empty() ||
         audit.claim_boundary_risk == "claim_scope_boundary_needs_review";
}

bool open_review_item_pointer_less(const FileAudit* lhs,
                                   const FileAudit* rhs) {
  if (lhs == nullptr || rhs == nullptr) {
    throw std::logic_error(
        "native audit review ordering contains a null file row");
  }
  const int lhs_priority = review_priority_rank(*lhs);
  const int rhs_priority = review_priority_rank(*rhs);
  if (lhs_priority != rhs_priority) return lhs_priority < rhs_priority;
  if (lhs->subsystem != rhs->subsystem) {
    return lhs->subsystem < rhs->subsystem;
  }
  return lhs->file < rhs->file;
}

}  // namespace inferlab_native_audit
