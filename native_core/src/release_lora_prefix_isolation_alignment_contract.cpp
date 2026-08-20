#include "release_lora_prefix_isolation_alignment_contract.hpp"

#include "artifact_metadata_reader.hpp"
#include "artifact_profile_section.hpp"
#include "release_alignment_contract_utils.hpp"
#include "release_lora_prefix_isolation_profile.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_lora_prefix_isolation {
namespace {

struct BoundSource {
  const ArtifactSpec* spec = nullptr;
  const inferlab_evidence::EvidenceSnapshot* snapshot = nullptr;
};

void add_check(ArtifactAlignmentProjection& projection,
               const bool satisfied, const std::string& issue) {
  inferlab_release_alignment::add_check(
      projection, satisfied, issue, "release LoRA/prefix isolation");
}

ArtifactAlignmentProjection inspect_source(const BoundSource& source) {
  if (source.spec == nullptr || source.snapshot == nullptr) {
    throw std::logic_error(
        "release LoRA/prefix isolation source binding is invalid");
  }
  const ArtifactSpec& spec = *source.spec;
  ArtifactAlignmentProjection projection;
  projection.artifact = spec.artifact;
  projection.expected_artifact_type = spec.artifact_type;
  projection.observed_artifact_type = source.snapshot->metadata.artifact_type;
  const auto selected =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          source.snapshot->read_result.text,
          "release_lora_prefix_isolation_profile");
  projection.profile_section_present =
      inferlab_artifact::contains_top_level_key_fallback(
          source.snapshot->read_result.text,
          "release_lora_prefix_isolation_profile") &&
      selected.usable;
  add_check(projection,
            source.snapshot->metadata.parse_ok &&
                projection.observed_artifact_type == spec.artifact_type,
            projection.artifact + ":artifact_type_missing_or_mismatched");
  add_check(projection, projection.profile_section_present,
            projection.artifact +
                ":release_lora_prefix_isolation_profile_missing_or_wrong_type");

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
            projection.artifact + ":release_lora_isolation_profile_id_drift");

  std::string source_role;
  const bool source_role_ok = inferlab_artifact_profile::exact_string(
                                  section, index, "source_role", source_role) &&
                              source_role == spec.source_role;
  projection.source_role = source_role.empty() ? "missing" : source_role;
  add_check(projection, source_role_ok,
            projection.artifact + ":release_lora_isolation_role_drift");

  std::string key_scope;
  const bool key_scope_ok = inferlab_artifact_profile::exact_string(
                                section, index, "prefix_cache_key_scope",
                                key_scope) &&
                            key_scope == expected.prefix_cache_key_scope;
  projection.prefix_cache_key_scope =
      key_scope.empty() ? "missing" : key_scope;
  add_check(projection, key_scope_ok,
            projection.artifact + ":release_prefix_cache_key_scope_drift");

  int residency_limit = 0;
  const bool residency_ok =
      inferlab_artifact_profile::exact_non_negative_int(
          section, index, "adapter_residency_limit", residency_limit) &&
      residency_limit == expected.adapter_residency_limit;
  projection.adapter_residency_limit = residency_limit;
  add_check(projection, residency_ok,
            projection.artifact + ":release_adapter_residency_limit_drift");

  bool sharing_allowed = true;
  const bool sharing_ok = inferlab_artifact_profile::exact_bool(
                              section, index,
                              "cross_adapter_prefix_sharing_allowed",
                              sharing_allowed) &&
                          sharing_allowed ==
                              expected.cross_adapter_prefix_sharing_allowed;
  projection.cross_adapter_prefix_sharing_allowed = sharing_allowed;
  add_check(projection, sharing_ok,
            projection.artifact + ":cross_adapter_prefix_sharing_not_disabled");

  bool matched = false;
  const bool matched_ok = inferlab_artifact_profile::exact_bool(
                              section, index, "matched", matched) &&
                          matched;
  projection.matched = matched;
  add_check(projection, matched_ok,
            projection.artifact + ":release_lora_isolation_not_matched");

  std::size_t match_count = 0U;
  const bool match_count_ok = inferlab_artifact_profile::exact_size(
                                  section, index, "match_count", match_count) &&
                              match_count == 1U;
  projection.match_count = match_count;
  add_check(projection, match_count_ok,
            projection.artifact + ":release_lora_isolation_match_count_invalid");

  const bool matched_source_ok = inferlab_artifact_profile::exact_string(
      section, index, "matched_source_identity",
      projection.matched_source_identity) &&
      projection.matched_source_identity == spec.expected_source_identity;
  add_check(projection, matched_source_ok,
            projection.artifact +
                ":release_lora_isolation_source_identity_drift");

  bool isolation_compatible = false;
  const bool isolation_compatible_ok = inferlab_artifact_profile::exact_bool(
                                           section, index,
                                           "isolation_compatible",
                                           isolation_compatible) &&
                                       isolation_compatible;
  projection.isolation_compatible = isolation_compatible;
  add_check(projection, isolation_compatible_ok,
            projection.artifact + ":release_lora_isolation_not_compatible");

  const bool status_ok = inferlab_artifact_profile::exact_string(
                             section, index, "isolation_status",
                             projection.isolation_status) &&
                         projection.isolation_status ==
                             spec.expected_isolation_status;
  add_check(projection, status_ok,
            projection.artifact + ":release_lora_isolation_status_drift");
  add_check(projection,
            inferlab_artifact_profile::empty_issues_array(section),
            projection.artifact + ":release_lora_isolation_has_issues");

  if (projection.required_check_count == 0U ||
      projection.satisfied_check_count > projection.required_check_count ||
      projection.issues.size() != projection.required_check_count -
                                      projection.satisfied_check_count) {
    throw std::logic_error(
        "release LoRA/prefix artifact accounting is invalid");
  }
  projection.ready = projection.required_check_count ==
                     projection.satisfied_check_count;
  return projection;
}

bool same_profile(const ArtifactAlignmentProjection& lhs,
                  const ArtifactAlignmentProjection& rhs) {
  return lhs.profile_id == rhs.profile_id &&
         lhs.prefix_cache_key_scope == rhs.prefix_cache_key_scope &&
         lhs.adapter_residency_limit == rhs.adapter_residency_limit &&
         lhs.cross_adapter_prefix_sharing_allowed ==
             rhs.cross_adapter_prefix_sharing_allowed;
}

}  // namespace

const std::vector<ArtifactSpec>& alignment_artifact_registry() {
  static const std::vector<ArtifactSpec> artifacts = {
      {"results/raw/native_lora_adapter_cache_plan.json",
       "native_lora_adapter_cache_plan", "lora_adapter_cache",
       "native_release_isolation_cache",
       "adapter_cache_residency_reconciled"},
      {"results/raw/native_radix_prefix_cache_plan.json",
       "native_radix_prefix_cache_plan", "radix_prefix_cache",
       "native_release_adapter_scoped_prefix",
       "radix_namespace_isolation_reconciled"},
  };
  return artifacts;
}

void validate_alignment_artifact_registry() {
  const auto& artifacts = alignment_artifact_registry();
  inferlab_release_alignment::validate_artifact_registry(
      artifacts, 2U,
      [](const ArtifactSpec& artifact) {
        return !artifact.expected_source_identity.empty() &&
               !artifact.expected_isolation_status.empty();
      },
      "release LoRA/prefix isolation");
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
      "release_lora_prefix_isolation_not_reconciled_across_all_artifacts",
      "release LoRA/prefix isolation alignment");
}

}  // namespace inferlab_release_lora_prefix_isolation
