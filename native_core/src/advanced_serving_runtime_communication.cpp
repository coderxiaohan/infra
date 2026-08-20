#include "inferlab/advanced_serving_runtime_communication.hpp"

#include "advanced_serving_runtime_communication_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {
namespace {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_nonempty;
using advanced_serving_internal::require_positive_finite;
using advanced_serving_internal::require_unit_interval;
using runtime_communication_internal::transfer_milliseconds;

TensorParallelCommScenario validate_tensor_parallel_comm_scenario(
    TensorParallelCommScenario scenario) {
  require_nonempty(scenario.name,
                   "tensor-parallel scenario name");
  require_nonempty(scenario.backend,
                   "tensor-parallel backend");
  if (scenario.tensor_parallel_size <= 0 ||
      scenario.pipeline_parallel_size <= 0 ||
      scenario.batch_size <= 0 || scenario.hidden_size <= 0 ||
      scenario.sequence_tokens <= 0 || scenario.layers <= 0) {
    throw std::invalid_argument(
        "tensor-parallel topology and shape values must be positive");
  }
  scenario.activation_bytes = require_positive_finite(
      scenario.activation_bytes,
      "tensor-parallel activation bytes");
  scenario.overlap_ratio = require_unit_interval(
      scenario.overlap_ratio, "tensor-parallel overlap ratio");
  scenario.compute_ms_per_layer = require_positive_finite(
      scenario.compute_ms_per_layer,
      "tensor-parallel compute time per layer");

  const bool has_collective =
      scenario.tensor_parallel_size > 1 ||
      scenario.pipeline_parallel_size > 1;
  if (has_collective) {
    scenario.link_bandwidth_gbps = require_positive_finite(
        scenario.link_bandwidth_gbps,
        "tensor-parallel link bandwidth");
    scenario.collective_latency_us = require_positive_finite(
        scenario.collective_latency_us,
        "tensor-parallel collective latency");
  } else {
    scenario.link_bandwidth_gbps = require_non_negative_finite(
        scenario.link_bandwidth_gbps,
        "single-device reference link bandwidth");
    scenario.collective_latency_us = require_non_negative_finite(
        scenario.collective_latency_us,
        "single-device reference collective latency");
  }
  return scenario;
}

}  // namespace

