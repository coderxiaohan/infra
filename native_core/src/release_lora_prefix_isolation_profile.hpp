#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_lora_prefix_isolation {

struct Profile {
  std::string id;
  std::string prefix_cache_key_scope;
  int adapter_residency_limit = 0;
  bool cross_adapter_prefix_sharing_allowed = true;
};

struct Observation {
  std::string source_identity;
  std::string prefix_cache_key_scope;
  int adapter_residency_limit = 0;
  bool cross_adapter_prefix_sharing_allowed = true;
  bool isolation_compatible = false;
  std::string isolation_status;
};

struct Binding {
  Profile profile;
  std::string source_role;
  std::string matched_source_identity;
  std::string isolation_status = "not_evaluated";
  std::size_t match_count = 0U;
  bool matched = false;
  bool isolation_compatible = false;
  std::vector<std::string> issues;
};

const Profile& canonical_profile();
void validate_profile(const Profile& profile);
Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations);

}  // namespace inferlab_release_lora_prefix_isolation
