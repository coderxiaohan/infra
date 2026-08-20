#include "release_kv_budget_alignment_contract.hpp"

#include "artifact_metadata_reader.hpp"
#include "artifact_profile_section.hpp"
#include "release_alignment_contract_utils.hpp"
#include "release_kv_budget_profile.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_kv_budget {
namespace {

struct BoundSource {
  const ArtifactSpec* spec = nullptr;
  const inferlab_evidence::EvidenceSnapshot* snapshot = nullptr;
};

void add_check(ArtifactAlignmentProjection& projection,
               const bool satisfied, const std::string& issue) {
  inferlab_release_alignment::add_check(projection, satisfied, issue,
                                        "release KV-budget artifact");
}

ArtifactAlignmentProjection inspect_source(const BoundSource& source) {
  if (source.spec == nullptr || source.snapshot == nullptr) {
    throw std::logic_error("release KV-budget source binding is invalid");
  }
  const ArtifactSpec& spec = *source.spec;
  ArtifactAlignmentProjection projection;
  projection.artifact = spec.artifact;
  projection.expected_artifact_type = spec.artifact_type;
  projection.observed_artifact_type = source.snapshot->metadata.artifact_type;
  const auto selected =
      inferlab_artifact::strict_json_object_section_or_flattened_root(
          source.snapshot->read_result.text, "release_kv_budget_profile");
  projection.profile_section_present =
      inferlab_artifact::contains_top_level_key_fallback(
          source.snapshot->read_result.text, "release_kv_budget_profile") &&
      selected.usable;
  add_check(projection,
            source.snapshot->metadata.parse_ok &&
                projection.observed_artifact_type == spec.artifact_type,
            projection.artifact + ":artifact_type_missing_or_mismatched");
  add_check(projection, projection.profile_section_present,
            projection.artifact +
                ":release_kv_budget_profile_missing_or_wrong_type");

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
            projection.artifact + ":release_kv_budget_profile_id_drift");

  std::string source_role;
  const bool source_role_ok = inferlab_artifact_profile::exact_string(
                                  section, index, "source_role", source_role) &&
                              source_role == spec.source_role;
  projection.source_role = source_role.empty() ? "missing" : source_role;
  add_check(projection, source_role_ok,
            projection.artifact + ":release_kv_budget_source_role_drift");

  double gpu_memory_gib = 0.0;
  const bool gpu_ok = inferlab_artifact_profile::exact_number(
                          section, index, "gpu_memory_gib", gpu_memory_gib) &&
                      gpu_memory_gib == expected.gpu_memory_gib;
  projection.gpu_memory_gib = gpu_memory_gib;
  add_check(projection, gpu_ok,
            projection.artifact + ":release_kv_gpu_memory_budget_drift");

  int prompt_tokens = 0;
  const bool prompt_ok = inferlab_artifact_profile::exact_non_negative_int(
                             section, index, "prompt_tokens", prompt_tokens) &&
                         prompt_tokens == expected.prompt_tokens;
  projection.prompt_tokens = prompt_tokens;
  add_check(projection, prompt_ok,
            projection.artifact + ":release_kv_prompt_tokens_drift");

  int output_tokens = 0;
  const bool output_ok = inferlab_artifact_profile::exact_non_negative_int(
                             section, index, "output_tokens", output_tokens) &&
                         output_tokens == expected.output_tokens;
  projection.output_tokens = output_tokens;
  add_check(projection, output_ok,
            projection.artifact + ":release_kv_output_tokens_drift");

  int chunk_tokens = 0;
  const bool chunk_ok = inferlab_artifact_profile::exact_non_negative_int(
                            section, index, "prefill_chunk_tokens",
                            chunk_tokens) &&
                        chunk_tokens == expected.prefill_chunk_tokens;
  projection.prefill_chunk_tokens = chunk_tokens;
  add_check(projection, chunk_ok,
            projection.artifact + ":release_kv_prefill_chunk_drift");

