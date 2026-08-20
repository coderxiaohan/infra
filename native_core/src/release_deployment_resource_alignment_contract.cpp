#include "release_deployment_resource_alignment_contract.hpp"

#include "artifact_metadata_reader.hpp"
#include "artifact_profile_section.hpp"
#include "release_alignment_contract_utils.hpp"
#include "release_deployment_resource_profile.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_deployment_resource {
namespace {

struct BoundSource {
  const ArtifactSpec* spec = nullptr;
  const inferlab_evidence::EvidenceSnapshot* snapshot = nullptr;
};

void add_check(ArtifactAlignmentProjection& projection,
               const bool satisfied, const std::string& issue) {
  inferlab_release_alignment::add_check(
      projection, satisfied, issue, "release deployment resource artifact");
}

bool exact_expected_int(
    const std::string& section,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, const int expected, int& observed) {
  return inferlab_artifact_profile::exact_non_negative_int(
             section, index, key, observed) &&
         observed == expected;
}

ArtifactAlignmentProjection inspect_source(const BoundSource& source) {
  if (source.spec == nullptr || source.snapshot == nullptr) {
    throw std::logic_error(
        "release deployment resource source binding is invalid");
  }
  const ArtifactSpec& spec = *source.spec;
  ArtifactAlignmentProjection projection;
  projection.artifact = spec.artifact;
  projection.expected_artifact_type = spec.artifact_type;
  projection.observed_artifact_type = source.snapshot->metadata.artifact_type;
  const auto selected =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          source.snapshot->read_result.text,
          "release_deployment_resource_profile");
  projection.profile_section_present =
      inferlab_artifact::contains_top_level_key_fallback(
          source.snapshot->read_result.text,
          "release_deployment_resource_profile") &&
      selected.usable;
  add_check(projection,
            source.snapshot->metadata.parse_ok &&
                projection.observed_artifact_type == spec.artifact_type,
            projection.artifact + ":artifact_type_missing_or_mismatched");
  add_check(
      projection, projection.profile_section_present,
      projection.artifact +
          ":release_deployment_resource_profile_missing_or_wrong_type");

  const std::string section =
      projection.profile_section_present ? selected.text : std::string{};
  const auto index = inferlab_artifact::build_artifact_semantic_index(section);
  const Profile& expected = canonical_profile();

  std::string profile_id;
  const bool profile_id_ok = inferlab_artifact_profile::exact_string(
                                 section, index, "id", profile_id) &&
                             profile_id == expected.id;
  projection.profile_id = profile_id.empty() ? "missing" : profile_id;
  add_check(projection, profile_id_ok,
            projection.artifact +
                ":release_deployment_resource_profile_id_drift");

  std::string source_role;
  const bool source_role_ok = inferlab_artifact_profile::exact_string(
                                  section, index, "source_role", source_role) &&
                              source_role == spec.source_role;
  projection.source_role = source_role.empty() ? "missing" : source_role;
  add_check(projection, source_role_ok,
            projection.artifact +
                ":release_deployment_resource_source_role_drift");

  add_check(projection,
            exact_expected_int(section, index, "total_accelerator_count",
                               expected.total_accelerator_count,
                               projection.total_accelerator_count),
            projection.artifact + ":release_total_accelerator_count_drift");
  add_check(projection,
            exact_expected_int(section, index, "replica_count",
                               expected.replica_count,
                               projection.replica_count),
            projection.artifact + ":release_replica_count_drift");
  add_check(projection,
            exact_expected_int(section, index, "tensor_parallel_size",
                               expected.tensor_parallel_size,
                               projection.tensor_parallel_size),
            projection.artifact + ":release_tensor_parallel_size_drift");
  add_check(projection,
            exact_expected_int(section, index, "pipeline_parallel_size",
                               expected.pipeline_parallel_size,
                               projection.pipeline_parallel_size),
            projection.artifact + ":release_pipeline_parallel_size_drift");
  add_check(projection,
            exact_expected_int(section, index, "accelerators_per_replica",
                               expected.accelerators_per_replica,
                               projection.accelerators_per_replica),
            projection.artifact + ":release_accelerators_per_replica_drift");
  add_check(projection,
            exact_expected_int(section, index, "accelerators_per_node",
                               expected.accelerators_per_node,
                               projection.accelerators_per_node),
            projection.artifact + ":release_accelerators_per_node_drift");

  double memory_gib = 0.0;
  const bool memory_ok = inferlab_artifact_profile::exact_number(
                             section, index, "accelerator_memory_gib",
                             memory_gib) &&
                         memory_gib == expected.accelerator_memory_gib;
  projection.accelerator_memory_gib = memory_gib;
  add_check(projection, memory_ok,
            projection.artifact + ":release_accelerator_memory_drift");

