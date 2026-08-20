#include "tensor_parallel_comm_plan_model.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::tensor_parallel_comm {
namespace {

bool same_scenario(const TensorParallelCommScenario& left,
                   const TensorParallelCommScenario& right) {
  return left.name == right.name &&
         left.backend == right.backend &&
         left.tensor_parallel_size ==
             right.tensor_parallel_size &&
         left.pipeline_parallel_size ==
             right.pipeline_parallel_size &&
         left.batch_size == right.batch_size &&
         left.hidden_size == right.hidden_size &&
         left.sequence_tokens == right.sequence_tokens &&
         left.layers == right.layers &&
         left.activation_bytes == right.activation_bytes &&
         left.link_bandwidth_gbps ==
             right.link_bandwidth_gbps &&
         left.collective_latency_us ==
             right.collective_latency_us &&
         left.overlap_ratio == right.overlap_ratio &&
         left.compute_ms_per_layer ==
             right.compute_ms_per_layer &&
         left.replica_count == right.replica_count &&
         left.total_accelerator_count == right.total_accelerator_count &&
         left.accelerators_per_replica == right.accelerators_per_replica &&
         left.accelerators_per_node == right.accelerators_per_node &&
         left.accelerator_memory_gib == right.accelerator_memory_gib;
}

void validate_result(const TensorParallelCommResult& result) {
  const auto& scenario = result.scenario;
  const bool has_release_topology =
      scenario.replica_count > 0 || scenario.total_accelerator_count > 0 ||
      scenario.accelerators_per_replica > 0 ||
      scenario.accelerators_per_node > 0 ||
      scenario.accelerator_memory_gib > 0.0;
  if (scenario.replica_count < 0 || scenario.total_accelerator_count < 0 ||
      scenario.accelerators_per_replica < 0 ||
      scenario.accelerators_per_node < 0 ||
      !std::isfinite(scenario.accelerator_memory_gib) ||
      scenario.accelerator_memory_gib < 0.0) {
    throw std::invalid_argument(
        "tensor-parallel release resource dimensions are invalid");
  }
  if (has_release_topology) {
    if (scenario.replica_count <= 0 ||
        scenario.total_accelerator_count <= 0 ||
        scenario.accelerators_per_replica <= 0 ||
        scenario.accelerators_per_node <= 0 ||
        scenario.accelerator_memory_gib <= 0.0) {
      throw std::invalid_argument(
          "tensor-parallel release topology must be fully specified");
    }
    const int parallel_width = internal::checked_multiply_int(
        scenario.tensor_parallel_size, scenario.pipeline_parallel_size,
        "tensor-parallel release parallel width");
    const int declared_total = internal::checked_multiply_int(
        scenario.replica_count, scenario.accelerators_per_replica,
        "tensor-parallel release total accelerator count");
    if (parallel_width != scenario.accelerators_per_replica ||
        declared_total != scenario.total_accelerator_count ||
        scenario.total_accelerator_count % scenario.accelerators_per_node !=
            0) {
      throw std::invalid_argument(
          "tensor-parallel release topology is inconsistent");
    }
  }
  const TensorParallelCommResult expected =
      estimate_tensor_parallel_comm(result.scenario);
  if (!same_scenario(result.scenario, expected.scenario) ||
      result.parallelism_policy != expected.parallelism_policy ||
      result.recommendation != expected.recommendation) {
    throw std::invalid_argument(
        "tensor-parallel communication result state or policy drift");
  }

  struct Metric {
    double actual;
    double expected;
    const char* name;
  };
  const Metric metrics[] = {
      {result.activation_mb_per_layer,
       expected.activation_mb_per_layer,
       "tensor-parallel activation MiB per layer"},
      {result.all_reduce_mb, expected.all_reduce_mb,
       "tensor-parallel all-reduce MiB"},
      {result.all_gather_mb, expected.all_gather_mb,
       "tensor-parallel all-gather MiB"},
      {result.collective_latency_ms,
       expected.collective_latency_ms,
       "tensor-parallel collective latency"},
      {result.communication_ms_per_layer,
       expected.communication_ms_per_layer,
       "tensor-parallel communication time per layer"},
      {result.exposed_comm_ms, expected.exposed_comm_ms,
       "tensor-parallel exposed communication"},
      {result.compute_ms_total, expected.compute_ms_total,
       "tensor-parallel total compute time"},
      {result.comm_to_compute_ratio,
       expected.comm_to_compute_ratio,
       "tensor-parallel communication-to-compute ratio"},
      {result.scaling_efficiency, expected.scaling_efficiency,
       "tensor-parallel scaling efficiency"},
  };
  for (const auto& metric : metrics) {
    advanced_serving_internal::require_non_negative_finite(
        metric.actual, metric.name);
    if (metric.actual != metric.expected) {
      throw std::invalid_argument(
          std::string(metric.name) + " contract drift");
    }
  }
  advanced_serving_internal::require_positive_finite(
      result.activation_mb_per_layer,
      "tensor-parallel activation MiB per layer");
  advanced_serving_internal::require_positive_finite(
      result.compute_ms_total,
      "tensor-parallel total compute time");
  advanced_serving_internal::require_unit_interval(
      result.scaling_efficiency,
      "tensor-parallel scaling efficiency");
}

std::size_t best_scaling_index(
    const std::vector<TensorParallelCommResult>& records) {
  const auto selected = std::max_element(
      records.begin(), records.end(),
      [](const auto& left, const auto& right) {
        return left.scaling_efficiency <
               right.scaling_efficiency;
      });
  return static_cast<std::size_t>(selected - records.begin());
}

std::size_t worst_ratio_index(
    const std::vector<TensorParallelCommResult>& records) {
  const auto selected = std::max_element(
      records.begin(), records.end(),
      [](const auto& left, const auto& right) {
        return left.comm_to_compute_ratio <
               right.comm_to_compute_ratio;
      });
  return static_cast<std::size_t>(selected - records.begin());
}

std::size_t max_exposed_comm_index(
    const std::vector<TensorParallelCommResult>& records) {
  const auto selected = std::max_element(
      records.begin(), records.end(),
      [](const auto& left, const auto& right) {
        return left.exposed_comm_ms < right.exposed_comm_ms;
      });
  return static_cast<std::size_t>(selected - records.begin());
}

}  // namespace

