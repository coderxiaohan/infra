#include "product_profile_contract_check_model.hpp"

#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "mature_product_profile_registry.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace inferlab::product_profile_contract_check {
namespace {

using mature_product_profiles::ProductProfileArtifactContract;

RecordDecision decide_record(const ProductProfileArtifactContract& contract,
                             const bool strict_missing) {
  const auto& profile =
      mature_product_profiles::contract_profile(contract);
  if (!mature_product_profiles::profile_is_default_required(profile) &&
      !contract.present && contract.text_read_status == "missing") {
    return RecordDecision::not_selected_optional;
  }
  if (!contract.present && contract.text_read_status == "missing" &&
      !strict_missing) {
    return RecordDecision::pending_missing_artifact;
  }
  return contract.contract_ready ? RecordDecision::pass
                                 : RecordDecision::fail;
}

std::string recommendation(const ProductProfileArtifactContract& contract,
                           const RecordDecision decision) {
  if (decision == RecordDecision::pending_missing_artifact) {
    return "pending_until_product_profile_dry_run_execute_or_replay_generates_artifact";
  }
  if (decision == RecordDecision::not_selected_optional) {
    return "optional_profile_not_selected_use_manual_profile_entry_when_needed";
  }
  if (contract.text_read_status == "unreadable_file_probe" ||
      contract.path_probe_status == "path_probe_error") {
    return "investigate_product_profile_artifact_path_probe_error";
  }
  if (!contract.present) {
    return "generate_or_copy_product_profile_artifact_before_release_gate";
  }
  if (!contract.regular_file) {
    return "replace_non_regular_artifact_path_with_a_product_profile_json_file";
  }
  if (!contract.readable) {
    return "repair_artifact_permissions_or_regenerate_product_profile_artifact";
  }
  if (!contract.parse_ok) {
    return "regenerate_duplicate_key_free_product_profile_json";
  }
  if (!contract.missing_fields.empty() ||
      !contract.invalid_type_fields.empty()) {
    return "fix_product_profile_required_field_presence_and_types";
  }
  if (!contract.mode_flags_consistent || !contract.mode_status_consistent) {
    return "repair_product_profile_mode_status_and_execution_flags";
  }
  if (!contract.claim_scope_consistent ||
      !contract.replay_evidence_consistent) {
    return "repair_product_profile_claim_and_replay_evidence_contract";
  }
  if (!contract.contract_ready) {
    return "restore_product_identity_boundary_commands_and_metric_contract";
  }
  if (contract.can_claim_product_performance) {
    return "preserve_source_artifact_and_wire_explicit_metrics_to_reports";
  }
  return "artifact_contract_ready_for_summary_and_report_wiring_without_performance_claim";
}

void add_count(std::size_t& value, const char* label) {
  value = inferlab::internal::checked_add_size(value, 1U, label);
}

void aggregate_record(AuditSummary& summary, const AuditRecord& record) {
  const auto& contract = record.contract;
  const auto& profile =
      mature_product_profiles::contract_profile(contract);
  if (mature_product_profiles::profile_is_default_required(profile)) {
    add_count(summary.default_required_product_count,
              "default required product profile count");
  } else {
    add_count(summary.optional_on_demand_product_count,
              "optional on-demand product profile count");
  }
  if (contract.present) add_count(summary.present_artifacts,
                                  "product profile present artifact count");
  if (!contract.present) add_count(summary.missing_artifacts,
                                   "product profile missing artifact count");
  if (!contract.present &&
      record.decision != RecordDecision::not_selected_optional) {
    add_count(summary.required_missing_artifacts,
              "required product profile missing artifact count");
  }
  if (contract.readable) add_count(summary.readable_artifacts,
                                   "product profile readable artifact count");
  if (contract.parse_ok) add_count(summary.parsed_artifacts,
                                   "product profile parsed artifact count");
  if ((contract.present &&
       (!contract.regular_file || !contract.readable)) ||
      (!contract.present && contract.text_read_status != "missing")) {
    add_count(summary.read_issue_count,
              "product profile artifact read issue count");
  }
  if (contract.present && !contract.contract_ready) {
    add_count(summary.malformed_or_invalid_artifact_count,
              "product profile malformed or invalid artifact count");
  }
  switch (record.decision) {
    case RecordDecision::pass:
      add_count(summary.passed_records,
                "product profile passed record count");
      break;
    case RecordDecision::pending_missing_artifact:
      add_count(summary.pending_records,
                "product profile pending record count");
      break;
    case RecordDecision::not_selected_optional:
      add_count(summary.not_selected_optional_records,
                "optional product profile not-selected record count");
      return;
    case RecordDecision::fail:
      add_count(summary.failed_records,
                "product profile failed record count");
      break;
  }
  add_count(summary.selected_product_count,
            "selected product profile count");
  if (contract.boundary_score == 1.0) {
    add_count(summary.boundary_pass_count,
              "product profile boundary pass count");
  }
  if (contract.execute_gate_present) {
    add_count(summary.execute_gate_pass_count,
              "product profile execute gate pass count");
  }
  if (contract.mode_flags_consistent) {
    add_count(summary.mode_flag_pass_count,
              "product profile mode flag pass count");
  }
  if (contract.claim_scope_consistent) {
    add_count(summary.claim_scope_pass_count,
              "product profile claim scope pass count");
  }
  if (contract.product_evidence_eligible) {
    add_count(summary.product_evidence_eligible_count,
              "product profile evidence eligible count");
  }
  if (contract.can_claim_product_performance) {
    add_count(summary.performance_claim_eligible_count,
              "product profile performance claim eligible count");
  }
  summary.total_required_fields = inferlab::internal::checked_add_size(
      summary.total_required_fields, contract.required_field_count,
      "product profile total required field count");
  summary.total_present_fields = inferlab::internal::checked_add_size(
      summary.total_present_fields, contract.present_field_count,
      "product profile total present field count");
  summary.total_required_checks = inferlab::internal::checked_add_size(
      summary.total_required_checks, contract.required_check_count,
      "product profile total required check count");
  summary.total_satisfied_checks = inferlab::internal::checked_add_size(
      summary.total_satisfied_checks, contract.satisfied_check_count,
      "product profile total satisfied check count");
}

void reconcile(const AuditPlan& plan) {
  const auto& summary = plan.summary;
  const auto decision_total = inferlab::internal::checked_sum_size(
      {summary.passed_records, summary.failed_records,
       summary.pending_records, summary.not_selected_optional_records},
      "product profile record decision total");
  const auto scope_total = inferlab::internal::checked_add_size(
      summary.default_required_product_count,
      summary.optional_on_demand_product_count,
      "product profile scope total");
  const auto selected_total = inferlab::internal::checked_sum_size(
      {summary.passed_records, summary.failed_records,
       summary.pending_records},
      "selected product profile decision total");
  const auto presence_total = inferlab::internal::checked_add_size(
      summary.present_artifacts, summary.missing_artifacts,
      "product profile artifact presence total");
  if (summary.product_count == 0U ||
      summary.product_count != plan.records.size() ||
      scope_total != summary.product_count ||
      decision_total != summary.product_count ||
      selected_total != summary.selected_product_count ||
      summary.not_selected_optional_records >
          summary.optional_on_demand_product_count ||
      presence_total != summary.product_count ||
      summary.physical_artifact_read_count != summary.product_count ||
      summary.total_present_fields > summary.total_required_fields ||
      summary.total_satisfied_checks > summary.total_required_checks ||
      summary.readable_artifacts > summary.present_artifacts ||
      summary.parsed_artifacts > summary.readable_artifacts) {
    throw std::logic_error(
        "product profile contract check summary reconciliation failed");
  }
  if ((plan.decision == AuditDecision::failed) !=
          (summary.failed_records > 0U) ||
      (plan.decision == AuditDecision::pending) !=
          (summary.failed_records == 0U && summary.pending_records > 0U) ||
      (plan.decision == AuditDecision::ok) !=
          (summary.failed_records == 0U && summary.pending_records == 0U)) {
    throw std::logic_error(
        "product profile contract check decision reconciliation failed");
  }
}

}  // namespace

