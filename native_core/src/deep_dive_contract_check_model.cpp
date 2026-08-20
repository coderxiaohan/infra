#include "deep_dive_contract_check_model.hpp"

#include "artifact_contract_model.hpp"
#include "artifact_metadata_reader.hpp"
#include "advanced_serving_internal.hpp"
#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "deep_dive_core_artifact_registry.hpp"
#include "deep_dive_evidence_bundle_fixture.hpp"
#include "evidence_release_policy.hpp"
#include "mature_product_artifact_dispatch.hpp"
#include "plan_json_utils.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_deep_dive_contract {
namespace {

namespace fs = std::filesystem;

constexpr std::size_t kExpectedMatureProductDispatchTargetCount = 19U;
constexpr std::size_t kExpectedDeepDiveWiringRegistryTargetCount = 10U;

const std::vector<std::string>& common_metric_owning_sections() {
  static const std::vector<std::string> sections = {
      "summary", "catalog_evidence_contract", "records[]"};
  return sections;
}

bool contains_required_metric(
    const std::vector<std::string>& required_metrics,
    const std::string& candidate) {
  return std::find(required_metrics.begin(), required_metrics.end(),
                   candidate) != required_metrics.end();
}

bool requires_native_trace_owner(
    const std::vector<std::string>& required_metrics) {
  for (const auto& metric :
       inferlab_deep_dive_core::native_trace_contract_metrics()) {
    if (contains_required_metric(required_metrics, metric)) return true;
  }
  return false;
}

void append_metric_owner(std::vector<std::string>& owners,
                         const std::string& owner) {
  inferlab_plan::append_unique_nonempty_string(owners, owner);
}

std::vector<std::string> metric_owning_sections_for(
    const std::string& target_name,
    const std::vector<std::string>& required_metrics) {
  std::vector<std::string> owners = common_metric_owning_sections();
  if (target_name == "advanced_kv_router") {
    append_metric_owner(owners, "decisions[]");
  }
  if (requires_native_trace_owner(required_metrics)) {
    append_metric_owner(owners, "native_trace_context");
  }
  if (target_name == "product_replay_semantic_events") {
    for (const auto* owner : {"normalized_events[]", "request_trace_join",
                              "event_schema_coverage",
                              "release_pipeline_linkage"}) {
      append_metric_owner(owners, owner);
    }
  }
  return owners;
}

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0U) == 0U;
}

bool ends_with(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) ==
             0;
}

std::string strip_prefix(std::string value, const std::string& prefix) {
  if (starts_with(value, prefix)) value.erase(0U, prefix.size());
  return value;
}

std::string artifact_filename(const std::string& path) {
  return fs::path(path).filename().generic_string();
}

Target catalog_target(const inferlab_catalog::ContractView& view) {
  constexpr char kCatalogPrefix[] = "catalog_";
  if (!starts_with(view.name, kCatalogPrefix)) {
    throw std::logic_error(
        "catalog contract view name does not use the canonical prefix: " +
        view.name);
  }
  Target target;
  target.name = strip_prefix(view.name, kCatalogPrefix);
  target.artifact = artifact_filename(view.artifact_path);
  target.expected_kind = view.evidence_level;
  target.required_fields = view.required_fields;
  target.required_metrics = view.required_metrics;
  target.required_fields.erase(
      std::remove_if(target.required_fields.begin(),
                     target.required_fields.end(),
                     [&](const std::string& field) {
                       return contains_required_metric(
                           target.required_metrics, field);
                     }),
      target.required_fields.end());
  target.metric_owning_sections =
      metric_owning_sections_for(target.name, target.required_metrics);
  target.claim_boundary = view.claim_boundary;
  target.refresh_command = view.refresh_command;
  target.source = TargetSource::CatalogManaged;
  target.canonical_wiring_complete = true;
  target.fallback_retained = false;
  return target;
}

