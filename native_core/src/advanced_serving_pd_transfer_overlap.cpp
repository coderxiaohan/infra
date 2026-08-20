#include "inferlab/advanced_serving_runtime_communication.hpp"

#include "inferlab/advanced_serving_scheduler.hpp"

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"
#include "pd_transfer_overlap_contract.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_non_negative_finite;
using advanced_serving_internal::require_positive_finite;

double checked_non_negative_difference(const double total,
                                       const double component,
                                       const char* name) {
  require_non_negative_finite(total, name);
  require_non_negative_finite(component, name);
  const double tolerance =
      std::max(1.0, std::max(total, component)) *
      pd_transfer_overlap_contract::kComparisonTolerance;
  if (component > total + tolerance) {
    throw std::logic_error(std::string(name) + " accounting underflow");
  }
  return total > component
             ? require_finite(total - component, name)
             : 0.0;
}

double checked_ratio(const double numerator,
                     const double denominator,
                     const char* name) {
  require_non_negative_finite(numerator, name);
  if (numerator == 0.0) {
    return 0.0;
  }
  require_positive_finite(denominator, name);
  return require_finite(numerator / denominator, name);
}

double scaled_chunk_duration(const double total_duration,
                             const int tokens,
                             const int total_tokens,
                             const char* name) {
  if (tokens == 0) {
    return 0.0;
  }
  if (total_tokens <= 0) {
    throw std::logic_error(
        std::string(name) + " requires a positive total token count");
  }
  return require_finite(
      finite_product(total_duration, static_cast<double>(tokens), name) /
          static_cast<double>(total_tokens),
      name);
}

PdTransferStage make_stage(const PdTransferStageKind kind,
                           const PdTransferResourceKind resource,
                           const int chunk_index,
                           const int token_count,
                           const double start_ms,
                           const double duration_ms) {
  (void)pd_transfer_stage_kind_name(kind);
  (void)pd_transfer_resource_kind_name(resource);
  if (chunk_index < 0 || token_count < 0) {
    throw std::logic_error("PD transfer stage count is negative");
  }
  PdTransferStage stage;
  stage.kind = kind;
  stage.resource = resource;
  stage.chunk_index = chunk_index;
  stage.token_count = token_count;
  stage.start_ms = require_non_negative_finite(
      start_ms, "PD transfer stage start");
  stage.duration_ms = require_non_negative_finite(
      duration_ms, "PD transfer stage duration");
  stage.end_ms = internal::checked_add_finite_double(
      stage.start_ms, stage.duration_ms, "PD transfer stage end");
  return stage;
}

PdTransferRecommendation select_recommendation(
    const PdTransferOverlapAssumptions& assumptions,
    const PdTransferOverlapResult& result) {
  if (!result.active_workload) {
    return PdTransferRecommendation::no_active_workload;
  }
  if (result.scenario.topology ==
      PdTransferTopology::aggregated_reference) {
    return PdTransferRecommendation::aggregated_reference_only;
  }
  if (!result.ranking_applicable) {
    return PdTransferRecommendation::prefer_simpler_aggregated_serving;
  }
  const double decode_stall_ratio = checked_ratio(
      result.decode_stall_ms, result.decode_compute_ms,
      "PD transfer decode-stall ratio");
  if (decode_stall_ratio >
      assumptions.high_decode_stall_to_decode_ratio_threshold) {
    return PdTransferRecommendation::
        increase_transfer_bandwidth_or_prestage_kv;
  }
  if (result.transfer_link_busy_ratio >=
          assumptions.high_transfer_link_busy_ratio_threshold &&
      result.transfer_exposed_ms > 0.0) {
    return PdTransferRecommendation::transfer_fabric_is_saturated;
  }
  if (result.speedup_vs_aggregated >=
      assumptions.pd_speedup_vs_aggregated_threshold) {
    return PdTransferRecommendation::pd_split_is_promising;
  }
  return PdTransferRecommendation::prefer_simpler_aggregated_serving;
}

}  // namespace

const char* pd_transfer_topology_name(const PdTransferTopology topology) {
  switch (topology) {
    case PdTransferTopology::aggregated_reference:
      return "aggregated_reference";
    case PdTransferTopology::prefill_decode_split:
      return "prefill_decode_split";
  }
  throw std::invalid_argument("unknown PD transfer topology");
}

