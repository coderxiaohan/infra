#pragma once

#include "inferlab/advanced_serving_runtime_communication.hpp"
#include "release_deployment_resource_profile.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::tensor_parallel_comm {

struct Plan {
  std::vector<TensorParallelCommResult> records;
  std::size_t best_scaling_record_index = 0U;
  std::size_t worst_ratio_record_index = 0U;
  std::size_t max_exposed_comm_record_index = 0U;
};

Plan build_plan(
    const std::vector<TensorParallelCommScenario>& scenarios);

void validate_plan(const Plan& plan);
inferlab_release_deployment_resource::Binding
release_deployment_resource_binding(const Plan& plan);

}  // namespace inferlab::tensor_parallel_comm
