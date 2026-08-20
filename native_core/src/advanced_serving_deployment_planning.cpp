#include "inferlab/advanced_serving_deployment_planning.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "advanced_serving_backend_internal.hpp"
#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {

KserveLlmdManifestResult estimate_kserve_llmd_manifest(
    const KserveLlmdManifestScenario& scenario) {
  KserveLlmdManifestScenario normalized = scenario;
  normalized.name = advanced_serving_backend_internal::normalized_identifier(
      scenario.name, "kserve_llmd", "KServe llm-d scenario name");
  normalized.backend =
      advanced_serving_backend_internal::normalized_identifier(
          scenario.backend, "kserve_llm_d", "KServe llm-d backend");
  normalized.model_uri =
      advanced_serving_backend_internal::normalized_resource_token(
          scenario.model_uri, "pvc://models/model", "KServe model URI");
  normalized.decode_replicas = std::max(0, scenario.decode_replicas);
  normalized.prefill_replicas = std::max(0, scenario.prefill_replicas);
  normalized.tensor_parallel_size =
      std::max(0, scenario.tensor_parallel_size);
  normalized.gpu_per_replica = std::max(0, scenario.gpu_per_replica);
  normalized.pipeline_parallel_size =
      std::max(0, scenario.pipeline_parallel_size);
  normalized.accelerators_per_node =
      std::max(0, scenario.accelerators_per_node);
  normalized.accelerator_memory_gib =
      advanced_serving_internal::non_negative_finite_or_zero(
          scenario.accelerator_memory_gib,
          "KServe accelerator memory GiB");
  normalized.target_qps =
      advanced_serving_internal::non_negative_finite_or_zero(
          scenario.target_qps, "KServe target QPS");
  normalized.target_ttft_ms =
      advanced_serving_internal::require_positive_finite(
          scenario.target_ttft_ms, "KServe target TTFT");
  normalized.target_tpot_ms =
      advanced_serving_internal::require_positive_finite(
          scenario.target_tpot_ms, "KServe target TPOT");
  if (scenario.disaggregated_prefill && normalized.prefill_replicas == 0) {
    throw std::invalid_argument(
        "KServe disaggregated prefill requires a prefill replica");
  }
  normalized.disaggregated_prefill = normalized.prefill_replicas > 0;

  const bool disaggregated = normalized.disaggregated_prefill;
  const int total_replicas = internal::checked_add_int(
      normalized.decode_replicas,
      disaggregated ? normalized.prefill_replicas : 0,
      "KServe llm-d total replicas");
  const int total_gpu = internal::checked_multiply_int(
      internal::checked_multiply_int(total_replicas,
                                     normalized.gpu_per_replica,
                                     "KServe llm-d total GPU request"),
      internal::checked_multiply_int(
          normalized.tensor_parallel_size,
          normalized.pipeline_parallel_size,
          "KServe llm-d parallel width"),
      "KServe llm-d total GPU request");
  const bool serving_topology = normalized.decode_replicas > 0 &&
                                normalized.gpu_per_replica > 0 &&
                                normalized.tensor_parallel_size > 0 &&
                                normalized.pipeline_parallel_size > 0;
  const double routing_base =
      (normalized.kv_cache_aware_routing ? 0.45 : 0.15) +
      (disaggregated ? 0.35 : 0.10) +
      (normalized.decode_replicas >= 2 ? 0.15 : 0.05) +
      (normalized.tensor_parallel_size >= 2 ? 0.05 : 0.0);
  const double routing_score =
      serving_topology
          ? advanced_serving_internal::finite_unit_interval(
                routing_base, "KServe routing readiness score")
          : 0.0;
  const double capacity_hint = advanced_serving_internal::require_finite(
      advanced_serving_internal::finite_product(
          static_cast<double>(normalized.decode_replicas), 12.0,
          "KServe decode capacity hint") +
          advanced_serving_internal::finite_product(
              static_cast<double>(normalized.prefill_replicas), 8.0,
              "KServe prefill capacity hint"),
      "KServe aggregate capacity hint");
  const double ttft_fit = std::min(
      1.0, advanced_serving_internal::require_finite(
               800.0 / normalized.target_ttft_ms,
               "KServe TTFT fit ratio"));
  const double tpot_fit = std::min(
      1.0, advanced_serving_internal::require_finite(
               60.0 / normalized.target_tpot_ms,
               "KServe TPOT fit ratio"));
  const double qps_fit =
      normalized.target_qps == 0.0
          ? 0.0
          : (serving_topology
                 ? std::min(
                       1.0, advanced_serving_internal::require_finite(
                                capacity_hint / normalized.target_qps,
                                "KServe QPS fit ratio"))
                 : 0.0);
  const double slo_fit =
      normalized.target_qps == 0.0
          ? 0.0
          : advanced_serving_internal::finite_unit_interval(
                qps_fit * 0.45 + ttft_fit * 0.30 + tpot_fit * 0.25,
                "KServe SLO fit score");
  const double complexity =
      total_replicas == 0
          ? 0.0
          : advanced_serving_internal::finite_unit_interval(
                0.12 + static_cast<double>(total_gpu) * 0.025 +
                    (disaggregated ? 0.22 : 0.0) +
                    (normalized.kv_cache_aware_routing ? 0.14 : 0.0) +
                    static_cast<double>(
                        std::max(0, normalized.tensor_parallel_size - 1)) *
                        0.04,
                "KServe deployment complexity score");

  KserveLlmdManifestResult result;
  result.scenario = std::move(normalized);
  result.total_gpu_request = total_gpu;
  result.total_replicas = total_replicas;
  result.routing_readiness_score = routing_score;
  result.slo_fit_score = slo_fit;
  result.deployment_complexity_score = complexity;
  result.service_kind =
      disaggregated ? "LLMInferenceService_prefill_decode"
                    : "LLMInferenceService_single_pool";
  result.gateway_policy =
      result.scenario.kv_cache_aware_routing
          ? "kv_cache_aware_gateway_routing"
          : "least_loaded_gateway_routing";
  result.worker_topology =
      disaggregated ? "separate_prefill_and_decode_worker_pools"
                    : "colocated_prefill_decode_workers";
  result.yaml_profile =
      disaggregated ? "deploy/kserve_llm_d.example.yaml#prefill-decode"
                    : "deploy/kserve_llm_d.example.yaml#single-pool";
  result.deployment_mode =
      total_replicas == 0
          ? "no_worker_pool"
          : (disaggregated
                 ? "prefill_decode_split"
                 : (result.scenario.kv_cache_aware_routing
                        ? "kv_cache_aware_decode_pool"
                        : (result.scenario.tensor_parallel_size > 1
                               ? "multi_node_tensor_parallel"
                               : "single_pool")));
  result.docker_compose_guidance =
      total_replicas == 0
          ? "configure a worker pool before selecting a deployment runtime"
          : (total_gpu <= 2 && !disaggregated
                 ? "prefer docker compose for local smoke, single-node reproduction, and backend API validation"
                 : "prefer KServe/llm-d because routing, autoscaling, or multi-worker topology is part of the experiment");
  result.recommendation =
      result.scenario.decode_replicas == 0
          ? "configure_decode_replicas"
          : (!serving_topology
                 ? "configure_tensor_parallel_and_gpu_inventory"
                 : (result.scenario.target_qps == 0.0
                        ? "configure_nonzero_target_qps_before_capacity_claim"
                        : (slo_fit < 0.55
                               ? "increase_replicas_or_lower_slo_target"
                               : (complexity > 0.70
                                      ? "stage_rollout_with_single_pool_then_split"
                                      : "render_manifest_and_apply_on_kserve_cluster"))));
  return result;
}

std::vector<KserveLlmdManifestScenario>
default_kserve_llmd_manifest_scenarios() {
  return {
      {"single_pool_vllm", "kserve_llm_d", "pvc://models/llama-8b", 3,
       0, 1, 1, false, false, 28.0, 900.0, 70.0},
      {"kv_aware_decode_pool", "kserve_llm_d", "pvc://models/llama-70b",
       6, 0, 4, 1, true, false, 42.0, 850.0, 55.0},
      {"prefill_decode_split", "kserve_llm_d", "s3://models/mixtral", 5,
       2, 2, 1, true, true, 64.0, 650.0, 45.0},
      {"dynamo_style_gateway", "nvidia_dynamo",
       "pvc://models/long-context", 8, 3, 2, 1, true, true, 90.0,
       700.0, 50.0},
      {"small_team_manifest", "kserve_llm_d", "pvc://models/dev", 1, 0,
       1, 1, false, false, 4.0, 1200.0, 90.0},
      {"release_gpu8_tp4_pp1_manifest", "kserve_llm_d",
       "pvc://models/release-candidate", 2, 0, 4, 1, true, false,
       24.0, 800.0, 60.0, 1, 4, 80.0},
  };
}

}  // namespace inferlab