  bool matched = false;
  const bool matched_ok = inferlab_artifact_profile::exact_bool(
                              section, index, "matched", matched) &&
                          matched;
  projection.matched = matched;
  add_check(projection, matched_ok,
            projection.artifact + ":release_resource_profile_not_matched");

  std::size_t match_count = 0U;
  const bool match_count_ok = inferlab_artifact_profile::exact_size(
                                  section, index, "match_count", match_count) &&
                              match_count == 1U;
  projection.match_count = match_count;
  add_check(projection, match_count_ok,
            projection.artifact + ":release_resource_match_count_invalid");

  const bool matched_source_ok = inferlab_artifact_profile::exact_string(
      section, index, "matched_source_identity",
      projection.matched_source_identity) &&
      projection.matched_source_identity == spec.expected_source_identity;
  add_check(projection, matched_source_ok,
            projection.artifact +
                ":release_resource_matched_source_identity_drift");

  bool resource_compatible = false;
  const bool resource_compatible_ok = inferlab_artifact_profile::exact_bool(
                                          section, index,
                                          "resource_compatible",
                                          resource_compatible) &&
                                      resource_compatible;
  projection.resource_compatible = resource_compatible;
  add_check(projection, resource_compatible_ok,
            projection.artifact + ":release_resources_not_compatible");

  const bool resource_status_ok = inferlab_artifact_profile::exact_string(
                                      section, index, "resource_status",
                                      projection.resource_status) &&
                                  projection.resource_status ==
                                      spec.expected_resource_status;
  add_check(projection, resource_status_ok,
            projection.artifact + ":release_resource_status_drift");
  add_check(projection,
            inferlab_artifact_profile::empty_issues_array(section),
            projection.artifact + ":release_resource_binding_has_issues");

  if (projection.required_check_count == 0U ||
      projection.satisfied_check_count > projection.required_check_count ||
      projection.issues.size() != projection.required_check_count -
                                      projection.satisfied_check_count) {
    throw std::logic_error(
        "release deployment resource artifact accounting is invalid");
  }
  projection.ready = projection.required_check_count ==
                     projection.satisfied_check_count;
  return projection;
}

bool same_profile(const ArtifactAlignmentProjection& lhs,
                  const ArtifactAlignmentProjection& rhs) {
  return lhs.profile_id == rhs.profile_id &&
         lhs.total_accelerator_count == rhs.total_accelerator_count &&
         lhs.replica_count == rhs.replica_count &&
         lhs.tensor_parallel_size == rhs.tensor_parallel_size &&
         lhs.pipeline_parallel_size == rhs.pipeline_parallel_size &&
         lhs.accelerators_per_replica == rhs.accelerators_per_replica &&
         lhs.accelerators_per_node == rhs.accelerators_per_node &&
         lhs.accelerator_memory_gib == rhs.accelerator_memory_gib;
}

}  // namespace

const std::vector<ArtifactSpec>& alignment_artifact_registry() {
  static const std::vector<ArtifactSpec> artifacts = {
      {"results/raw/kserve_llm_d_plan.json",
       "native_kserve_llm_d_plan", "kserve_llmd_manifest",
       "release_gpu8_tp4_pp1_manifest",
       "manifest_gpu_requests_reconciled"},
      {"results/raw/native_tensor_parallel_comm_plan.json",
       "native_tensor_parallel_comm_plan", "tensor_parallel_topology",
       "release_gpu8_tp4_pp1_comm",
       "parallel_group_inventory_reconciled"},
  };
  return artifacts;
}

void validate_alignment_artifact_registry() {
  const auto& artifacts = alignment_artifact_registry();
  inferlab_release_alignment::validate_artifact_registry(
      artifacts, 2U,
      [](const ArtifactSpec& artifact) {
        return !artifact.expected_source_identity.empty() &&
               !artifact.expected_resource_status.empty();
      },
      "release deployment resource alignment");
}

AlignmentProjection evaluate_alignment(
    const std::filesystem::path& repo_root,
    inferlab_evidence::EvidenceSnapshotRepository& repository) {
  validate_alignment_artifact_registry();
  const auto& specs = alignment_artifact_registry();
  return inferlab_release_alignment::evaluate_alignment<AlignmentProjection>(
      repo_root, repository, specs, canonical_profile().id,
      [](const ArtifactSpec& spec,
         const inferlab_evidence::EvidenceSnapshot& snapshot) {
        return inspect_source(BoundSource{&spec, &snapshot});
      },
      same_profile,
      "release_deployment_resource_profile_not_reconciled_across_all_artifacts",
      "release deployment resource alignment");
}

}  // namespace inferlab_release_deployment_resource
