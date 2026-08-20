#include "release_evidence_pipeline_alignment_projection.hpp"

#include "checked_index.hpp"
#include "release_deployment_resource_alignment_contract.hpp"
#include "release_kv_budget_alignment_contract.hpp"
#include "release_lora_prefix_isolation_alignment_contract.hpp"
#include "release_traffic_alignment_contract.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace inferlab_release_pipeline {
namespace {

template <typename Projection>
ReleasePipelineAlignmentDimension dimension_from(
    const std::string& id, const bool release_blocking,
    const Projection& projection) {
  ReleasePipelineAlignmentDimension dimension;
  dimension.id = id;
  dimension.profile_id = projection.profile_id;
  dimension.status = projection.status;
  dimension.blocker_category = projection.blocker_category;
  dimension.repair_environment = projection.repair_environment;
  dimension.release_blocking = release_blocking;
  dimension.ready = projection.ready;
  dimension.cross_artifact_reconciled =
      projection.cross_artifact_reconciled;
  dimension.required_check_count = projection.required_check_count;
  dimension.satisfied_check_count = projection.satisfied_check_count;
  dimension.issues = projection.issues;
  return dimension;
}

void validate_dimension(const ReleasePipelineAlignmentDimension& dimension) {
  if (dimension.id.empty() || dimension.profile_id.empty() ||
      dimension.status.empty() || dimension.blocker_category.empty() ||
      dimension.repair_environment.empty() ||
      dimension.required_check_count == 0U ||
      dimension.satisfied_check_count > dimension.required_check_count ||
      dimension.issues.size() != dimension.required_check_count -
                                     dimension.satisfied_check_count ||
      dimension.ready !=
          (dimension.cross_artifact_reconciled && dimension.issues.empty()) ||
      (dimension.ready &&
       (dimension.status != "verified" ||
        dimension.blocker_category != "none" ||
        dimension.repair_environment != "none"))) {
    throw std::logic_error(
        "release pipeline alignment dimension contract is invalid: " +
        dimension.id);
  }
}

}  // namespace

ReleasePipelineAlignmentProjection::ReleasePipelineAlignmentProjection(
    const std::filesystem::path& repo_root,
    inferlab_evidence::EvidenceSnapshotRepository& snapshots) {
  dimensions = {
      dimension_from(
          "traffic_capacity_alignment", false,
          inferlab_release_traffic::evaluate_alignment(repo_root, snapshots)),
      dimension_from(
          "kv_memory_and_context_budget", true,
          inferlab_release_kv_budget::evaluate_alignment(repo_root,
                                                          snapshots)),
      dimension_from(
          "lora_prefix_isolation", false,
          inferlab_release_lora_prefix_isolation::evaluate_alignment(
              repo_root, snapshots)),
      dimension_from(
          "deployment_resource_contract", false,
          inferlab_release_deployment_resource::evaluate_alignment(
              repo_root, snapshots)),
  };

  std::map<std::string, bool> ids;
  for (const auto& dimension : dimensions) {
    validate_dimension(dimension);
    if (!ids.emplace(dimension.id, true).second) {
      throw std::logic_error(
          "release pipeline alignment dimension identity is duplicated: " +
          dimension.id);
    }
    required_check_count = inferlab::internal::checked_add_size(
        required_check_count, dimension.required_check_count,
        "release pipeline alignment required check count");
    satisfied_check_count = inferlab::internal::checked_add_size(
        satisfied_check_count, dimension.satisfied_check_count,
        "release pipeline alignment satisfied check count");
    issue_count = inferlab::internal::checked_add_size(
        issue_count, dimension.issues.size(),
        "release pipeline alignment issue count");
    if (dimension.ready) {
      ready_dimension_count = inferlab::internal::checked_add_size(
          ready_dimension_count, 1U,
          "release pipeline alignment ready dimension count");
    } else if (dimension.release_blocking) {
      blocking_dimension_count = inferlab::internal::checked_add_size(
          blocking_dimension_count, 1U,
          "release pipeline alignment blocking dimension count");
    } else {
      advisory_dimension_count = inferlab::internal::checked_add_size(
          advisory_dimension_count, 1U,
          "release pipeline alignment advisory dimension count");
    }
  }
  if (dimensions.empty() ||
      inferlab::internal::checked_add_size(
          ready_dimension_count,
          inferlab::internal::checked_add_size(
              blocking_dimension_count, advisory_dimension_count,
              "release pipeline alignment non-ready dimension count"),
          "release pipeline alignment classified dimension count") !=
          dimensions.size() ||
      satisfied_check_count > required_check_count ||
      issue_count != required_check_count - satisfied_check_count) {
    throw std::logic_error(
        "release pipeline alignment projection does not reconcile");
  }
}

}  // namespace inferlab_release_pipeline