Target mature_product_dispatch_target(
    const inferlab_artifact_contract::Contract& contract) {
  if (contract.source !=
      inferlab_artifact_contract::ContractSource::MatureProductDispatch) {
    throw std::logic_error(
        "mature-product target requires a dispatch-backed contract");
  }
  const auto* dispatch = inferlab_mature_product::find_exact(contract.name);
  if (dispatch == nullptr ||
      artifact_filename(contract.artifact) !=
          inferlab_mature_product::artifact_filename(*dispatch) ||
      contract.producer_target !=
          inferlab_mature_product::canonical_target(dispatch->family) ||
      contract.refresh_commands.size() != 1U ||
      contract.refresh_commands.front() !=
          inferlab_mature_product::refresh_command(dispatch->artifact_type)) {
    throw std::logic_error(
        "mature-product contract drifted from canonical dispatch: " +
        contract.name);
  }
  Target target;
  target.name = strip_prefix(contract.name, "native_");
  target.artifact = artifact_filename(contract.artifact);
  target.expected_kind = contract.evidence_level;
  target.required_fields = contract.required_fields;
  target.required_metrics = contract.required_metrics;
  target.metric_owning_sections =
      metric_owning_sections_for(target.name, target.required_metrics);
  target.claim_boundary = contract.claim_boundary;
  target.refresh_command = contract.refresh_commands.front();
  target.source = TargetSource::MatureProductDispatch;
  target.canonical_wiring_complete = true;
  target.fallback_retained = false;
  return target;
}

Target deep_dive_wiring_target(
    const inferlab_artifact_contract::Contract& contract) {
  if (contract.source !=
      inferlab_artifact_contract::ContractSource::DeepDiveWiringRegistry) {
    throw std::logic_error(
        "Deep-Dive wiring target requires a registry-backed contract");
  }
  const auto& wiring =
      inferlab_deep_dive_bundle::require_artifact_wiring_spec(contract.name);
  if (artifact_filename(contract.artifact) != wiring.artifact ||
      contract.producer_target != wiring.target ||
      contract.refresh_commands.size() != 1U ||
      contract.refresh_commands.front() != wiring.command) {
    throw std::logic_error(
        "Deep-Dive contract drifted from canonical wiring registry: " +
        contract.name);
  }
  Target target;
  target.name = strip_prefix(contract.name, "native_");
  target.artifact = wiring.artifact;
  target.expected_kind = contract.evidence_level;
  target.required_fields = contract.required_fields;
  target.required_metrics = contract.required_metrics;
  target.metric_owning_sections =
      metric_owning_sections_for(target.name, target.required_metrics);
  target.claim_boundary = contract.claim_boundary;
  target.refresh_command = wiring.command;
  target.source = TargetSource::DeepDiveWiringRegistry;
  target.canonical_wiring_complete = true;
  target.fallback_retained = false;
  return target;
}

void require_unique_nonempty(const std::vector<std::string>& values,
                             const char* label) {
  inferlab::advanced_serving_internal::require_unique_nonempty_strings(
      values, label);
}

void validate_target(const Target& target, const Policy& policy) {
  if (target.name.empty() || target.artifact.empty() ||
      target.expected_kind.empty() || target.claim_boundary.empty()) {
    throw std::invalid_argument(
        "deep-dive contract target identity fields must not be empty");
  }
  if (target.artifact != artifact_filename(target.artifact) ||
      !ends_with(target.artifact, ".json")) {
    throw std::invalid_argument(
        "deep-dive contract target must use a safe JSON filename: " +
        target.name);
  }
  if (target.required_fields.empty() || target.required_metrics.empty() ||
      target.required_fields.size() > policy.maximum_required_field_count ||
      target.required_metrics.size() > policy.maximum_required_metric_count) {
    throw std::invalid_argument(
        "deep-dive contract target field or metric contract is invalid: " +
        target.name);
  }
  require_unique_nonempty(target.required_fields,
                          "deep-dive contract required fields");
  require_unique_nonempty(target.required_metrics,
                          "deep-dive contract required metrics");
  require_unique_nonempty(target.metric_owning_sections,
                          "deep-dive contract metric owning sections");
  if ((target.source == TargetSource::CatalogManaged ||
       target.source == TargetSource::MatureProductDispatch ||
       target.source == TargetSource::DeepDiveWiringRegistry) &&
      (!target.canonical_wiring_complete || target.fallback_retained ||
       target.refresh_command.empty())) {
    throw std::logic_error(
        "canonical deep-dive target lost producer wiring: " +
        target.name);
  }
}

