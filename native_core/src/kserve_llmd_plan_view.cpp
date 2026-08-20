#include "kserve_llmd_plan_view.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "release_deployment_resource_profile_view.hpp"

namespace inferlab_kserve_view {

const std::vector<std::string>& field_metric_mapping() {
  static const std::vector<std::string> mapping = {
      "spec.predictor.model.uri -> scenario.model_uri",
      "spec.predictor.workerSpec.tensorParallelSize -> scenario.tensor_parallel_size",
      "spec.predictor.workerSpec.gpuPerReplica -> scenario.gpu_per_replica",
      "spec.router.kvCacheAware -> scenario.kv_cache_aware_routing and routing_readiness_score",
      "spec.prefill.replicas -> scenario.prefill_replicas and target_ttft_ms",
      "spec.decode.replicas -> scenario.decode_replicas and target_tpot_ms",
      "spec.autoscaling.targetQPS -> scenario.target_qps and slo_fit_score",
      "Gateway/HTTPRoute host -> gateway_policy",
      "Prometheus serviceMonitor -> service_kind and worker_topology"};
  return mapping;
}

std::vector<std::string> migration_commands(
    const inferlab::KserveLlmdManifestResult& result) {
  const std::string manifest = "deploy/kserve_llm_d.example.yaml";
  return {
      "kubectl create namespace inferlab-serving --dry-run=client -o yaml | kubectl apply -f -",
      "kubectl -n inferlab-serving apply --server-side --dry-run=server -f " +
          manifest,
      "kubectl -n inferlab-serving get llminferenceservice,gateway,httproute,pods",
      "python3 scripts/export_prometheus_metrics.py --repo-root . --results results/raw --output results/tables/inferlab_metrics.prom",
      "curl -sS ${INFERLAB_GATEWAY_URL}/metrics | head",
      "curl -sS ${INFERLAB_GATEWAY_URL}/v1/models",
      "mode=" + result.deployment_mode + " backend=" +
          result.scenario.backend + " model_uri=" + result.scenario.model_uri};
}

std::string scenario_json(
    const inferlab::KserveLlmdManifestScenario& scenario,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(
      inferlab::internal::checked_add_int(
          indent, 2, "KServe scenario JSON indent"));
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"backend\": "
      << inferlab_plan::json_string(scenario.backend) << ",\n";
  out << nested << "\"accelerator_memory_gib\": "
      << inferlab_plan::fixed6(scenario.accelerator_memory_gib) << ",\n";
  out << nested << "\"accelerators_per_node\": "
      << scenario.accelerators_per_node << ",\n";
  out << nested << "\"decode_replicas\": " << scenario.decode_replicas
      << ",\n";
  out << nested << "\"disaggregated_prefill\": "
      << inferlab_plan::json_bool(scenario.disaggregated_prefill) << ",\n";
  out << nested << "\"gpu_per_replica\": " << scenario.gpu_per_replica
      << ",\n";
  out << nested << "\"kv_cache_aware_routing\": "
      << inferlab_plan::json_bool(scenario.kv_cache_aware_routing) << ",\n";
  out << nested << "\"model_uri\": "
      << inferlab_plan::json_string(scenario.model_uri) << ",\n";
  out << nested << "\"name\": " << inferlab_plan::json_string(scenario.name)
      << ",\n";
  out << nested << "\"prefill_replicas\": " << scenario.prefill_replicas
      << ",\n";
  out << nested << "\"pipeline_parallel_size\": "
      << scenario.pipeline_parallel_size << ",\n";
  out << nested << "\"target_qps\": " << std::fixed
      << std::setprecision(6) << scenario.target_qps << ",\n";
  out << nested << "\"target_tpot_ms\": " << std::fixed
      << std::setprecision(6) << scenario.target_tpot_ms << ",\n";
  out << nested << "\"target_ttft_ms\": " << std::fixed
      << std::setprecision(6) << scenario.target_ttft_ms << ",\n";
  out << nested << "\"tensor_parallel_size\": "
      << scenario.tensor_parallel_size << "\n";
  out << pad << "}";
  return out.str();
}

std::string record_json(
    const inferlab::KserveLlmdManifestResult& result,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "KServe record JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"deployment_complexity_score\": " << std::fixed
      << std::setprecision(6) << result.deployment_complexity_score << ",\n";
  out << nested << "\"deployment_mode\": "
      << inferlab_plan::json_string(result.deployment_mode) << ",\n";
  out << nested << "\"docker_compose_guidance\": "
      << inferlab_plan::json_string(result.docker_compose_guidance)
      << ",\n";
  out << nested << "\"field_metric_mapping\": "
      << inferlab_plan::json_string_array_multiline(field_metric_mapping(),
                                                     nested_indent)
      << ",\n";
  out << nested << "\"gateway_policy\": "
      << inferlab_plan::json_string(result.gateway_policy) << ",\n";
  out << nested << "\"migration_commands\": "
      << inferlab_plan::json_string_array_multiline(
             migration_commands(result), nested_indent)
      << ",\n";
  out << nested << "\"recommendation\": "
      << inferlab_plan::json_string(result.recommendation) << ",\n";
  out << nested << "\"routing_readiness_score\": " << std::fixed
      << std::setprecision(6) << result.routing_readiness_score << ",\n";
  out << nested << "\"scenario\": "
      << scenario_json(result.scenario, nested_indent) << ",\n";
  out << nested << "\"service_kind\": "
      << inferlab_plan::json_string(result.service_kind) << ",\n";
  out << nested << "\"slo_fit_score\": " << std::fixed
      << std::setprecision(6) << result.slo_fit_score << ",\n";
  out << nested << "\"total_gpu_request\": " << result.total_gpu_request
      << ",\n";
  out << nested << "\"total_replicas\": " << result.total_replicas
      << ",\n";
  out << nested << "\"worker_topology\": "
      << inferlab_plan::json_string(result.worker_topology) << ",\n";
  out << nested << "\"yaml_profile\": "
      << inferlab_plan::json_string(result.yaml_profile) << "\n";
  out << pad << "}";
  return out.str();
}

std::string artifact_json(
    const inferlab::kserve_llmd_plan::Plan& plan) {
  inferlab::kserve_llmd_plan::validate_plan(plan);
  const auto& best_slo = plan.records[plan.summary.best_slo_index];
  const auto& max_gpu = plan.records[plan.summary.max_gpu_index];

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": \"native_kserve_llm_d_plan\",\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run_plan\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"dry_run\": true,\n";
  out << "  \"claim_scope\": \"C++ native KServe/llm-d deployment and manifest planning artifact only; does not apply Kubernetes resources, run product backends, benchmarks, or target-environment execution\",\n";
  out << "  \"summary\": {\n";
  const auto& best_routing = plan.records[plan.summary.best_routing_index];
  out << "    \"best_routing_scenario\": "
      << inferlab_plan::json_string(best_routing.scenario.name) << ",\n";
  out << "    \"best_routing_score\": " << std::fixed
      << std::setprecision(6) << best_routing.routing_readiness_score
      << ",\n";
  out << "    \"best_slo_fit_scenario\": "
      << inferlab_plan::json_string(best_slo.scenario.name) << ",\n";
  out << "    \"best_slo_fit_score\": " << std::fixed
      << std::setprecision(6) << best_slo.slo_fit_score << ",\n";
  const auto& lowest_complexity =
      plan.records[plan.summary.lowest_complexity_index];
  out << "    \"lowest_complexity_scenario\": "
      << inferlab_plan::json_string(lowest_complexity.scenario.name)
      << ",\n";
  out << "    \"max_gpu_request\": " << max_gpu.total_gpu_request
      << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    if (index != 0U) {
      out << ",\n";
    }
    out << record_json(plan.records[index], 4);
  }
  out << "\n";
  out << "  ],\n";
  const auto release_resources =
      inferlab::kserve_llmd_plan::release_deployment_resource_binding(plan);
  out << "  \"release_deployment_resource_profile\": "
      << inferlab_release_deployment_resource::binding_json(
             release_resources, 2)
      << ",\n";
  out << "  \"not_run\": " << inferlab_plan::default_not_run_json(2)
      << "\n";
  out << "}\n";
  return out.str();
}