TensorParallelCommResult estimate_tensor_parallel_comm(
    const TensorParallelCommScenario& input_scenario) {
  const TensorParallelCommScenario scenario =
      validate_tensor_parallel_comm_scenario(input_scenario);

  const std::size_t activation_element_count =
      internal::checked_product_size(
          scenario.batch_size, scenario.sequence_tokens,
          scenario.hidden_size,
          "tensor-parallel activation element cardinality");
  const double activation_bytes = finite_product(
      static_cast<double>(activation_element_count),
      scenario.activation_bytes,
      "tensor-parallel activation storage");
  const double activation_mb = require_finite(
      activation_bytes /
          runtime_communication_internal::kBytesPerMib,
      "tensor-parallel activation MiB per layer");
  const double ring_factor =
      scenario.tensor_parallel_size <= 1 ? 0.0 :
      require_finite(2.0 *
                         (static_cast<double>(
                              scenario.tensor_parallel_size - 1) /
                          static_cast<double>(
                              scenario.tensor_parallel_size)),
                     "tensor-parallel ring factor");
  const double all_reduce_mb = finite_product(
      activation_mb, ring_factor,
      "tensor-parallel all-reduce MiB");
  const double all_gather_factor =
      scenario.pipeline_parallel_size <= 1 ? 0.0 :
      require_finite(
          static_cast<double>(
              scenario.pipeline_parallel_size - 1) /
              static_cast<double>(
                  scenario.pipeline_parallel_size),
                     "pipeline-parallel gather factor");
  const double all_gather_mb = finite_product(
      activation_mb, all_gather_factor,
      "pipeline-parallel gather MiB");
  const int collective_peers = internal::checked_add_int(
      scenario.tensor_parallel_size - 1,
      scenario.pipeline_parallel_size - 1,
      "tensor-parallel collective peer count");
  const double collective_latency = require_finite(
      finite_product(
          static_cast<double>(collective_peers),
          scenario.collective_latency_us,
                     "tensor-parallel collective latency microseconds") /
          1000.0,
      "tensor-parallel collective latency milliseconds");
  const double transfer_mib = internal::checked_add_finite_double(
      all_reduce_mb, all_gather_mb,
      "tensor-parallel collective transfer MiB");
  const double transfer_gib = require_finite(
      transfer_mib / 1024.0,
      "tensor-parallel transfer GiB");
  const double serialization = transfer_milliseconds(
      transfer_gib, scenario.link_bandwidth_gbps,
      "tensor-parallel communication serialization time");
  const double comm_ms = internal::checked_add_finite_double(
      serialization, collective_latency,
      "tensor-parallel communication time per layer");
  const double exposed = finite_product(
      comm_ms, 1.0 - scenario.overlap_ratio,
      "tensor-parallel exposed communication time");
  const double compute_ms_all_layers = finite_product(
      scenario.compute_ms_per_layer,
      static_cast<double>(scenario.layers),
      "tensor-parallel total layer compute time");
  const double compute_total = require_finite(
      compute_ms_all_layers /
          static_cast<double>(scenario.pipeline_parallel_size),
      "tensor-parallel pipeline-stage compute time");
  const double exposed_all_layers = finite_product(
      exposed, static_cast<double>(scenario.layers),
      "tensor-parallel exposed communication across layers");
  const double ratio = require_finite(
      exposed_all_layers / compute_total,
      "tensor-parallel communication-to-compute ratio");
  const double topology_penalty = require_finite(
      0.92 -
          static_cast<double>(std::max(
              0, scenario.tensor_parallel_size - 8)) *
              0.025 -
          static_cast<double>(std::max(
              0, scenario.pipeline_parallel_size - 2)) *
              0.035,
      "tensor-parallel topology penalty");
  const double scaling =
      scenario.tensor_parallel_size == 1 &&
              scenario.pipeline_parallel_size == 1
          ? 1.0
          : clamp_unit_interval(require_finite(
                1.0 / (1.0 + ratio) * topology_penalty,
                "tensor-parallel scaling efficiency"));

  TensorParallelCommResult result;
  result.scenario = scenario;
  result.activation_mb_per_layer = activation_mb;
  result.all_reduce_mb = all_reduce_mb;
  result.all_gather_mb = all_gather_mb;
  result.collective_latency_ms = collective_latency;
  result.communication_ms_per_layer = comm_ms;
  result.exposed_comm_ms = exposed;
  result.compute_ms_total = compute_total;
  result.comm_to_compute_ratio = ratio;
  result.scaling_efficiency = scaling;
  result.parallelism_policy =
      scenario.tensor_parallel_size <= 1 &&
              scenario.pipeline_parallel_size <= 1
          ? "single_gpu_no_collective"
          : (scenario.tensor_parallel_size <= 1
                 ? "pipeline_parallel_stage_transfer"
                 : (scenario.link_bandwidth_gbps >= 200.0
                        ? "intra_node_tensor_parallel"
                        : "cross_node_pipeline_or_reduce_tp"));
  result.recommendation =
      scaling >= 0.70 ? "keep_parallel_layout_and_profile_nccl" :
      (ratio >= 0.45 ? "reduce_tensor_parallel_or_increase_overlap" :
       "rebucket_batch_shapes_before_scaling_out");
  return result;
}

std::vector<TensorParallelCommScenario>
default_tensor_parallel_comm_scenarios() {
  return {
      {"trtllm_tp8_nvlink", "tensorrt_llm", 8, 1, 16, 8192, 4096, 80, 2.0, 450.0, 12.0, 0.72, 1.35},
      {"vllm_tp4_pcie", "vllm", 4, 1, 12, 6144, 4096, 48, 2.0, 96.0, 35.0, 0.48, 1.15},
      {"sglang_tp2_pp2_rdma", "sglang", 2, 2, 8, 8192, 8192, 64, 2.0, 100.0, 55.0, 0.56, 1.60},
      {"kserve_llmd_cross_node_pp", "kserve_llm_d", 1, 4, 6, 8192, 16384, 80, 2.0, 50.0, 85.0, 0.40, 1.80},
      {"native_over_sharded_tp16", "native", 16, 1, 8, 4096, 2048, 32, 2.0, 64.0, 45.0, 0.35, 0.90},
      {"release_gpu8_tp4_pp1_comm", "native", 4, 1, 12, 6144, 4096,
       48, 2.0, 300.0, 18.0, 0.60, 1.15, 2, 8, 4, 4, 80.0},
  };
}

}  // namespace inferlab