Plan build_plan(
    const std::vector<TensorParallelCommScenario>& scenarios) {
  Plan plan;
  plan.records.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    plan.records.push_back(
        estimate_tensor_parallel_comm(scenario));
  }
  if (!plan.records.empty()) {
    plan.best_scaling_record_index =
        best_scaling_index(plan.records);
    plan.worst_ratio_record_index =
        worst_ratio_index(plan.records);
    plan.max_exposed_comm_record_index =
        max_exposed_comm_index(plan.records);
  }
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  if (plan.records.empty()) {
    throw std::invalid_argument(
        "tensor-parallel communication plan requires records");
  }

  std::vector<std::string> scenario_names;
  scenario_names.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    validate_result(record);
    scenario_names.push_back(record.scenario.name);
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      scenario_names,
      "tensor-parallel communication scenario identities");

  const std::size_t expected_best_scaling =
      best_scaling_index(plan.records);
  const std::size_t expected_worst_ratio =
      worst_ratio_index(plan.records);
  const std::size_t expected_max_exposed_comm =
      max_exposed_comm_index(plan.records);
  if (plan.best_scaling_record_index >= plan.records.size() ||
      plan.worst_ratio_record_index >= plan.records.size() ||
      plan.max_exposed_comm_record_index >=
          plan.records.size() ||
      plan.best_scaling_record_index != expected_best_scaling ||
      plan.worst_ratio_record_index != expected_worst_ratio ||
      plan.max_exposed_comm_record_index !=
          expected_max_exposed_comm) {
    throw std::invalid_argument(
        "tensor-parallel communication plan summary ranking drift");
  }
}

inferlab_release_deployment_resource::Binding
release_deployment_resource_binding(const Plan& plan) {
  validate_plan(plan);
  std::vector<inferlab_release_deployment_resource::Observation>
      observations;
  observations.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    const auto& scenario = record.scenario;
    if (scenario.replica_count <= 0) continue;
    const int expected_total = internal::checked_multiply_int(
        scenario.replica_count, scenario.accelerators_per_replica,
        "tensor-parallel release resource replica total");
    const int parallel_width = internal::checked_multiply_int(
        scenario.tensor_parallel_size, scenario.pipeline_parallel_size,
        "tensor-parallel release resource parallel width");
    const bool compatible =
        scenario.total_accelerator_count == expected_total &&
        parallel_width == scenario.accelerators_per_replica &&
        scenario.total_accelerator_count % scenario.accelerators_per_node ==
            0;
    observations.push_back(
        {scenario.name,
         scenario.total_accelerator_count,
         scenario.replica_count,
         scenario.tensor_parallel_size,
         scenario.pipeline_parallel_size,
         scenario.accelerators_per_replica,
         scenario.accelerators_per_node,
         scenario.accelerator_memory_gib,
         compatible,
         compatible ? "parallel_group_inventory_reconciled"
                    : "parallel_group_inventory_inconsistent"});
  }
  return inferlab_release_deployment_resource::bind_profile(
      "tensor_parallel_topology", observations);
}

}  // namespace inferlab::tensor_parallel_comm