void validate_targets(const std::vector<Target>& targets,
                      const Policy& policy) {
  if (targets.empty() || targets.size() > policy.maximum_target_count) {
    throw std::invalid_argument(
        "deep-dive contract target inventory is empty or exceeds its bound");
  }
  for (std::size_t i = 0; i < targets.size(); ++i) {
    validate_target(targets[i], policy);
    for (std::size_t j = i + 1U; j < targets.size(); ++j) {
      if (targets[i].name == targets[j].name) {
        throw std::logic_error("duplicate deep-dive contract target name: " +
                               targets[i].name);
      }
      if (targets[i].artifact == targets[j].artifact) {
        throw std::logic_error(
            "duplicate deep-dive contract artifact filename: " +
            targets[i].artifact);
      }
    }
  }
}

void add_check(Record& record,
               const bool satisfied,
               const std::string& issue) {
  record.required_checks = inferlab::internal::checked_add_size(
      record.required_checks, 1U,
      "deep-dive contract required check count");
  if (satisfied) {
    record.passed_checks = inferlab::internal::checked_add_size(
        record.passed_checks, 1U,
        "deep-dive contract passed check count");
    return;
  }
  inferlab_plan::append_unique_nonempty_string(record.missing_checks, issue);
}

void add_base_contract(Record& record) {
  record.required_checks = inferlab::internal::checked_add_size(
      record.required_checks, record.base_contract.required_check_count,
      "deep-dive base contract required check count");
  record.passed_checks = inferlab::internal::checked_add_size(
      record.passed_checks, record.base_contract.satisfied_check_count,
      "deep-dive base contract passed check count");
  for (const auto& issue : record.base_contract.issues) {
    inferlab_plan::append_unique_nonempty_string(
        record.missing_checks, "base_contract:" + issue);
  }
}

bool base_contract_owned_field(const std::string& field) {
  static const std::vector<std::string> fields = {
      "schema_version", "schema",   "artifact_type", "status",
      "summary",        "claim_scope", "mode",       "executed",
      "replayed"};
  return std::find(fields.begin(), fields.end(), field) != fields.end();
}

bool record_collection_present(const std::string& document) {
  static const std::vector<std::string> collection_keys = {
      "records",      "artifacts",    "root_causes",
      "operators",    "dependencies", "correlations",
      "native_records", "results", "artifact_schemas", "checks",
      "fixtures", "repair_cards", "artifact_checks", "files",
      "pipeline_stages", "included_artifacts", "fixture_results"};
  for (const auto& key : collection_keys) {
    if (!inferlab_artifact::json_array_section_or_empty(document, key)
             .empty()) {
      return true;
    }
  }
  return false;
}

bool required_metric_present(
    const std::string& document,
    const std::string& metric,
    const std::vector<std::string>& owning_sections) {
  if (inferlab_artifact::contains_top_level_key_fallback(document, metric)) {
    return true;
  }
  for (const auto& owning_section : owning_sections) {
    const bool array_owner = ends_with(owning_section, "[]");
    const std::string section_name =
        array_owner
            ? owning_section.substr(0U, owning_section.size() - 2U)
            : owning_section;
    const std::string section =
        array_owner
            ? inferlab_artifact::json_array_section_or_empty(document,
                                                              section_name)
            : inferlab_artifact::json_object_section_or_empty(document,
                                                               section_name);
    if (!section.empty() && inferlab_artifact::contains_key(section, metric)) {
      return true;
    }
  }
  return false;
}

