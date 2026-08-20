#include "tensor_parallel_comm_plan_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_deployment_resource_profile_view.hpp"

#include <cstddef>
#include <sstream>
#include <string>

namespace inferlab::tensor_parallel_comm {
namespace {

std::string scenario_json(
    const TensorParallelCommScenario& scenario,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "tensor-parallel scenario JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"activation_bytes\": "
      << inferlab_plan::fixed6(scenario.activation_bytes)
      << ",\n";
  out << nested << "\"accelerator_memory_gib\": "
      << inferlab_plan::fixed6(scenario.accelerator_memory_gib) << ",\n";
  out << nested << "\"accelerators_per_node\": "
      << scenario.accelerators_per_node << ",\n";
  out << nested << "\"accelerators_per_replica\": "
      << scenario.accelerators_per_replica << ",\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"batch_size\": "
      << scenario.batch_size << ",\n";
  out << nested << "\"collective_latency_us\": "
      << inferlab_plan::fixed6(scenario.collective_latency_us)
      << ",\n";
  out << nested << "\"compute_ms_per_layer\": "
      << inferlab_plan::fixed6(scenario.compute_ms_per_layer)
      << ",\n";
  out << nested << "\"hidden_size\": "
      << scenario.hidden_size << ",\n";
  out << nested << "\"layers\": " << scenario.layers << ",\n";
  out << nested << "\"link_bandwidth_gbps\": "
      << inferlab_plan::fixed6(scenario.link_bandwidth_gbps)
      << ",\n";
  out << nested << "\"name\": "
      << inferlab_plan::json_string(scenario.name) << ",\n";
  out << nested << "\"overlap_ratio\": "
      << inferlab_plan::fixed6(scenario.overlap_ratio) << ",\n";
  out << nested << "\"pipeline_parallel_size\": "
      << scenario.pipeline_parallel_size << ",\n";
  out << nested << "\"replica_count\": "
      << scenario.replica_count << ",\n";
  out << nested << "\"sequence_tokens\": "
      << scenario.sequence_tokens << ",\n";
  out << nested << "\"tensor_parallel_size\": "
      << scenario.tensor_parallel_size << ",\n";
  out << nested << "\"total_accelerator_count\": "
      << scenario.total_accelerator_count << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(const TensorParallelCommResult& result,
                        const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "tensor-parallel record JSON indent");
  const std::string nested =
      inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"activation_mb_per_layer\": "
      << inferlab_plan::fixed6(result.activation_mb_per_layer)
      << ",\n";
  out << nested << "\"all_gather_mb\": "
      << inferlab_plan::fixed6(result.all_gather_mb) << ",\n";
  out << nested << "\"all_reduce_mb\": "
      << inferlab_plan::fixed6(result.all_reduce_mb) << ",\n";
  out << nested << "\"collective_latency_ms\": "
      << inferlab_plan::fixed6(result.collective_latency_ms)
      << ",\n";
  out << nested << "\"comm_to_compute_ratio\": "
      << inferlab_plan::fixed6(result.comm_to_compute_ratio)
      << ",\n";
  out << nested << "\"communication_ms_per_layer\": "
      << inferlab_plan::fixed6(
             result.communication_ms_per_layer)
      << ",\n";
  out << nested << "\"compute_ms_total\": "
      << inferlab_plan::fixed6(result.compute_ms_total) << ",\n";
  out << nested << "\"exposed_comm_ms\": "
      << inferlab_plan::fixed6(result.exposed_comm_ms) << ",\n";
  out << nested << "\"parallelism_policy\": "
      << inferlab_plan::json_string(result.parallelism_policy)
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation)
      << ",\n";
  out << nested << "\"scaling_efficiency\": "
      << inferlab_plan::fixed6(result.scaling_efficiency)
      << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& best_scaling =
      plan.records[plan.best_scaling_record_index];
  const auto& worst_ratio =
      plan.records[plan.worst_ratio_record_index];
  const auto& max_exposed_comm =
      plan.records[plan.max_exposed_comm_record_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string("native_tensor_parallel_comm_plan")
      << ",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"claim_scope\": \"C++ native tensor-parallel communication planning artifact only; does not run benchmarks, collectives, product backends, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_scaling_efficiency\": "
      << inferlab_plan::fixed6(best_scaling.scaling_efficiency)
      << ",\n";
  out << "    \"best_scaling_scenario\": "
      << inferlab_plan::json_string(best_scaling.scenario.name)
      << ",\n";
  out << "    \"max_exposed_comm_ms\": "
      << inferlab_plan::fixed6(max_exposed_comm.exposed_comm_ms)
      << ",\n";
  out << "    \"scenario_count\": " << plan.records.size()
      << ",\n";
  out << "    \"worst_comm_to_compute_ratio\": "
      << inferlab_plan::fixed6(
             worst_ratio.comm_to_compute_ratio)
      << ",\n";
  out << "    \"worst_ratio_scenario\": "
      << inferlab_plan::json_string(worst_ratio.scenario.name)
      << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size();
       ++index) {
    out << record_json(plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  const auto release_resources = release_deployment_resource_binding(plan);
  out << "  \"release_deployment_resource_profile\": "
      << inferlab_release_deployment_resource::binding_json(
             release_resources, 2)
      << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::tensor_parallel_comm
