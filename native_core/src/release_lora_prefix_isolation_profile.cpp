#include "release_lora_prefix_isolation_profile.hpp"

#include "exact_one_profile_match.hpp"
#include "plan_json_utils.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_lora_prefix_isolation {
namespace {

void validate_observation(const Observation& observation) {
  if (observation.source_identity.empty() ||
      observation.prefix_cache_key_scope.empty() ||
      observation.adapter_residency_limit <= 0 ||
      observation.isolation_status.empty()) {
    throw std::invalid_argument(
        "release LoRA/prefix isolation observation is invalid");
  }
}

bool matches(const Profile& profile, const Observation& observation) {
  return profile.prefix_cache_key_scope ==
             observation.prefix_cache_key_scope &&
         profile.adapter_residency_limit ==
             observation.adapter_residency_limit &&
         profile.cross_adapter_prefix_sharing_allowed ==
             observation.cross_adapter_prefix_sharing_allowed;
}

}  // namespace

const Profile& canonical_profile() {
  static const Profile profile{
      "release_adapter_scoped_prefix_cache_resident12_v1",
      "base_model_id:adapter_id:adapter_revision:template_hash", 12, false};
  validate_profile(profile);
  return profile;
}

void validate_profile(const Profile& profile) {
  if (profile.id.empty() || profile.prefix_cache_key_scope.empty() ||
      profile.adapter_residency_limit <= 0 ||
      profile.cross_adapter_prefix_sharing_allowed) {
    throw std::logic_error(
        "canonical release LoRA/prefix isolation profile is invalid");
  }
}

Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations) {
  if (source_role.empty() || observations.empty()) {
    throw std::invalid_argument(
        "release LoRA/prefix isolation binding source must not be empty");
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
      candidates, "canonical_release_lora_prefix_isolation_profile_missing",
      "canonical_release_lora_prefix_isolation_profile_is_ambiguous");
  binding.matched_source_identity = result.matched_identity;
  binding.match_count = result.match_count;
  binding.matched = result.matched;
  binding.issues = result.issues;
  if (binding.matched && matched_observation != nullptr) {
    binding.isolation_compatible =
        matched_observation->isolation_compatible;
    binding.isolation_status = matched_observation->isolation_status;
    if (!binding.isolation_compatible) {
      inferlab_plan::append_unique_nonempty_string(
          binding.issues,
          "canonical_release_lora_prefix_isolation_is_not_compatible");
    }
  }
  return binding;
}

}  // namespace inferlab_release_lora_prefix_isolation