double coverage(const Record& record) {
  if (record.required_checks == 0U ||
      record.passed_checks > record.required_checks) {
    throw std::logic_error(
        "deep-dive contract check accounting is invalid");
  }
  const double value = static_cast<double>(record.passed_checks) /
                       static_cast<double>(record.required_checks);
  if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
    throw std::logic_error(
        "deep-dive contract coverage is not finite or bounded");
  }
  return value;
}

void finalize_record(Record& record,
                     const Policy& policy,
                     const bool strict_missing) {
  record.contract_score = coverage(record);
  if (!record.present) {
    if (record.path_probe_status == "error") {
      record.status = RecordStatus::Fail;
      record.recommendation = Recommendation::InvestigateArtifactPathProbe;
    } else if (strict_missing) {
      record.status = RecordStatus::Fail;
      record.recommendation = Recommendation::RefreshArtifactBeforeRelease;
    } else {
      record.status = RecordStatus::PendingMissingArtifact;
      record.recommendation =
          Recommendation::PendingUntilArtifactIsGenerated;
    }
    return;
  }
  if (record.text_read_status == "unreadable") {
    record.status = RecordStatus::Fail;
    record.recommendation = Recommendation::RepairArtifactReadability;
    return;
  }
  if (!record.parse_ok) {
    record.status = RecordStatus::Fail;
    record.recommendation = Recommendation::RepairMalformedArtifact;
    return;
  }
  if (record.executed_true || !record.failed_forbidden_checks.empty()) {
    record.status = RecordStatus::Fail;
    record.recommendation =
        Recommendation::RemoveExecuteSemanticsBeforeClaims;
    return;
  }
  if (record.passed_checks == record.required_checks) {
    record.status = RecordStatus::Pass;
    record.recommendation =
        Recommendation::ContractReadyForSummaryAndReports;
    return;
  }
  if (record.contract_score >= policy.warning_coverage_threshold) {
    record.status = RecordStatus::Warn;
    record.recommendation =
        Recommendation::AddMissingFieldsBeforeTargetMachineRelease;
    return;
  }
  record.status = RecordStatus::Fail;
  record.recommendation =
      Recommendation::FixIncompleteContractBeforeRuntimeClaims;
}

Record analyze_target(const Target& target,
                      const fs::path& results_root,
                      const bool strict_missing,
                      inferlab_evidence::EvidenceSnapshotRepository&
                          snapshots,
                      const Policy& policy) {
  Record record;
  record.target = target;
  record.resolved_path = results_root / target.artifact;
  const auto& snapshot = snapshots.get(record.resolved_path);
  const auto& read = snapshot.read_result;
  const auto& metadata = snapshot.metadata;

  record.present = read.present;
  record.path_probe_status =
      read.status == "unreadable_file_probe"
          ? "error"
          : (read.present ? "present" : "missing");
  record.path_probe_error =
      read.status == "unreadable_file_probe" ? read.error : "";
  record.text_read_status = read.present ? read.status : "not_attempted";
  record.text_read_error =
      read.status == "unreadable_file_probe" ? "" : read.error;
  record.parse_ok = metadata.parse_ok;
  record.executed_false = metadata.executed_present && !metadata.executed;
  record.executed_true = metadata.executed ||
                         inferlab_artifact::json_mode_matches(metadata.mode,
                                                              "execute");
  record.replayed = metadata.replayed;
  record.claim_scope_present = metadata.claim_scope_present &&
                               !metadata.claim_scope.empty();
  record.observed_artifact_type = metadata.artifact_type;
  record.observed_mode = metadata.mode;

  inferlab_evidence::ArtifactContractPolicy base_policy;
  base_policy.require_status_ok = false;
  base_policy.require_clear_blockers = false;
  record.base_contract =
      inferlab_evidence::evaluate_artifact_contract(snapshot, base_policy);
  add_base_contract(record);

  if (!record.present || !read.readable || !record.parse_ok) {
    finalize_record(record, policy, strict_missing);
    return record;
  }

  const std::string& document = read.text;
  if (std::find(target.required_fields.begin(), target.required_fields.end(),
                "status") != target.required_fields.end()) {
    std::string structural_status;
    add_check(record,
              inferlab_artifact::extract_top_level_string_key_fallback(
                  document, "status", structural_status) &&
                  inferlab_plan::has_non_whitespace(structural_status),
              "status_missing_empty_or_wrong_type");
  }
  for (const auto& field : target.required_fields) {
    if (base_contract_owned_field(field)) continue;
    add_check(record,
              inferlab_artifact::contains_top_level_key_fallback(document,
                                                                  field),
              "field:" + field);
  }
  for (const auto& metric : target.required_metrics) {
    add_check(record,
              required_metric_present(document, metric,
                                      target.metric_owning_sections),
              "metric:" + metric);
  }
  record.records_present = record_collection_present(document);
  add_check(record, record.records_present,
            "recognized_record_collection_missing_or_wrong_type");
  add_check(record, record.executed_false, "executed_false");
  add_check(record, !record.executed_true, "no_execute_mode");
  if (metadata.executed_present && metadata.executed) {
    record.failed_forbidden_checks.push_back("\"executed\": true");
  }
  if (inferlab_artifact::json_mode_matches(metadata.mode, "execute")) {
    record.failed_forbidden_checks.push_back("\"mode\": \"execute\"");
  }

  finalize_record(record, policy, strict_missing);
  return record;
}