const char* pd_transfer_stage_kind_name(const PdTransferStageKind kind) {
  switch (kind) {
    case PdTransferStageKind::prefill_chunk:
      return "prefill_chunk";
    case PdTransferStageKind::kv_send_recv:
      return "kv_send_recv";
    case PdTransferStageKind::decode_chunk:
      return "decode_chunk";
  }
  throw std::invalid_argument("unknown PD transfer stage kind");
}

const char* pd_transfer_resource_kind_name(
    const PdTransferResourceKind kind) {
  switch (kind) {
    case PdTransferResourceKind::prefill_worker:
      return "prefill_worker";
    case PdTransferResourceKind::transfer_link:
      return "transfer_link";
    case PdTransferResourceKind::decode_worker:
      return "decode_worker";
  }
  throw std::invalid_argument("unknown PD transfer resource kind");
}

const char* pd_transfer_recommendation_text(
    const PdTransferRecommendation recommendation) {
  switch (recommendation) {
    case PdTransferRecommendation::no_active_workload:
      return "no active workload";
    case PdTransferRecommendation::aggregated_reference_only:
      return "aggregated reference; no KV transfer is modeled";
    case PdTransferRecommendation::
        increase_transfer_bandwidth_or_prestage_kv:
      return "increase transfer bandwidth or pre-stage KV chunks";
    case PdTransferRecommendation::transfer_fabric_is_saturated:
      return "transfer fabric is saturated; add links or compress KV";
    case PdTransferRecommendation::pd_split_is_promising:
      return "PD split is promising for this compiled workload";
    case PdTransferRecommendation::prefer_simpler_aggregated_serving:
      return "benefit is modest; prefer simpler aggregated serving";
  }
  throw std::invalid_argument("unknown PD transfer recommendation");
}

const char* pd_transfer_recommendation_name(
    const PdTransferRecommendation recommendation) {
  switch (recommendation) {
    case PdTransferRecommendation::no_active_workload:
      return "no_active_workload";
    case PdTransferRecommendation::aggregated_reference_only:
      return "aggregated_reference_only";
    case PdTransferRecommendation::
        increase_transfer_bandwidth_or_prestage_kv:
      return "increase_transfer_bandwidth_or_prestage_kv";
    case PdTransferRecommendation::transfer_fabric_is_saturated:
      return "transfer_fabric_is_saturated";
    case PdTransferRecommendation::pd_split_is_promising:
      return "pd_split_is_promising";
    case PdTransferRecommendation::prefer_simpler_aggregated_serving:
      return "prefer_simpler_aggregated_serving";
  }
  throw std::invalid_argument("unknown PD transfer recommendation");
}

PdTransferOverlapAssumptions
default_pd_transfer_overlap_assumptions() {
  return {};
}

std::vector<PdTransferOverlapScenario>
default_pd_transfer_overlap_scenarios() {
  return {
      {"single_gpu_aggregated_reference",
       PdTransferTopology::aggregated_reference,
       8192, 256, 2048, 32, 1, 1, 0.08, 1.20, 32768.0,
       0.0, 0.0, 0.0, 0.0},
      {"pd_nvlink_overlap", PdTransferTopology::prefill_decode_split,
       8192, 256, 2048, 32, 1, 1, 0.08, 1.20, 32768.0,
       300.0, 25.0, 15.0, 0.82},
      {"pd_pcie_partial_overlap",
       PdTransferTopology::prefill_decode_split,
       16384, 512, 2048, 32, 2, 2, 0.08, 1.20, 32768.0,
       64.0, 45.0, 30.0, 0.62},
      {"pd_rdma_remote_decode",
       PdTransferTopology::prefill_decode_split,
       32768, 512, 4096, 32, 2, 4, 0.08, 1.20, 32768.0,
       100.0, 70.0, 55.0, 0.55},
  };
}

