#include "release_deployment_resource_profile.hpp"

#include "checked_index.hpp"
#include "exact_one_profile_match.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_deployment_resource {
namespace {

void validate_dimensions(const int total_accelerator_count,
                         const int replica_count,
                         const int tensor_parallel_size,
                         const int pipeline_parallel_size,
                         const int accelerators_per_replica,
                         const int accelerators_per_node,
                         const double accelerator_memory_gib,
                         const char* context) {
  if (total_accelerator_count <= 0 || replica_count <= 0 ||
      tensor_parallel_size <= 0 || pipeline_parallel_size <= 0 ||
      accelerators_per_replica <= 0 || accelerators_per_node <= 0 ||
      !std::isfinite(accelerator_memory_gib) ||
      accelerator_memory_gib <= 0.0) {
    throw std::invalid_argument(std::string(context) +
                                " dimensions must be positive and finite");
  }
  const int parallel_width = inferlab::internal::checked_multiply_int(
      tensor_parallel_size, pipeline_parallel_size,
      "release deployment parallel width");
  const int declared_total = inferlab::internal::checked_multiply_int(
      replica_count, accelerators_per_replica,
      "release deployment total accelerator count");
  if (parallel_width != accelerators_per_replica ||
      declared_total != total_accelerator_count ||
      total_accelerator_count % accelerators_per_node != 0) {
    throw std::invalid_argument(std::string(context) +
                                " topology dimensions are inconsistent");
  }
}

void validate_observation(const Observation& observation) {
  if (observation.source_identity.empty() ||
      observation.resource_status.empty() ||
      observation.total_accelerator_count <= 0 ||
      observation.replica_count <= 0 ||
      observation.tensor_parallel_size <= 0 ||
      observation.pipeline_parallel_size <= 0 ||
      observation.accelerators_per_replica <= 0 ||
      observation.accelerators_per_node <= 0 ||
      !std::isfinite(observation.accelerator_memory_gib) ||
      observation.accelerator_memory_gib <= 0.0) {
    throw std::invalid_argument(
        "release deployment resource observation is invalid");
  }
}

bool matches(const Profile& profile, const Observation& observation) {
  return profile.total_accelerator_count ==
             observation.total_accelerator_count &&
         profile.replica_count == observation.replica_count &&
         profile.tensor_parallel_size == observation.tensor_parallel_size &&
         profile.pipeline_parallel_size ==
             observation.pipeline_parallel_size &&
         profile.accelerators_per_replica ==
             observation.accelerators_per_replica &&
         profile.accelerators_per_node == observation.accelerators_per_node &&
         profile.accelerator_memory_gib ==
             observation.accelerator_memory_gib;
}

}  // namespace

const Profile& canonical_profile() {
  static const Profile profile{
      "release_gpu8_replica2_tp4_pp1_node4_hbm80_v1",
      8, 2, 4, 1, 4, 4, 80.0};
  validate_profile(profile);
  return profile;
}

void validate_profile(const Profile& profile) {
  if (profile.id.empty()) {
    throw std::logic_error(
        "canonical release deployment resource profile id is empty");
  }
  try {
    validate_dimensions(
        profile.total_accelerator_count, profile.replica_count,
        profile.tensor_parallel_size, profile.pipeline_parallel_size,
        profile.accelerators_per_replica, profile.accelerators_per_node,
        profile.accelerator_memory_gib,
        "canonical release deployment resource profile");
  } catch (const std::invalid_argument& error) {
    throw std::logic_error(error.what());
  }
}

Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations) {
  if (source_role.empty() || observations.empty()) {
    throw std::invalid_argument(
        "release deployment resource binding source must not be empty");
  }
  Binding binding;
  binding.profile = canonical_profile();
  binding.source_role = source_role;
  std::vector<inferlab_profile_match::Candidate> candidates;
  candidates.reserve(observations.size());
  const Observation* matched_observation = nullptr;
  for (const auto& observation : observations) {
    validate_observation(observation);
    const bool candidate_matches = matches(binding.profile, observation);
    candidates.push_back({observation.source_identity, candidate_matches});
    if (candidate_matches && matched_observation == nullptr) {
      matched_observation = &observation;
    }
  }
  const auto result = inferlab_profile_match::exact_one(
      candidates, "canonical_release_deployment_resource_profile_missing",
      "canonical_release_deployment_resource_profile_is_ambiguous");
  binding.matched_source_identity = result.matched_identity;
  binding.match_count = result.match_count;
  binding.matched = result.matched;
  binding.issues = result.issues;
  if (binding.matched && matched_observation != nullptr) {
    binding.resource_compatible = matched_observation->resource_compatible;
    binding.resource_status = matched_observation->resource_status;
    if (!binding.resource_compatible) {
      inferlab_plan::append_unique_nonempty_string(
          binding.issues,
          "canonical_release_deployment_resources_are_not_compatible");
    }
  }
  return binding;
}

}  // namespace inferlab_release_deployment_resource
