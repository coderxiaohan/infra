#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_traffic {

struct Profile {
  std::string id;
  double request_qps = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double gpu_hour_cost = 0.0;
};

struct Observation {
  std::string source_identity;
  double request_qps = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  double gpu_hour_cost = 0.0;
};

struct Binding {
  Profile profile;
  std::string source_role;
  std::string matched_source_identity;
  std::size_t match_count = 0U;
  bool matched = false;
  std::vector<std::string> issues;
};

const Profile& canonical_profile();
void validate_profile(const Profile& profile);
Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations);

}  // namespace inferlab_release_traffic
