#include "dependency_governance_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::dependency_governance {
namespace {

constexpr const char* kProductProfileControl = "product profile config";

void checked_increment(std::size_t& value, const char* name) {
  value = internal::checked_add_size(value, 1U, name);
}

DependencyRisk parse_risk(const std::string& value) {
  if (value == "low") return DependencyRisk::low;
  if (value == "medium") return DependencyRisk::medium;
  if (value == "high") return DependencyRisk::high;
  throw std::logic_error("unknown dependency risk level: " + value);
}

LicenseReviewStatus license_review_status(const std::string& license) {
  if (license.empty()) return LicenseReviewStatus::metadata_missing;
  if (license == "MIT" || license == "Apache-2.0" ||
      license == "Public Domain") {
    return LicenseReviewStatus::recorded_permissive;
  }
  return LicenseReviewStatus::manual_review_required;
}

bool used_by_metadata_complete(const std::vector<std::string>& used_by) {
  if (used_by.empty()) return false;
  for (const auto& value : used_by) {
    if (value.empty()) return false;
  }
  return true;
}

void append_gap_if_empty(std::vector<std::string>& gaps,
                         const std::string& value,
                         const std::string& field) {
  if (value.empty()) {
    inferlab_plan::append_unique_string(gaps, field);
  }
}

DependencyRecord build_record(
    const inferlab_high_value::DependencySpec& dependency) {
  DependencyRecord record;
  record.dependency = dependency;
  record.risk = parse_risk(dependency.risk_level);
  record.kind = dependency.cmake_option == kProductProfileControl
                    ? DependencyKind::product_runtime
                    : DependencyKind::build_or_tooling;
  record.license_review_status = license_review_status(dependency.license);

  append_gap_if_empty(record.metadata_gaps, dependency.pinned_version,
                      "version_record");
  append_gap_if_empty(record.metadata_gaps, dependency.source, "source");
  append_gap_if_empty(record.metadata_gaps, dependency.license, "license");
  append_gap_if_empty(record.metadata_gaps, dependency.cmake_option,
                      "cmake_option_or_profile_control");
  if (!used_by_metadata_complete(dependency.used_by)) {
    inferlab_plan::append_unique_string(record.metadata_gaps, "used_by");
  }
  append_gap_if_empty(record.metadata_gaps, dependency.engineering_value,
                      "engineering_value");
  append_gap_if_empty(record.metadata_gaps, dependency.fallback, "fallback");
  append_gap_if_empty(record.metadata_gaps, dependency.claim_impact,
                      "claim_impact");
  append_gap_if_empty(record.metadata_gaps, dependency.current_server_action,
                      "current_server_action");

  record.metadata_complete = record.metadata_gaps.empty();
  record.fallback_ready = !dependency.fallback.empty();
  record.fallback_blocks_core_claim =
      dependency.required && !record.fallback_ready;
  record.dry_run_claim_allowed = !dependency.engineering_value.empty();
  return record;
}

void validate_artifact_contract(
    const inferlab_high_value::ManagedArtifact& artifact,
    const std::string& expected_id) {
  if (artifact.id != expected_id || artifact.artifact.empty() ||
      artifact.target.empty() || artifact.command.empty() ||
      artifact.claim_boundary.empty()) {
    throw std::logic_error(
        "dependency governance managed artifact contract is incomplete: " +
        expected_id);
  }
}

std::vector<std::string> validation_commands(
    const inferlab_high_value::ManagedArtifact& policy_artifact,
    const inferlab_high_value::ManagedArtifact& sbom_artifact) {
  std::vector<std::string> commands;
  for (const auto& command : std::vector<std::string>{
           "cmake -B build -DINFERLAB_ENABLE_THIRD_PARTY_JSON=ON",
           "cmake -B build -DINFERLAB_ENABLE_EVIDENCE_SQLITE=ON",
           "cmake -B build -DINFERLAB_ENABLE_GOOGLE_BENCHMARK=ON",
           "cmake -B build -DINFERLAB_ENABLE_CUDA=ON -DINFERLAB_ENABLE_NVTX=ON",
           policy_artifact.command,
           sbom_artifact.command}) {
    inferlab_plan::append_unique_string(commands, command);
  }
  if (commands.size() != 6U) {
    throw std::logic_error(
        "dependency governance validation commands must be nonempty and unique");
  }
  return commands;
}

}  // namespace