  bool matched = false;
  const bool matched_ok = inferlab_artifact_profile::exact_bool(
                              section, index, "matched", matched) &&
                          matched;
  projection.matched = matched;
  add_check(projection, matched_ok,
            projection.artifact + ":release_kv_profile_not_matched");

  std::size_t match_count = 0U;
  const bool match_count_ok = inferlab_artifact_profile::exact_size(
                                  section, index, "match_count", match_count) &&
                              match_count == 1U;
  projection.match_count = match_count;
  add_check(projection, match_count_ok,
            projection.artifact + ":release_kv_match_count_invalid");

  const bool matched_source_ok = inferlab_artifact_profile::exact_string(
      section, index, "matched_source_identity",
      projection.matched_source_identity) &&
      projection.matched_source_identity == spec.expected_source_identity;
  add_check(projection, matched_source_ok,
            projection.artifact +
                ":release_kv_matched_source_identity_missing");

  bool budget_compatible = false;
  const bool budget_compatible_ok = inferlab_artifact_profile::exact_bool(
                                        section, index, "budget_compatible",
                                        budget_compatible) &&
                                    budget_compatible;
  projection.budget_compatible = budget_compatible;
  add_check(projection, budget_compatible_ok,
            projection.artifact + ":release_kv_budget_not_compatible");

  const bool budget_status_ok = inferlab_artifact_profile::exact_string(
                                    section, index, "budget_status",
                                    projection.budget_status) &&
                                projection.budget_status ==
                                    spec.expected_budget_status;
  add_check(projection, budget_status_ok,
            projection.artifact + ":release_kv_budget_status_missing");
  add_check(projection,
            inferlab_artifact_profile::empty_issues_array(section),
            projection.artifact + ":release_kv_budget_binding_has_issues");

  if (projection.required_check_count == 0U ||
      projection.satisfied_check_count > projection.required_check_count ||
      projection.issues.size() != projection.required_check_count -
                                      projection.satisfied_check_count) {
    throw std::logic_error(
        "release KV-budget artifact alignment accounting is invalid");
  }
  projection.ready = projection.required_check_count ==
                     projection.satisfied_check_count;
  return projection;
}

bool same_profile(const ArtifactAlignmentProjection& lhs,
                  const ArtifactAlignmentProjection& rhs) {
  return lhs.profile_id == rhs.profile_id &&
         lhs.gpu_memory_gib == rhs.gpu_memory_gib &&
         lhs.prompt_tokens == rhs.prompt_tokens &&
         lhs.output_tokens == rhs.output_tokens &&
         lhs.prefill_chunk_tokens == rhs.prefill_chunk_tokens;
}

}  // namespace

const std::vector<ArtifactSpec>& alignment_artifact_registry() {
  static const std::vector<ArtifactSpec> artifacts = {
      {"results/raw/native_engine_memory_planner.json",
       "native_engine_memory_planner", "engine_memory_planner",
       "release_kv_13b_int4_8k", "fits_local_kv_budget"},
      {"results/raw/native_long_context_matrix.json",
       "native_long_context_matrix", "long_context_matrix",
       "release_kv_budget_8k", "fits_local_kv_budget"},
      {"results/raw/native_chunked_prefill_scheduler_plan.json",
       "native_chunked_prefill_scheduler_plan",
       "chunked_prefill_scheduler", "release_kv_budget_8k",
       "fits_local_kv_budget"},
      {"results/raw/native_kv_offload_hierarchy_plan.json",
       "native_kv_offload_hierarchy_plan", "kv_offload_hierarchy",
       "rag_long_context", "local_hbm_with_hierarchy_spill_supported"},
  };
  return artifacts;
}

void validate_alignment_artifact_registry() {
  const auto& artifacts = alignment_artifact_registry();
  inferlab_release_alignment::validate_artifact_registry(
      artifacts, 4U,
      [](const ArtifactSpec& artifact) {
        return !artifact.expected_source_identity.empty() &&
               !artifact.expected_budget_status.empty();
      },
      "release KV-budget alignment");
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
      "release_kv_budget_profile_not_reconciled_across_all_artifacts",
      "release KV-budget alignment");
}

}  // namespace inferlab_release_kv_budget
