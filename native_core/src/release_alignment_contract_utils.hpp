#pragma once

#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "evidence_release_policy.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace inferlab_release_alignment {

inline std::size_t checked_add_size_with_context(const std::size_t lhs,
                                                 const std::size_t rhs,
                                                 const std::string& name) {
  return inferlab::internal::checked_add_size(lhs, rhs, name.c_str());
}

template <typename ArtifactProjection>
void add_check(ArtifactProjection& projection, const bool satisfied,
               const std::string& issue, const std::string& context) {
  projection.required_check_count = checked_add_size_with_context(
      projection.required_check_count, 1U,
      context + " required check count");
  if (satisfied) {
    projection.satisfied_check_count = checked_add_size_with_context(
        projection.satisfied_check_count, 1U,
        context + " satisfied check count");
  } else {
    inferlab_plan::append_unique_nonempty_string(projection.issues, issue);
  }
}

template <typename ArtifactSpec, typename DomainValidator>
void validate_artifact_registry(
    const std::vector<ArtifactSpec>& artifacts, const std::size_t expected_size,
    DomainValidator domain_validator, const std::string& context) {
  if (artifacts.size() != expected_size) {
    throw std::logic_error(context + " artifact count is invalid");
  }
  std::unordered_set<std::string> paths;
  std::unordered_set<std::string> types;
  std::unordered_set<std::string> roles;
  for (const auto& artifact : artifacts) {
    if (artifact.artifact.empty() || artifact.artifact_type.empty() ||
        artifact.source_role.empty() || !domain_validator(artifact) ||
        !paths.insert(artifact.artifact).second ||
        !types.insert(artifact.artifact_type).second ||
        !roles.insert(artifact.source_role).second) {
      throw std::logic_error(context + " artifact registry is invalid");
    }
  }
}

template <typename AlignmentProjection, typename ArtifactSpec,
          typename Inspector, typename ProfileComparator>
AlignmentProjection evaluate_alignment(
    const std::filesystem::path& repo_root,
    inferlab_evidence::EvidenceSnapshotRepository& repository,
    const std::vector<ArtifactSpec>& specs, const std::string& profile_id,
    Inspector inspector, ProfileComparator same_profile,
    const std::string& cross_artifact_issue, const std::string& context) {
  AlignmentProjection projection;
  projection.profile_id = profile_id;
  projection.artifacts.reserve(specs.size());
  bool source_missing = false;

  for (const auto& spec : specs) {
    const auto& snapshot = repository.get(repo_root / spec.artifact);
    source_missing = source_missing || !snapshot.read_result.readable;
    auto artifact = inspector(spec, snapshot);
    projection.required_check_count = checked_add_size_with_context(
        projection.required_check_count, artifact.required_check_count,
        context + " required check count");
    projection.satisfied_check_count = checked_add_size_with_context(
        projection.satisfied_check_count, artifact.satisfied_check_count,
        context + " satisfied check count");
    for (const auto& issue : artifact.issues) {
      inferlab_plan::append_unique_nonempty_string(projection.issues, issue);
    }
    projection.artifacts.push_back(std::move(artifact));
  }

  projection.required_check_count = checked_add_size_with_context(
      projection.required_check_count, 1U,
      context + " cross-artifact check count");
  projection.cross_artifact_reconciled =
      projection.artifacts.size() == specs.size();
  for (std::size_t index = 0U;
       projection.cross_artifact_reconciled &&
       index < projection.artifacts.size(); ++index) {
    projection.cross_artifact_reconciled =
        projection.artifacts[index].ready &&
        (index == 0U || same_profile(projection.artifacts[0],
                                    projection.artifacts[index]));
  }
  if (projection.cross_artifact_reconciled) {
    projection.satisfied_check_count = checked_add_size_with_context(
        projection.satisfied_check_count, 1U,
        context + " cross-artifact satisfied check count");
  } else {
    inferlab_plan::append_unique_nonempty_string(projection.issues,
                                                 cross_artifact_issue);
  }

  if (projection.required_check_count == 0U ||
      projection.satisfied_check_count > projection.required_check_count ||
      projection.issues.size() != projection.required_check_count -
                                      projection.satisfied_check_count) {
    throw std::logic_error(context + " aggregate accounting is invalid");
  }
  projection.coverage =
      static_cast<double>(projection.satisfied_check_count) /
      static_cast<double>(projection.required_check_count);
  projection.gap = 1.0 - projection.coverage;
  if (!std::isfinite(projection.coverage) ||
      !std::isfinite(projection.gap) || projection.coverage < 0.0 ||
      projection.coverage > 1.0 || projection.gap < 0.0 ||
      projection.gap > 1.0) {
    throw std::logic_error(context + " score is invalid");
  }

  projection.ready = projection.cross_artifact_reconciled &&
                     projection.required_check_count ==
                         projection.satisfied_check_count;
  projection.status = projection.ready ? "verified" : "blocked";
  if (projection.ready) {
    projection.blocker_category = "none";
    projection.repair_environment = "none";
  } else {
    projection.blocker_category =
        source_missing ? "artifact_missing" : "artifact_blocked";
    projection.repair_environment = inferlab_evidence::repair_environment_name(
        source_missing
            ? inferlab_evidence::RepairEnvironment::
                  kCurrentServerWhenValidationEnabled
            : inferlab_evidence::RepairEnvironment::kCurrentRepoFixable);
  }
  return projection;
}

}  // namespace inferlab_release_alignment
