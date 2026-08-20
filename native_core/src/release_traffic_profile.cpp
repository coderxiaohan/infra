#include "release_traffic_profile.hpp"

#include "exact_one_profile_match.hpp"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_release_traffic {
namespace {

void validate_observation(const Observation& observation) {
  if (observation.source_identity.empty() ||
      !std::isfinite(observation.request_qps) ||
      observation.request_qps < 0.0 || observation.prompt_tokens < 0 ||
      observation.output_tokens < 0 ||
      !std::isfinite(observation.gpu_hour_cost) ||
      observation.gpu_hour_cost < 0.0) {
    throw std::invalid_argument(
        "release traffic-profile observation is invalid");
  }
}

bool matches(const Profile& profile, const Observation& observation) {
  return profile.request_qps == observation.request_qps &&
         profile.prompt_tokens == observation.prompt_tokens &&
         profile.output_tokens == observation.output_tokens &&
         profile.gpu_hour_cost == observation.gpu_hour_cost;
}

}  // namespace

const Profile& canonical_profile() {
  static const Profile profile{
      "release_chat_80qps_1024p_256o_gpu3_2_v1", 80.0, 1024, 256, 3.2};
  validate_profile(profile);
  return profile;
}

void validate_profile(const Profile& profile) {
  if (profile.id.empty() || !std::isfinite(profile.request_qps) ||
      profile.request_qps <= 0.0 || profile.prompt_tokens <= 0 ||
      profile.output_tokens <= 0 ||
      !std::isfinite(profile.gpu_hour_cost) ||
      profile.gpu_hour_cost < 0.0) {
    throw std::logic_error("canonical release traffic profile is invalid");
  }
}

Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations) {
  if (source_role.empty() || observations.empty()) {
    throw std::invalid_argument(
        "release traffic-profile binding source must not be empty");
  }
  Binding binding;
  binding.profile = canonical_profile();
  binding.source_role = source_role;
  std::vector<inferlab_profile_match::Candidate> candidates;
  candidates.reserve(observations.size());
  for (const auto& observation : observations) {
    validate_observation(observation);
    candidates.push_back(
        {observation.source_identity, matches(binding.profile, observation)});
  }
  const auto result = inferlab_profile_match::exact_one(
      candidates, "canonical_release_traffic_profile_missing",
      "canonical_release_traffic_profile_is_ambiguous");
  binding.matched_source_identity = result.matched_identity;
  binding.match_count = result.match_count;
  binding.matched = result.matched;
  binding.issues = result.issues;
  return binding;
}

}  // namespace inferlab_release_traffic
