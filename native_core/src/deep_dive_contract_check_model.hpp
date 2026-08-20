#pragma once

#include "artifact_evidence_contract.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace inferlab_deep_dive_contract {

enum class TargetSource {
  Unspecified,
  CatalogManaged,
  MatureProductDispatch,
  DeepDiveWiringRegistry,
};

enum class RecordStatus {
  Pass,
  Warn,
  Fail,
  PendingMissingArtifact,
};

enum class Recommendation {
  ContractReadyForSummaryAndReports,
  AddMissingFieldsBeforeTargetMachineRelease,
  FixIncompleteContractBeforeRuntimeClaims,
  RemoveExecuteSemanticsBeforeClaims,
  PendingUntilArtifactIsGenerated,
  RefreshArtifactBeforeRelease,
  InvestigateArtifactPathProbe,
  RepairArtifactReadability,
  RepairMalformedArtifact,
};

enum class GateDecision {
  BlockClaimsUntilContractsAreFixed,
  PendingUntilArtifactsExist,
  DeepDiveContractsReady,
};

struct Policy {
  double warning_coverage_threshold = 0.72;
  std::size_t maximum_target_count = 256U;
  std::size_t maximum_required_field_count = 4096U;
  std::size_t maximum_required_metric_count = 8192U;
};

struct Target {
  std::string name;
  std::string artifact;
  std::string expected_kind;
  std::vector<std::string> required_fields;
  std::vector<std::string> required_metrics;
  std::vector<std::string> metric_owning_sections;
  std::string claim_boundary;
  std::string refresh_command;
  TargetSource source = TargetSource::Unspecified;
  bool canonical_wiring_complete = false;
  bool fallback_retained = true;
};

struct Record {
  Target target;
  std::filesystem::path resolved_path;
  inferlab_evidence::ArtifactContractProjection base_contract;
  bool present = false;
  bool executed_false = false;
  bool executed_true = false;
  bool replayed = false;
  bool claim_scope_present = false;
  bool records_present = false;
  bool parse_ok = false;
  std::string observed_artifact_type = "unknown";
  std::string observed_mode = "missing";
  std::string path_probe_status = "not_checked";
  std::string path_probe_error;
  std::string text_read_status = "not_attempted";
  std::string text_read_error;
  std::size_t required_checks = 0U;
  std::size_t passed_checks = 0U;
  double contract_score = 0.0;
  std::vector<std::string> missing_checks;
  std::vector<std::string> failed_forbidden_checks;
  RecordStatus status = RecordStatus::Fail;
  Recommendation recommendation =
      Recommendation::FixIncompleteContractBeforeRuntimeClaims;
};

struct Summary {
  std::size_t target_count = 0U;
  std::size_t catalog_managed_target_count = 0U;
  std::size_t mature_product_dispatch_target_count = 0U;
  std::size_t deep_dive_wiring_registry_target_count = 0U;
  // Deprecated output-compatibility counter. No target may increment it.
  std::size_t legacy_fallback_target_count = 0U;
  // Deprecated output-compatibility counter. Core targets are catalog-managed.
  std::size_t core_deep_dive_extension_target_count = 0U;
  std::size_t canonical_wiring_complete_target_count = 0U;
  std::size_t fallback_retained_target_count = 0U;
  std::size_t present_count = 0U;
  std::size_t pass_count = 0U;
  std::size_t warn_count = 0U;
  std::size_t fail_count = 0U;
  std::size_t pending_count = 0U;
  double contract_score = 0.0;
  GateDecision gate_decision = GateDecision::DeepDiveContractsReady;
};

struct Plan {
  Policy policy;
  bool strict_missing = false;
  std::vector<Record> records;
  Summary summary;
  std::vector<std::string> blockers;
  std::vector<std::string> recommended_next_commands;
};

const char* target_source_name(TargetSource source);
const char* record_status_name(RecordStatus status);
const char* recommendation_text(Recommendation recommendation);
const char* gate_decision_name(GateDecision decision);

Policy default_policy();
std::vector<Target> default_targets();
Plan build_plan(const Policy& policy,
                const std::filesystem::path& results_root,
                bool strict_missing);
void validate_plan(const Plan& plan);

}  // namespace inferlab_deep_dive_contract
