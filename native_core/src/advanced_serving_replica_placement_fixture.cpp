#include "inferlab/advanced_serving_deployment_planning.hpp"

#include "mature_product_profile_registry.hpp"

#include <string>
#include <vector>

namespace inferlab {

ReplicaPlacementAssumptions default_replica_placement_assumptions() {
  ReplicaPlacementAssumptions assumptions;
  assumptions.profile_name =
      "compiled_replica_placement_reference_v2";
  return assumptions;
}

std::vector<ReplicaPlacementScenario>
default_replica_placement_scenarios() {
  return {
      {"spread_baseline", ReplicaPlacementPolicy::spread,
       8, 80.0, 38.0, 8, 12, 24, 0.10, 24.0, 90.0,
       0.32, 0.45, 760.0, 900.0, 0.08},
      {"prefix_colocated", ReplicaPlacementPolicy::prefix_colocated,
       12, 80.0, 38.0, 10, 32, 64, 0.12, 72.0, 180.0,
       0.70, 0.58, 820.0, 900.0, 0.10},
      {"lora_colocated", ReplicaPlacementPolicy::lora_colocated,
       16, 80.0, 38.0, 12, 64, 160, 0.16, 48.0, 260.0,
       0.52, 0.82, 930.0, 900.0, 0.18},
      {"slo_tiered_pool", ReplicaPlacementPolicy::slo_tiered,
       24, 80.0, 42.0, 18, 96, 256, 0.18, 96.0, 480.0,
       0.62, 0.74, 1320.0, 900.0, 0.24},
      {"drain_rebalance", ReplicaPlacementPolicy::drain_rebalance,
       16, 80.0, 38.0, 14, 48, 96, 0.14, 64.0, 210.0,
       0.58, 0.66, 1050.0, 900.0, 0.46},
      {"remote_overflow", ReplicaPlacementPolicy::remote_overflow,
       12, 80.0, 48.0, 12, 128, 320, 0.20, 180.0, 520.0,
       0.48, 0.56, 1600.0, 900.0, 0.32},
  };
}

std::vector<std::string> default_replica_placement_research_basis() {
  static constexpr const char* kProductIds[] = {
      "kserve_llmd", "dynamo", "ray_serve", "kv_offload"};
  std::vector<std::string> basis = {"AIBrix"};
  basis.reserve(1U + sizeof(kProductIds) / sizeof(kProductIds[0]));
  for (const char* id : kProductIds) {
    basis.push_back(
        mature_product_profiles::require_product_profile(id).display_name);
  }
  return basis;
}

}  // namespace inferlab