const char* dependency_risk_name(const DependencyRisk risk) {
  switch (risk) {
    case DependencyRisk::low:
      return "low";
    case DependencyRisk::medium:
      return "medium";
    case DependencyRisk::high:
      return "high";
  }
  throw std::logic_error("unknown dependency risk enum");
}

const char* dependency_kind_name(const DependencyKind kind) {
  switch (kind) {
    case DependencyKind::build_or_tooling:
      return "build_or_tooling_dependency";
    case DependencyKind::product_runtime:
      return "product_runtime_dependency";
  }
  throw std::logic_error("unknown dependency kind enum");
}

const char* license_review_status_name(const LicenseReviewStatus status) {
  switch (status) {
    case LicenseReviewStatus::recorded_permissive:
      return "recorded_permissive";
    case LicenseReviewStatus::manual_review_required:
      return "manual_review_required";
    case LicenseReviewStatus::metadata_missing:
      return "metadata_missing";
  }
  throw std::logic_error("unknown dependency license review enum");
}

const char* governance_decision_name(const GovernanceDecision decision) {
  switch (decision) {
    case GovernanceDecision::documented:
      return "documented";
    case GovernanceDecision::blocked:
      return "blocked";
  }
  throw std::logic_error("unknown dependency governance decision enum");
}

Plan build_plan(
    const std::vector<inferlab_high_value::DependencySpec>& dependencies,
    const inferlab_high_value::ManagedArtifact& policy_artifact,
    const inferlab_high_value::ManagedArtifact& sbom_artifact) {
  inferlab_high_value::validate_dependency_spec_registry(dependencies);
  validate_artifact_contract(policy_artifact,
                             "third_party_dependency_gate");
  validate_artifact_contract(sbom_artifact, "dependency_sbom_gate");

  Plan plan;
  plan.summary.dependency_count = dependencies.size();
  plan.records.reserve(dependencies.size());
  plan.dependency_names.reserve(dependencies.size());
  plan.product_runtime_names.reserve(dependencies.size());
  std::map<std::string, std::size_t> license_counts;

  for (const auto& dependency : dependencies) {
    auto record = build_record(dependency);
    if (record.dependency.required) {
      checked_increment(plan.summary.required_dependency_count,
                        "required dependency count");
    }
    if (record.dependency.optional) {
      checked_increment(plan.summary.optional_dependency_count,
                        "optional dependency count");
    }
    if (record.kind == DependencyKind::product_runtime) {
      checked_increment(plan.summary.product_runtime_dependency_count,
                        "product runtime dependency count");
      plan.product_runtime_names.push_back(record.dependency.name);
    }
    if (record.risk == DependencyRisk::high) {
      checked_increment(plan.summary.high_risk_dependency_count,
                        "high-risk dependency count");
    }
    if (record.fallback_ready) {
      checked_increment(plan.summary.fallback_ready_count,
                        "fallback-ready dependency count");
    }
    if (record.metadata_complete) {
      checked_increment(plan.summary.metadata_complete_dependency_count,
                        "metadata-complete dependency count");
    } else {
      plan.blocker_record_indices.push_back(plan.records.size());
    }
    if (record.license_review_status ==
        LicenseReviewStatus::recorded_permissive) {
      checked_increment(plan.summary.permissive_license_count,
                        "permissive license count");
    } else {
      checked_increment(plan.summary.manual_license_review_count,
                        "manual license review count");
    }

    const std::string license = record.dependency.license.empty()
                                    ? "unrecorded"
                                    : record.dependency.license;
    license_counts[license] = internal::checked_add_size(
        license_counts[license], 1U, "dependency license group count");
    plan.dependency_names.push_back(record.dependency.name);
    plan.records.push_back(std::move(record));
  }

  plan.summary.blocker_count = plan.blocker_record_indices.size();
  const bool all_metadata_complete =
      plan.summary.metadata_complete_dependency_count ==
      plan.summary.dependency_count;
  const bool all_fallbacks_recorded =
      plan.summary.fallback_ready_count == plan.summary.dependency_count;
  plan.summary.decision = all_metadata_complete && all_fallbacks_recorded
                              ? GovernanceDecision::documented
                              : GovernanceDecision::blocked;

  plan.license_groups.reserve(license_counts.size());
  for (const auto& [license, count] : license_counts) {
    plan.license_groups.push_back({license, count});
  }
  plan.target_machine_validation_commands =
      validation_commands(policy_artifact, sbom_artifact);
  return plan;
}

}  // namespace inferlab::dependency_governance