PdTransferOverlapResult estimate_pd_transfer_overlap(
    const PdTransferOverlapAssumptions& assumptions,
    const PdTransferOverlapScenario& input_scenario) {
  pd_transfer_overlap_contract::validate_assumptions(assumptions);
  const PdTransferOverlapScenario scenario =
      pd_transfer_overlap_contract::validate_scenario(
          input_scenario);

  const bool split =
      scenario.topology == PdTransferTopology::prefill_decode_split;
  const bool transfer_required =
      split && scenario.prompt_tokens > 0 &&
      scenario.kv_bytes_per_token > 0.0;

  DisaggregatedScenario compute_scenario;
  compute_scenario.prefill_workers = scenario.prefill_workers;
  compute_scenario.decode_workers = scenario.decode_workers;
  compute_scenario.prompt_tokens = scenario.prompt_tokens;
  compute_scenario.output_tokens = scenario.output_tokens;
  compute_scenario.arrival_qps = 0.0;
  compute_scenario.bandwidth_gbps =
      transfer_required ? scenario.link_bandwidth_gbps : 0.0;
  DisaggregatedConstants compute_constants;
  compute_constants.prefill_ms_per_token =
      scenario.prefill_ms_per_token;
  compute_constants.decode_ms_per_token = scenario.decode_ms_per_token;
  compute_constants.kv_bytes_per_token =
      transfer_required ? scenario.kv_bytes_per_token : 0.0;
  const DisaggregatedResult compute = estimate_disaggregated_serving(
      compute_scenario, compute_constants);

  PdTransferOverlapResult result;
  result.scenario = scenario;
  result.active_workload =
      scenario.prompt_tokens > 0 || scenario.output_tokens > 0;
  result.transfer_required = transfer_required;
  result.ranking_applicable =
      transfer_required && scenario.output_tokens > 0;
  result.prefill_chunks =
      scenario.prompt_tokens == 0
          ? 0
          : internal::checked_ceil_div_non_negative_int(
                scenario.prompt_tokens, scenario.prefill_chunk_tokens,
                "PD transfer prefill chunk count");
  result.decode_chunks =
      scenario.output_tokens == 0
          ? 0
          : internal::checked_ceil_div_non_negative_int(
                scenario.output_tokens, scenario.decode_chunk_tokens,
                "PD transfer decode chunk count");
  result.prompt_kv_token_count =
      static_cast<std::int64_t>(scenario.prompt_tokens);
  result.prompt_kv_bytes = finite_product(
      static_cast<double>(scenario.prompt_tokens),
      scenario.kv_bytes_per_token, "PD transfer prompt KV bytes");
  result.prompt_kv_gib = runtime_communication_internal::bytes_to_gib(
      result.prompt_kv_bytes, "PD transfer prompt KV GiB");
  result.prefill_compute_ms = compute.prefill_ms;
  result.decode_compute_ms = compute.decode_ms;
  result.aggregated_baseline_ms = compute.aggregate_baseline_ms;

  double prefill_cursor = 0.0;
  double transfer_cursor = 0.0;
  double first_transfer_start = 0.0;
  bool has_transfer_stage = false;
  for (int index = 0; index < result.prefill_chunks; ++index) {
    const int tokens =
        pd_transfer_overlap_contract::chunk_tokens_at(
            scenario.prompt_tokens,
            scenario.prefill_chunk_tokens, index,
            "PD transfer prefill chunk");
    const double duration = scaled_chunk_duration(
        result.prefill_compute_ms, tokens, scenario.prompt_tokens,
        "PD transfer prefill chunk duration");
    const double start = prefill_cursor;
    const PdTransferStage prefill_stage = make_stage(
        PdTransferStageKind::prefill_chunk,
        PdTransferResourceKind::prefill_worker, index, tokens, start,
        duration);
    prefill_cursor = prefill_stage.end_ms;
    result.stages.push_back(prefill_stage);

    if (!transfer_required) {
      continue;
    }
    const double chunk_bytes = finite_product(
        static_cast<double>(tokens), scenario.kv_bytes_per_token,
        "PD transfer chunk KV bytes");
    const double payload_ms =
        runtime_communication_internal::transfer_milliseconds_from_bytes(
            chunk_bytes, scenario.link_bandwidth_gbps,
            "PD transfer chunk payload serialization");
    const double overhead_us = internal::checked_add_finite_double(
        scenario.serialization_us_per_chunk,
        scenario.receiver_setup_us_per_chunk,
        "PD transfer chunk sender/receiver overhead microseconds");
    const double overhead_ms = require_finite(
        overhead_us / 1000.0,
        "PD transfer chunk sender/receiver overhead milliseconds");
    const double transfer_duration = internal::checked_add_finite_double(
        payload_ms, overhead_ms, "PD transfer chunk service time");
    const double ready_delay = finite_product(
        duration, 1.0 - scenario.overlap_efficiency,
        "PD transfer chunk streaming-ready delay");
    const double ready_time = internal::checked_add_finite_double(
        start, ready_delay, "PD transfer chunk streaming-ready time");
    const double transfer_start = std::max(transfer_cursor, ready_time);
    const PdTransferStage transfer_stage = make_stage(
        PdTransferStageKind::kv_send_recv,
        PdTransferResourceKind::transfer_link, index, tokens,
        transfer_start, transfer_duration);
    if (!has_transfer_stage) {
      first_transfer_start = transfer_stage.start_ms;
      has_transfer_stage = true;
    }
    transfer_cursor = transfer_stage.end_ms;
    result.transfer_payload_serialization_ms =
        internal::checked_add_finite_double(
            result.transfer_payload_serialization_ms, payload_ms,
            "PD transfer payload service total");
    result.transfer_overhead_ms = internal::checked_add_finite_double(
        result.transfer_overhead_ms, overhead_ms,
        "PD transfer overhead total");
    result.kv_transfer_total_ms = internal::checked_add_finite_double(
        result.kv_transfer_total_ms, transfer_duration,
        "PD transfer service total");
    result.stages.push_back(transfer_stage);
  }

  if (has_transfer_stage) {
    result.transfer_pipeline_span_ms =
        checked_non_negative_difference(
            transfer_cursor, first_transfer_start,
            "PD transfer pipeline span");
    result.transfer_link_busy_ratio = checked_ratio(
        result.kv_transfer_total_ms, result.transfer_pipeline_span_ms,
        "PD transfer link busy ratio");
    if (result.transfer_link_busy_ratio >
        1.0 +
            pd_transfer_overlap_contract::kComparisonTolerance) {
      throw std::logic_error(
          "PD transfer link busy ratio exceeds serialized pipeline capacity");
    }
    if (result.transfer_link_busy_ratio > 1.0) {
      result.transfer_link_busy_ratio = 1.0;
    }
    if (std::fabs(result.transfer_payload_serialization_ms -
                  compute.kv_transfer_ms) >
        std::max(1.0, compute.kv_transfer_ms) *
            pd_transfer_overlap_contract::kComparisonTolerance) {
      throw std::logic_error(
          "PD transfer chunk payload total drifted from the canonical disaggregated estimate");
    }
  }

  result.full_prompt_kv_ready_ms = transfer_required
      ? std::max(prefill_cursor, transfer_cursor)
      : prefill_cursor;
  result.transfer_exposed_ms = transfer_required
      ? checked_non_negative_difference(
            result.full_prompt_kv_ready_ms, prefill_cursor,
            "PD transfer exposed tail")
      : 0.0;
  result.transfer_hidden_ms = transfer_required
      ? checked_non_negative_difference(
            result.kv_transfer_total_ms, result.transfer_exposed_ms,
            "PD transfer hidden service time")
      : 0.0;
  result.decode_stall_ms = scenario.output_tokens > 0
      ? result.transfer_exposed_ms
      : 0.0;

  double decode_cursor = result.full_prompt_kv_ready_ms;
  for (int index = 0; index < result.decode_chunks; ++index) {
    const int tokens =
        pd_transfer_overlap_contract::chunk_tokens_at(
            scenario.output_tokens,
            scenario.decode_chunk_tokens, index,
            "PD transfer decode chunk");
    const double duration = scaled_chunk_duration(
        result.decode_compute_ms, tokens, scenario.output_tokens,
        "PD transfer decode chunk duration");
    const PdTransferStage stage = make_stage(
        PdTransferStageKind::decode_chunk,
        PdTransferResourceKind::decode_worker, index, tokens,
        decode_cursor, duration);
    decode_cursor = stage.end_ms;
    result.stages.push_back(stage);
  }

  result.disaggregated_no_overlap_ms = transfer_required
      ? internal::checked_add_finite_double(
            internal::checked_add_finite_double(
                result.prefill_compute_ms, result.kv_transfer_total_ms,
                "PD transfer no-overlap prefill and transfer"),
            result.decode_compute_ms,
            "PD transfer no-overlap end-to-end")
      : result.aggregated_baseline_ms;
  result.overlapped_end_to_end_ms = scenario.output_tokens > 0
      ? decode_cursor
      : result.full_prompt_kv_ready_ms;
  result.ttft_ms = scenario.output_tokens > 0
      ? result.full_prompt_kv_ready_ms
      : 0.0;
  result.tpot_ms = scenario.output_tokens > 0
      ? checked_ratio(result.decode_compute_ms,
                      static_cast<double>(scenario.output_tokens),
                      "PD transfer TPOT")
      : 0.0;
  result.overlap_gain_ms = checked_non_negative_difference(
      result.disaggregated_no_overlap_ms,
      result.overlapped_end_to_end_ms,
      "PD transfer overlap gain");
  result.speedup_vs_no_overlap = result.overlapped_end_to_end_ms > 0.0
      ? checked_ratio(result.disaggregated_no_overlap_ms,
                      result.overlapped_end_to_end_ms,
                      "PD transfer speedup versus no overlap")
      : 0.0;
  result.speedup_vs_aggregated = result.overlapped_end_to_end_ms > 0.0
      ? checked_ratio(result.aggregated_baseline_ms,
                      result.overlapped_end_to_end_ms,
                      "PD transfer speedup versus aggregated")
      : 0.0;
  result.fabric_utilization_score = result.transfer_link_busy_ratio;
  result.recommendation = select_recommendation(assumptions, result);
  pd_transfer_overlap_contract::validate_result(result);
  return result;
}