void increment(std::size_t& value, const char* label) {
  value = inferlab::internal::checked_add_size(value, 1U, label);
}

void add_record_to_summary(const Record& record,
                           Summary& summary,
                           double& score_sum) {
  switch (record.target.source) {
    case TargetSource::Unspecified:
      throw std::logic_error(
          "unspecified deep-dive target source reached aggregation");
    case TargetSource::CatalogManaged:
      increment(summary.catalog_managed_target_count,
                "catalog-managed target count");
      break;
    case TargetSource::MatureProductDispatch:
      increment(summary.mature_product_dispatch_target_count,
                "mature-product dispatch target count");
      break;
    case TargetSource::DeepDiveWiringRegistry:
      increment(summary.deep_dive_wiring_registry_target_count,
                "Deep-Dive wiring registry target count");
      break;
  }
  if (record.target.canonical_wiring_complete) {
    increment(summary.canonical_wiring_complete_target_count,
              "canonical wiring complete target count");
  }
  if (record.target.fallback_retained) {
    increment(summary.fallback_retained_target_count,
              "fallback retained target count");
  }
  if (record.present) increment(summary.present_count, "present target count");
  switch (record.status) {
    case RecordStatus::Pass:
      increment(summary.pass_count, "passing target count");
      break;
    case RecordStatus::Warn:
      increment(summary.warn_count, "warning target count");
      break;
    case RecordStatus::Fail:
      increment(summary.fail_count, "failing target count");
      break;
    case RecordStatus::PendingMissingArtifact:
      increment(summary.pending_count, "pending target count");
      break;
  }
  score_sum = inferlab::internal::checked_add_finite_double(
      score_sum, record.contract_score,
      "deep-dive contract aggregate coverage");
}

bool action_required(const RecordStatus status) {
  return status != RecordStatus::Pass;
}

}  // namespace

const char* target_source_name(const TargetSource source) {
  switch (source) {
    case TargetSource::Unspecified:
      throw std::invalid_argument(
          "unspecified deep-dive contract target source");
    case TargetSource::CatalogManaged:
      return "catalog_managed";
    case TargetSource::MatureProductDispatch:
      return "mature_product_dispatch";
    case TargetSource::DeepDiveWiringRegistry:
      return "deep_dive_wiring_registry";
  }
  throw std::invalid_argument("unknown deep-dive contract target source");
}

const char* record_status_name(const RecordStatus status) {
  switch (status) {
    case RecordStatus::Pass:
      return "pass";
    case RecordStatus::Warn:
      return "warn";
    case RecordStatus::Fail:
      return "fail";
    case RecordStatus::PendingMissingArtifact:
      return "pending_missing_artifact";
  }
  throw std::invalid_argument("unknown deep-dive contract record status");
}