std::string deployment_report(
    const inferlab::kserve_llmd_plan::Plan& plan) {
  inferlab::kserve_llmd_plan::validate_plan(plan);
  std::ostringstream out;
  out << "# KServe/llm-d Deployment Plan\n\n";
  out << "Generated by the C++ native planner. This is a dry-run migration artifact and does not apply Kubernetes resources.\n\n";
  out << "## Scenario Matrix\n\n";
  out << "| Scenario | Backend | Mode | GPUs | Replicas | Routing | SLO Fit | Complexity |\n";
  out << "|---|---|---|---:|---:|---:|---:|---:|\n";
  for (const auto& record : plan.records) {
    out << "| " << record.scenario.name << " | "
        << record.scenario.backend << " | " << record.deployment_mode
        << " | " << record.total_gpu_request << " | "
        << record.total_replicas << " | " << std::fixed
        << std::setprecision(3) << record.routing_readiness_score << " | "
        << std::fixed << std::setprecision(3) << record.slo_fit_score
        << " | " << std::fixed << std::setprecision(3)
        << record.deployment_complexity_score << " |\n";
  }
  out << "\n## Field Mapping\n\n";
  for (const auto& mapping : field_metric_mapping()) {
    out << "- `" << mapping << "`\n";
  }
  out << "\n## Migration Commands\n\n";
  out << "Use these after moving the project to a Kubernetes environment with the relevant CRDs installed:\n\n";
  for (const auto& command : migration_commands(plan.records.front())) {
    out << "- `" << command << "`\n";
  }
  out << "\n## Docker Compose vs KServe/llm-d\n\n";
  out << "Use Docker compose for local smoke checks and single-node backend API validation. Use KServe/llm-d when the experiment needs KV-aware routing, prefill/decode separation, Gateway routing, autoscaling, or multi-node tensor parallel serving.\n";
  return out.str();
}

}  // namespace inferlab_kserve_view
