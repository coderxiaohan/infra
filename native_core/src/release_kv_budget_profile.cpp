#include "release_kv_budget_profile.hpp"

#include "exact_one_profile_match.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_kv_budget {
namespace {

void validate_observation(const Observation& observation) {
  if (observation.source_identity.empty() ||
      !std::isfinite(observation.gpu_memory_gib) ||
      observation.gpu_memory_gib <= 0.0 || observation.prompt_tokens <= 0 ||
      observation.output_tokens <= 0 ||
      observation.prefill_chunk_tokens <= 0 ||
      observation.budget_status.empty()) {
    throw std::invalid_argument("release KV-budget observation is invalid");
  }
}

bool matches(const Profile& profile, const Observation& observation) {
  return profile.gpu_memory_gib == observation.gpu_memory_gib &&
         profile.prompt_tokens == observation.prompt_tokens &&
         profile.output_tokens == observation.output_tokens &&
         profile.prefill_chunk_tokens == observation.prefill_chunk_tokens;
}

}  // namespace

const Profile& canonical_profile() {
  static const Profile profile{
      "release_kv_24gib_8192p_256o_chunk2048_v1", 24.0, 8192, 256,
      2048};
  validate_profile(profile);
  return profile;
}

void validate_profile(const Profile& profile) {
  if (profile.id.empty() || !std::isfinite(profile.gpu_memory_gib) ||
      profile.gpu_memory_gib <= 0.0 || profile.prompt_tokens <= 0 ||
      profile.output_tokens <= 0 || profile.prefill_chunk_tokens <= 0 ||
      profile.prefill_chunk_tokens > profile.prompt_tokens) {
    throw std::logic_error("canonical release KV-budget profile is invalid");
  }
}

Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations) {
  if (source_role.empty() || observations.empty()) {
    throw std::invalid_argument(
        "release KV-budget binding source must not be empty");
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
      candidates, "canonical_release_kv_budget_profile_missing",
      "canonical_release_kv_budget_profile_is_ambiguous");
  binding.matched_source_identity = result.matched_identity;
  binding.match_count = result.match_count;
  binding.matched = result.matched;
  binding.issues = result.issues;
  if (binding.matched && matched_observation != nullptr) {
    binding.budget_compatible = matched_observation->budget_compatible;
    binding.budget_status = matched_observation->budget_status;
    if (!binding.budget_compatible) {
      inferlab_plan::append_unique_nonempty_string(
          binding.issues, "canonical_release_kv_budget_is_not_compatible");
    }
  }
  return binding;
}

}  // namespace inferlab_release_kv_budget
