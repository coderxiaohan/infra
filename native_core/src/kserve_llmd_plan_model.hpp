#pragma once

#include "inferlab/advanced_serving_deployment_planning.hpp"
#include "release_deployment_resource_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::kserve_llmd_plan {

struct Summary {
  std::size_t best_slo_index = 0U;
  std::size_t best_routing_index = 0U;
  std::size_t lowest_complexity_index = 0U;
  std::size_t max_gpu_index = 0U;
};

struct Plan {
  std::vector<KserveLlmdManifestScenario> scenarios;
  std::vector<KserveLlmdManifestResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);
inferlab_release_deployment_resource::Binding
release_deployment_resource_binding(const Plan& plan);

}  // namespace inferlab::kserve_llmd_plan