const char* recommendation_text(const Recommendation recommendation) {
  switch (recommendation) {
    case Recommendation::ContractReadyForSummaryAndReports:
      return "deep_dive_contract_ready_for_summary_and_reports";
    case Recommendation::AddMissingFieldsBeforeTargetMachineRelease:
      return "add_missing_deep_dive_fields_before_target_machine_release";
    case Recommendation::FixIncompleteContractBeforeRuntimeClaims:
      return "deep_dive_artifact_contract_incomplete_fix_before_runtime_claims";
    case Recommendation::RemoveExecuteSemanticsBeforeClaims:
      return "remove_execute_semantics_from_deep_dive_artifact_before_claims";
    case Recommendation::PendingUntilArtifactIsGenerated:
      return "pending_until_deep_dive_artifact_is_generated";
    case Recommendation::RefreshArtifactBeforeRelease:
      return "refresh_required_deep_dive_artifact_before_release";
    case Recommendation::InvestigateArtifactPathProbe:
      return "investigate_artifact_path_probe_error_before_contract_check";
    case Recommendation::RepairArtifactReadability:
      return "repair_artifact_readability_or_regenerate_before_contract_check";
    case Recommendation::RepairMalformedArtifact:
      return "repair_malformed_or_empty_artifact_before_contract_check";
  }
  throw std::invalid_argument("unknown deep-dive contract recommendation");
}

const char* gate_decision_name(const GateDecision decision) {
  switch (decision) {
    case GateDecision::BlockClaimsUntilContractsAreFixed:
      return "block_claims_until_deep_dive_contracts_are_fixed";
    case GateDecision::PendingUntilArtifactsExist:
      return "pending_until_deep_dive_artifacts_exist";
    case GateDecision::DeepDiveContractsReady:
      return "deep_dive_contracts_ready";
  }
  throw std::invalid_argument("unknown deep-dive contract gate decision");
}

Policy default_policy() { return {}; }

std::vector<Target> default_targets() {
  const Policy policy = default_policy();
  std::vector<Target> targets;

  const auto catalog = inferlab_catalog::contract_views();
  targets.reserve(catalog.size() +
                  kExpectedMatureProductDispatchTargetCount +
                  kExpectedDeepDiveWiringRegistryTargetCount);
  for (const auto& view : catalog) targets.push_back(catalog_target(view));

  std::size_t mature_product_dispatch_count = 0U;
  std::size_t deep_dive_wiring_registry_count = 0U;
  for (const auto& contract :
       inferlab_artifact_contract::contract_extension_registry()) {
    if (contract.source ==
        inferlab_artifact_contract::ContractSource::MatureProductDispatch) {
      targets.push_back(mature_product_dispatch_target(contract));
      mature_product_dispatch_count = inferlab::internal::checked_add_size(
          mature_product_dispatch_count, 1U,
          "selected mature-product dispatch contract count");
      continue;
    }
    if (contract.source == inferlab_artifact_contract::ContractSource::
                               DeepDiveWiringRegistry) {
      const std::string filename = artifact_filename(contract.artifact);
      if (filename != "native_deep_dive_contract_check.json") {
        targets.push_back(deep_dive_wiring_target(contract));
        deep_dive_wiring_registry_count =
            inferlab::internal::checked_add_size(
                deep_dive_wiring_registry_count, 1U,
                "selected Deep-Dive wiring registry contract count");
      }
      continue;
    }
  }
  if (mature_product_dispatch_count !=
      kExpectedMatureProductDispatchTargetCount) {
    throw std::logic_error(
        "selected mature-product dispatch inventory count drift");
  }
  if (deep_dive_wiring_registry_count !=
      kExpectedDeepDiveWiringRegistryTargetCount) {
    throw std::logic_error(
        "selected Deep-Dive wiring registry inventory count drift");
  }
  validate_targets(targets, policy);
  return targets;
}