PdTransferOverlapPlan build_pd_transfer_overlap_plan(
    const PdTransferOverlapAssumptions& assumptions,
    const std::vector<PdTransferOverlapScenario>& scenarios) {
  pd_transfer_overlap_contract::validate_assumptions(
      assumptions);
  if (scenarios.empty()) {
    throw std::invalid_argument(
        "PD transfer scenario inventory must not be empty");
  }

  std::vector<std::string> scenario_names;
  scenario_names.reserve(scenarios.size());
  PdTransferOverlapPlan plan;
  plan.assumptions = assumptions;
  plan.scenarios.reserve(scenarios.size());
  plan.records.reserve(scenarios.size());
  for (const auto& input : scenarios) {
    const PdTransferOverlapScenario scenario =
        pd_transfer_overlap_contract::validate_scenario(input);
    scenario_names.push_back(scenario.name);
    plan.scenarios.push_back(scenario);
    plan.records.push_back(
        estimate_pd_transfer_overlap(assumptions, scenario));
  }
  advanced_serving_internal::require_unique_nonempty_strings(
      scenario_names, "PD transfer scenario identities");
  if (plan.scenarios.size() != plan.records.size()) {
    throw std::logic_error("PD transfer scenario/record cardinality drift");
  }

  for (std::size_t index = 0; index < plan.records.size(); ++index) {
    const auto& record = plan.records[index];
    if (record.active_workload) {
      plan.active_record_count = internal::checked_add_size(
          plan.active_record_count, 1U,
          "PD transfer active record count");
    }
    if (!record.ranking_applicable) {
      continue;
    }
    plan.comparable_record_count = internal::checked_add_size(
        plan.comparable_record_count, 1U,
        "PD transfer comparable record count");
    if (!plan.highest_overlap_gain_record_index.has_value() ||
        record.overlap_gain_ms >
            plan.records[*plan.highest_overlap_gain_record_index]
                .overlap_gain_ms) {
      plan.highest_overlap_gain_record_index = index;
    }
    if (!plan.highest_decode_stall_record_index.has_value() ||
        record.decode_stall_ms >
            plan.records[*plan.highest_decode_stall_record_index]
                .decode_stall_ms) {
      plan.highest_decode_stall_record_index = index;
    }
    if (!plan.highest_pd_speedup_record_index.has_value() ||
        record.speedup_vs_aggregated >
            plan.records[*plan.highest_pd_speedup_record_index]
                .speedup_vs_aggregated) {
      plan.highest_pd_speedup_record_index = index;
    }
  }
  return plan;
}

}  // namespace inferlab