const ArtifactSpec& artifact_spec() {
  static const ArtifactSpec spec{
      "native_product_profile_contract_check",
      "inferlab_product_profile_contract_check",
      "results/raw/native_product_profile_contract_check.json",
      "reports/57_product_profile_contract_check.md",
      "./build/inferlab_product_profile_contract_check --repo-root . --results results/raw --output results/raw/native_product_profile_contract_check.json --format json",
      "static structural and semantic contract audit of existing product_profile.v1 artifacts; it does not execute products, collect metrics, prove performance, or approve release",
  };
  return spec;
}

const char* audit_status(const AuditDecision decision) {
  switch (decision) {
    case AuditDecision::ok:
      return "ok";
    case AuditDecision::pending:
      return "pending";
    case AuditDecision::failed:
      return "failed";
  }
  return "failed";
}

const char* record_status(const RecordDecision decision) {
  switch (decision) {
    case RecordDecision::pass:
      return "pass";
    case RecordDecision::pending_missing_artifact:
      return "pending_missing_artifact";
    case RecordDecision::not_selected_optional:
      return "not_selected_optional";
    case RecordDecision::fail:
      return "fail";
  }
  return "fail";
}

AuditPlan build_audit(const std::filesystem::path& results_root,
                      const bool strict_missing) {
  AuditPlan plan;
  plan.strict_missing = strict_missing;
  inferlab_evidence::EvidenceSnapshotRepository snapshots;
  const auto& profiles = mature_product_profiles::product_profiles();
  plan.records.reserve(profiles.size());
  for (const auto& profile : profiles) {
    AuditRecord record;
    record.resolved_path = results_root / profile.artifact;
    record.contract =
        mature_product_profiles::evaluate_product_profile_artifact_contract(
            profile, record.resolved_path, snapshots.get(record.resolved_path));
    record.decision = decide_record(record.contract, strict_missing);
    record.recommendation = recommendation(record.contract, record.decision);
    plan.records.push_back(std::move(record));
  }

  plan.summary.product_count = plan.records.size();
  plan.summary.physical_artifact_read_count = snapshots.size();
  for (const auto& record : plan.records) {
    aggregate_record(plan.summary, record);
  }
  if (plan.summary.total_required_fields == 0U ||
      plan.summary.total_required_checks == 0U) {
    throw std::logic_error(
        "product profile contract check has no required contract surface");
  }
  plan.summary.required_field_pass_rate =
      static_cast<double>(plan.summary.total_present_fields) /
      static_cast<double>(plan.summary.total_required_fields);
  plan.summary.contract_coverage =
      static_cast<double>(plan.summary.total_satisfied_checks) /
      static_cast<double>(plan.summary.total_required_checks);
  if (!std::isfinite(plan.summary.required_field_pass_rate) ||
      !std::isfinite(plan.summary.contract_coverage) ||
      plan.summary.required_field_pass_rate < 0.0 ||
      plan.summary.required_field_pass_rate > 1.0 ||
      plan.summary.contract_coverage < 0.0 ||
      plan.summary.contract_coverage > 1.0) {
    throw std::logic_error(
        "product profile contract check aggregate ratio is invalid");
  }
  plan.decision = plan.summary.failed_records > 0U
                      ? AuditDecision::failed
                      : plan.summary.pending_records > 0U
                            ? AuditDecision::pending
                            : AuditDecision::ok;
  reconcile(plan);
  return plan;
}

}  // namespace inferlab::product_profile_contract_check