Plan build_plan(const Policy& policy,
                const fs::path& results_root,
                const bool strict_missing) {
  if (results_root.empty() || !std::isfinite(policy.warning_coverage_threshold) ||
      policy.warning_coverage_threshold < 0.0 ||
      policy.warning_coverage_threshold > 1.0 ||
      policy.maximum_target_count == 0U ||
      policy.maximum_required_field_count == 0U ||
      policy.maximum_required_metric_count == 0U) {
    throw std::invalid_argument(
        "deep-dive contract policy and results root must be valid");
  }

  Plan plan;
  plan.policy = policy;
  plan.strict_missing = strict_missing;
  const auto targets = default_targets();
  validate_targets(targets, policy);
  plan.records.reserve(targets.size());

  inferlab_evidence::EvidenceSnapshotRepository snapshots;
  double score_sum = 0.0;
  for (const auto& target : targets) {
    plan.records.push_back(
        analyze_target(target, results_root, strict_missing, snapshots, policy));
    const auto& record = plan.records.back();
    add_record_to_summary(record, plan.summary, score_sum);
    if (record.status == RecordStatus::Fail ||
        record.status == RecordStatus::PendingMissingArtifact) {
      inferlab_plan::append_unique_nonempty_string(
          plan.blockers,
          record.target.name + ":" + record_status_name(record.status));
    }
    if (action_required(record.status) &&
        !record.target.refresh_command.empty()) {
      inferlab_plan::append_unique_nonempty_string(
          plan.recommended_next_commands, record.target.refresh_command);
    }
  }

  plan.summary.target_count = plan.records.size();
  plan.summary.contract_score =
      plan.records.empty()
          ? 0.0
          : score_sum / static_cast<double>(plan.records.size());
  if (!std::isfinite(plan.summary.contract_score) ||
      plan.summary.contract_score < 0.0 ||
      plan.summary.contract_score > 1.0) {
    throw std::logic_error(
        "deep-dive contract summary coverage is not finite or bounded");
  }
  plan.summary.gate_decision =
      plan.summary.fail_count > 0U
          ? GateDecision::BlockClaimsUntilContractsAreFixed
          : (plan.summary.pending_count > 0U
                 ? GateDecision::PendingUntilArtifactsExist
                 : GateDecision::DeepDiveContractsReady);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  if (plan.records.empty() ||
      plan.summary.target_count != plan.records.size()) {
    throw std::logic_error(
        "deep-dive contract plan record count reconciliation failed");
  }
  const std::size_t source_total = inferlab::internal::checked_sum_size(
      {plan.summary.catalog_managed_target_count,
       plan.summary.mature_product_dispatch_target_count,
       plan.summary.deep_dive_wiring_registry_target_count},
      "deep-dive contract source count reconciliation");
  const std::size_t status_total = inferlab::internal::checked_sum_size(
      {plan.summary.pass_count, plan.summary.warn_count,
       plan.summary.fail_count, plan.summary.pending_count},
      "deep-dive contract status count reconciliation");
  if (source_total != plan.records.size() ||
      status_total != plan.records.size() ||
      plan.summary.present_count > plan.records.size() ||
      plan.summary.canonical_wiring_complete_target_count >
          plan.records.size() ||
      plan.summary.fallback_retained_target_count > plan.records.size() ||
      plan.summary.legacy_fallback_target_count != 0U ||
      plan.summary.core_deep_dive_extension_target_count != 0U ||
      !std::isfinite(plan.summary.contract_score) ||
      plan.summary.contract_score < 0.0 ||
      plan.summary.contract_score > 1.0) {
    throw std::logic_error(
        "deep-dive contract plan summary reconciliation failed");
  }
  for (const auto& record : plan.records) {
    if (record.required_checks == 0U ||
        record.passed_checks > record.required_checks ||
        !std::isfinite(record.contract_score) ||
        record.contract_score < 0.0 || record.contract_score > 1.0) {
      throw std::logic_error(
          "deep-dive contract record reconciliation failed: " +
          record.target.name);
    }
  }
}

}  // namespace inferlab_deep_dive_contract
