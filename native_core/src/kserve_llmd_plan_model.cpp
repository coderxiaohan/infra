#include "kserve_llmd_plan_model.hpp"

#include "checked_index.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::kserve_llmd_plan {
namespace {

bool same_scenario(const KserveLlmdManifestScenario& lhs,
                   const KserveLlmdManifestScenario& rhs) {
  return lhs.name == rhs.name &&
         lhs.backend == rhs.backend &&
         lhs.model_uri == rhs.model_uri &&
         lhs.decode_replicas == rhs.decode_replicas &&
         lhs.prefill_replicas == rhs.prefill_replicas &&
         lhs.tensor_parallel_size == rhs.tensor_parallel_size &&
         lhs.gpu_per_replica == rhs.gpu_per_replica &&
         lhs.kv_cache_aware_routing == rhs.kv_cache_aware_routing &&
         lhs.disaggregated_prefill == rhs.disaggregated_prefill &&
         lhs.target_qps == rhs.target_qps &&
         lhs.target_ttft_ms == rhs.target_ttft_ms &&
         lhs.target_tpot_ms == rhs.target_tpot_ms &&
         lhs.pipeline_parallel_size == rhs.pipeline_parallel_size &&
         lhs.accelerators_per_node == rhs.accelerators_per_node &&
         lhs.accelerator_memory_gib == rhs.accelerator_memory_gib;
}

bool same_result(const KserveLlmdManifestResult& lhs,
                 const KserveLlmdManifestResult& rhs) {
  return same_scenario(lhs.scenario, rhs.scenario) &&
         lhs.total_gpu_request == rhs.total_gpu_request &&
         lhs.total_replicas == rhs.total_replicas &&
         lhs.routing_readiness_score == rhs.routing_readiness_score &&
         lhs.slo_fit_score == rhs.slo_fit_score &&
         lhs.deployment_complexity_score ==
             rhs.deployment_complexity_score &&
         lhs.service_kind == rhs.service_kind &&
         lhs.gateway_policy == rhs.gateway_policy &&
         lhs.worker_topology == rhs.worker_topology &&
         lhs.yaml_profile == rhs.yaml_profile &&
         lhs.deployment_mode == rhs.deployment_mode &&
         lhs.docker_compose_guidance == rhs.docker_compose_guidance &&
         lhs.recommendation == rhs.recommendation;
}

void validate_scenarios(
    const std::vector<KserveLlmdManifestScenario>& scenarios) {
  if (scenarios.empty()) {
    throw std::logic_error(
        "KServe/llm-d scenarios must not be empty");
  }
  std::unordered_set<std::string> names;
  names.reserve(scenarios.size());
  for (const auto& scenario : scenarios) {
    if (scenario.name.empty() || scenario.backend.empty() ||
        scenario.model_uri.empty()) {
      throw std::logic_error(
          "KServe/llm-d scenario identity is invalid");
    }
    if (!names.insert(scenario.name).second) {
      throw std::logic_error(
          "KServe/llm-d scenario names must be unique");
    }
  }
}

Summary summarize(
    const std::vector<KserveLlmdManifestResult>& records) {
  if (records.empty()) {
    throw std::logic_error(
        "KServe/llm-d records must not be empty");
  }
  Summary summary;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (records[index].slo_fit_score >
        records[summary.best_slo_index].slo_fit_score) {
      summary.best_slo_index = index;
    }
    if (records[index].routing_readiness_score >
        records[summary.best_routing_index].routing_readiness_score) {
      summary.best_routing_index = index;
    }
    if (records[index].deployment_complexity_score <
        records[summary.lowest_complexity_index]
            .deployment_complexity_score) {
      summary.lowest_complexity_index = index;
    }
    if (records[index].total_gpu_request >
        records[summary.max_gpu_index].total_gpu_request) {
      summary.max_gpu_index = index;
    }
  }
  return summary;
}

}  // namespace

Plan build_default_plan() {
  Plan plan;
  plan.scenarios = default_kserve_llmd_manifest_scenarios();
  validate_scenarios(plan.scenarios);
  plan.records.reserve(plan.scenarios.size());
  for (const auto& scenario : plan.scenarios) {
    plan.records.push_back(estimate_kserve_llmd_manifest(scenario));
  }
  plan.summary = summarize(plan.records);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_scenarios(plan.scenarios);
  if (plan.records.size() != plan.scenarios.size()) {
    throw std::logic_error(
        "KServe/llm-d scenario/record alignment is invalid");
  }
  if (plan.summary.best_slo_index >= plan.records.size() ||
      plan.summary.best_routing_index >= plan.records.size() ||
      plan.summary.lowest_complexity_index >= plan.records.size() ||
      plan.summary.max_gpu_index >= plan.records.size()) {
    throw std::logic_error(
        "KServe/llm-d summary index is out of range");
  }

  std::unordered_set<std::string> normalized_names;
  normalized_names.reserve(plan.records.size());
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    const auto expected =
        estimate_kserve_llmd_manifest(plan.scenarios[index]);
    if (!same_result(plan.records[index], expected)) {
      throw std::logic_error(
          "KServe/llm-d record is not the canonical estimate");
    }
    if (!normalized_names.insert(expected.scenario.name).second) {
      throw std::logic_error(
          "KServe/llm-d normalized names must be unique");
    }
  }

  const auto expected_summary = summarize(plan.records);
  if (plan.summary.best_slo_index != expected_summary.best_slo_index ||
      plan.summary.best_routing_index !=
          expected_summary.best_routing_index ||
      plan.summary.lowest_complexity_index !=
          expected_summary.lowest_complexity_index ||
      plan.summary.max_gpu_index != expected_summary.max_gpu_index) {
    throw std::logic_error(
        "KServe/llm-d summary is not canonical");
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
    if (scenario.accelerators_per_node <= 0 ||
        scenario.accelerator_memory_gib <= 0.0) {
      continue;
    }
    const int parallel_width = internal::checked_multiply_int(
        scenario.tensor_parallel_size, scenario.pipeline_parallel_size,
        "KServe release deployment parallel width");
    const int accelerators_per_replica = internal::checked_multiply_int(
        scenario.gpu_per_replica, parallel_width,
        "KServe release accelerators per replica");
    const int expected_total = internal::checked_multiply_int(
        record.total_replicas, accelerators_per_replica,
        "KServe release total accelerator count");
    const bool compatible =
        record.total_gpu_request == expected_total &&
        expected_total % scenario.accelerators_per_node == 0;
    observations.push_back(
        {scenario.name,
         record.total_gpu_request,
         record.total_replicas,
         scenario.tensor_parallel_size,
         scenario.pipeline_parallel_size,
         accelerators_per_replica,
         scenario.accelerators_per_node,
         scenario.accelerator_memory_gib,
         compatible,
         compatible ? "manifest_gpu_requests_reconciled"
                    : "manifest_gpu_requests_inconsistent"});
  }
  return inferlab_release_deployment_resource::bind_profile(
      "kserve_llmd_manifest", observations);
}

}  // namespace inferlab::kserve_llmd_plan
