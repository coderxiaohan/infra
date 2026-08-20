#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_kv_budget {

struct Profile {
  std::string id;
  double gpu_memory_gib = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int prefill_chunk_tokens = 0;
};

struct Observation {
  std::string source_identity;
  double gpu_memory_gib = 0.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int prefill_chunk_tokens = 0;
  bool budget_compatible = false;
  std::string budget_status;
};

struct Binding {
  Profile profile;
  std::string source_role;
  std::string matched_source_identity;
  std::string budget_status = "not_evaluated";
  std::size_t match_count = 0U;
  bool matched = false;
  bool budget_compatible = false;
  std::vector<std::string> issues;
};

const Profile& canonical_profile();
void validate_profile(const Profile& profile);
Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations);

}  // namespace inferlab_release_kv_budget
