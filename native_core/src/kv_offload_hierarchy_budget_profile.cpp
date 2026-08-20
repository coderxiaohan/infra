#include "kv_offload_hierarchy_budget_profile.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace inferlab {

inferlab_release_kv_budget::Binding release_kv_budget_binding(
    const KvOffloadHierarchyPlan& plan) {
  double hbm_gib = 0.0;
  std::size_t hbm_count = 0U;
  for (const auto& tier : plan.tiers) {
    if (tier.kind != KvOffloadTierKind::hbm) continue;
    hbm_count = internal::checked_add_size(
        hbm_count, 1U, "release KV-budget HBM tier count");
    hbm_gib = tier.capacity_gib;
  }
  const bool unique_hbm_tier =
      hbm_count == 1U && std::isfinite(hbm_gib) && hbm_gib > 0.0;

  std::vector<inferlab_release_kv_budget::Observation> observations;
  observations.reserve(plan.workloads.size());
  for (const auto& workload : plan.workloads) {
    bool spill_supported = false;
    for (const auto& record : plan.records) {
      const bool has_non_hbm_placement = std::any_of(
          record.placements.begin(), record.placements.end(),
          [](const KvOffloadPlacement& placement) {
            return placement.tier.kind != KvOffloadTierKind::hbm &&
                   placement.placed_gib > 0.0;
          });
      if (record.workload.name == workload.name && record.active_workload &&
          record.unplaced_reusable_kv_gib == 0.0 &&
          has_non_hbm_placement) {
        spill_supported = true;
        break;
      }
    }
    const bool compatible = unique_hbm_tier && spill_supported;
    observations.push_back(
        {workload.name, hbm_gib, workload.prompt_tokens,
         workload.output_tokens, workload.prefill_chunk_tokens, compatible,
         compatible ? "local_hbm_with_hierarchy_spill_supported"
                    : "offload_hierarchy_budget_not_supported"});
  }
  return inferlab_release_kv_budget::bind_profile(
      "kv_offload_hierarchy", observations);
}

}  // namespace inferlab
