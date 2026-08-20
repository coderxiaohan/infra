#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_release_deployment_resource {

struct Profile {
  std::string id;
  int total_accelerator_count = 0;
  int replica_count = 0;
  int tensor_parallel_size = 0;
  int pipeline_parallel_size = 0;
  int accelerators_per_replica = 0;
  int accelerators_per_node = 0;
  double accelerator_memory_gib = 0.0;
};

struct Observation {
  std::string source_identity;
  int total_accelerator_count = 0;
  int replica_count = 0;
  int tensor_parallel_size = 0;
  int pipeline_parallel_size = 0;
  int accelerators_per_replica = 0;
  int accelerators_per_node = 0;
  double accelerator_memory_gib = 0.0;
  bool resource_compatible = false;
  std::string resource_status;
};

struct Binding {
  Profile profile;
  std::string source_role;
  std::string matched_source_identity;
  std::string resource_status = "not_evaluated";
  std::size_t match_count = 0U;
  bool matched = false;
  bool resource_compatible = false;
  std::vector<std::string> issues;
};

const Profile& canonical_profile();
void validate_profile(const Profile& profile);
Binding bind_profile(const std::string& source_role,
                     const std::vector<Observation>& observations);

}  // namespace inferlab_release_deployment_resource
