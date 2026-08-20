#include "inferlab/advanced_serving_scheduler.hpp"

#include <algorithm>
#include <stdexcept>
#include <vector>

#include "advanced_serving_internal.hpp"
#include "advanced_serving_runtime_communication_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::finite_unit_interval;
using advanced_serving_internal::non_negative_finite_or_zero;
using advanced_serving_internal::positive_or_one;
using advanced_serving_internal::require_finite;
using advanced_serving_internal::require_positive_finite;

DisaggregatedResult estimate_disaggregated_serving(
    const DisaggregatedScenario& scenario,
    const DisaggregatedConstants& constants) {
  const int prefill_workers = std::max(0, scenario.prefill_workers);
  const int decode_workers = std::max(0, scenario.decode_workers);
  const int prompt_tokens = std::max(0, scenario.prompt_tokens);
  const int output_tokens = std::max(0, scenario.output_tokens);
  const double arrival_qps = non_negative_finite_or_zero(
      scenario.arrival_qps, "disaggregated arrival QPS");
  const double bandwidth_gbps = non_negative_finite_or_zero(
      scenario.bandwidth_gbps, "disaggregated bandwidth");
  const double prefill_ms_per_token =
      non_negative_finite_or_zero(constants.prefill_ms_per_token,
                                  "disaggregated prefill milliseconds per token");
  const double decode_ms_per_token =
      non_negative_finite_or_zero(constants.decode_ms_per_token,
                                  "disaggregated decode milliseconds per token");
  const double kv_bytes_per_token =
      non_negative_finite_or_zero(constants.kv_bytes_per_token,
                                  "disaggregated KV bytes per token");
  if (prompt_tokens > 0 && prefill_workers == 0) {
    throw std::invalid_argument(
        "disaggregated prefill workers must be positive when prompt work exists");
  }
  if (output_tokens > 0 && decode_workers == 0) {
    throw std::invalid_argument(
        "disaggregated decode workers must be positive when decode work exists");
  }
  if (prompt_tokens > 0 && kv_bytes_per_token > 0.0 &&
      bandwidth_gbps <= 0.0) {
    throw std::invalid_argument(
        "disaggregated bandwidth must be positive when KV transfer exists");
  }

  DisaggregatedResult result;
  result.scenario = scenario;
  result.scenario.prefill_workers = prefill_workers;
  result.scenario.decode_workers = decode_workers;
  result.scenario.prompt_tokens = prompt_tokens;
  result.scenario.output_tokens = output_tokens;
  result.scenario.arrival_qps = arrival_qps;
  result.scenario.bandwidth_gbps = bandwidth_gbps;

  result.prefill_ms =
      prompt_tokens == 0
          ? 0.0
          : require_finite(
                (static_cast<double>(prompt_tokens) * prefill_ms_per_token) /
                    static_cast<double>(prefill_workers),
                "disaggregated prefill latency");
  result.decode_ms =
      output_tokens == 0
          ? 0.0
          : require_finite(
                (static_cast<double>(output_tokens) * decode_ms_per_token) /
                    static_cast<double>(decode_workers),
                "disaggregated decode latency");
  const double kv_transfer_bytes = finite_product(
      static_cast<double>(prompt_tokens), kv_bytes_per_token,
      "disaggregated KV transfer bytes");
  result.kv_transfer_ms =
      runtime_communication_internal::transfer_milliseconds_from_bytes(
          kv_transfer_bytes, bandwidth_gbps,
          "disaggregated KV transfer latency");
  result.queue_penalty_ms =
      prompt_tokens == 0
          ? 0.0
          : require_finite(
                std::max(0.0,
                         arrival_qps - static_cast<double>(prefill_workers)) *
                    5.0,
                "disaggregated queue penalty");
  result.ttft_ms = require_finite(
      result.prefill_ms + result.kv_transfer_ms + result.queue_penalty_ms,
      "disaggregated TTFT");
  result.tpot_ms =
      output_tokens == 0
          ? 0.0
          : require_finite(result.decode_ms /
                               static_cast<double>(output_tokens),
                           "disaggregated TPOT");
  result.end_to_end_ms = require_finite(
      result.ttft_ms + result.decode_ms, "disaggregated end-to-end latency");
  result.aggregate_baseline_ms = require_finite(
      static_cast<double>(prompt_tokens) * prefill_ms_per_token +
          static_cast<double>(output_tokens) * decode_ms_per_token,
      "disaggregated aggregate baseline");
  result.speedup_vs_aggregate = require_finite(
      result.end_to_end_ms <= 0.0
          ? 0.0
          : result.aggregate_baseline_ms / result.end_to_end_ms,
      "disaggregated aggregate speedup");
  return result;
}

SloCapacityResult estimate_slo_capacity(const SloScenario& scenario,
                                        const SloConstants& constants) {
  const double qps =
      non_negative_finite_or_zero(scenario.qps, "SLO request QPS");
  const int prompt_tokens = std::max(0, scenario.prompt_tokens);
  const int output_tokens = std::max(0, scenario.output_tokens);
  const double ttft_slo_ms = require_positive_finite(
      constants.ttft_slo_ms, "SLO TTFT target milliseconds");
  const double tpot_slo_ms = require_positive_finite(
      constants.tpot_slo_ms, "SLO TPOT target milliseconds");
  const double gpu_hour_cost = non_negative_finite_or_zero(
      constants.gpu_hour_cost, "SLO GPU hourly cost");
  const double decode_tps = require_positive_finite(
      constants.decode_tokens_per_second_per_gpu,
      "SLO decode tokens per second per GPU");
  const double prefill_ms_per_token =
      non_negative_finite_or_zero(constants.prefill_ms_per_token,
                                  "SLO prefill milliseconds per token");

  SloCapacityResult result;
  result.scenario = scenario;
  result.scenario.qps = qps;
  result.scenario.prompt_tokens = prompt_tokens;
  result.scenario.output_tokens = output_tokens;
  result.prefill_gpu_ms_per_request = require_finite(
      static_cast<double>(prompt_tokens) * prefill_ms_per_token,
      "SLO prefill GPU milliseconds per request");
  result.decode_tokens_per_second_required = finite_product(
      qps, static_cast<double>(output_tokens),
      "SLO required decode tokens per second");
  const double prefill_gpu_demand = finite_product(
      qps, result.prefill_gpu_ms_per_request, "SLO prefill GPU demand");
  result.prefill_replicas = internal::checked_non_negative_ceil_double_to_int(
      prefill_gpu_demand / ttft_slo_ms, "SLO prefill replicas");
  result.decode_replicas = internal::checked_non_negative_ceil_double_to_int(
      result.decode_tokens_per_second_required / decode_tps,
      "SLO decode replicas");
  result.required_replicas =
      std::max(result.prefill_replicas, result.decode_replicas);
  result.estimated_hourly_cost = require_finite(
      static_cast<double>(result.required_replicas) * gpu_hour_cost,
      "SLO estimated hourly cost");
  result.estimated_daily_cost = require_finite(
      result.estimated_hourly_cost * 24.0, "SLO estimated daily cost");
  result.replica_utilization =
      result.required_replicas == 0
          ? 0.0
          : require_finite(
                result.decode_tokens_per_second_required /
                    (static_cast<double>(result.required_replicas) *
                     decode_tps),
                "SLO replica utilization");
  const bool prefill_work = qps > 0.0 && prompt_tokens > 0;
  const bool decode_work = qps > 0.0 && output_tokens > 0;
  result.ttft_slo_feasible =
      !prefill_work ||
      (result.required_replicas > 0 &&
       result.prefill_gpu_ms_per_request / result.required_replicas <=
           ttft_slo_ms);
  result.tpot_slo_feasible =
      !decode_work ||
      require_finite(1000.0 / decode_tps, "SLO single-token TPOT") <=
          tpot_slo_ms;
  return result;
}

LongContextResult estimate_long_context(
    const LongContextScenario& scenario) {
  const int prompt_tokens = std::max(0, scenario.prompt_tokens);
  const int output_tokens = std::max(0, scenario.output_tokens);
  const int chunk_size = positive_or_one(scenario.chunk_size);
  const double prefix_cache_hit_rate =
      finite_unit_interval(scenario.prefix_cache_hit_rate,
                           "long context prefix cache hit rate");
  const double prefill_ms_per_token =
      non_negative_finite_or_zero(scenario.prefill_ms_per_token,
                                  "long context prefill milliseconds per token");
  const double decode_ms_per_token =
      non_negative_finite_or_zero(scenario.decode_ms_per_token,
                                  "long context decode milliseconds per token");
  const double kv_bytes_per_token =
      non_negative_finite_or_zero(scenario.kv_bytes_per_token,
                                  "long context KV bytes per token");
  const int cached_tokens = internal::checked_non_negative_floor_double_to_int(
      finite_product(static_cast<double>(prompt_tokens), prefix_cache_hit_rate,
                     "long context cached token estimate"),
      "long context cached tokens");
  const int effective_prompt_tokens = prompt_tokens - cached_tokens;
  const int chunk_count = internal::checked_ceil_div_non_negative_int(
      effective_prompt_tokens, chunk_size, "long context chunk count");
  const double scheduling_overhead_ms = require_finite(
      static_cast<double>(std::max(0, chunk_count - 1)) * 2.5,
      "long context scheduling overhead");
  const int total_sequence_tokens = internal::checked_add_int(
      prompt_tokens, output_tokens, "long context total sequence tokens");

  LongContextResult result;
  result.scenario = scenario;
  result.scenario.prompt_tokens = prompt_tokens;
  result.scenario.output_tokens = output_tokens;
  result.scenario.chunk_size = chunk_size;
  result.scenario.prefix_cache_hit_rate = prefix_cache_hit_rate;
  result.scenario.prefill_ms_per_token = prefill_ms_per_token;
  result.scenario.decode_ms_per_token = decode_ms_per_token;
  result.scenario.kv_bytes_per_token = kv_bytes_per_token;
  result.chunk_count = chunk_count;
  result.full_prefill_ms = require_finite(
      static_cast<double>(prompt_tokens) * prefill_ms_per_token,
      "long context full prefill latency");
  result.prefix_cache_saved_ms = require_finite(
      static_cast<double>(cached_tokens) * prefill_ms_per_token,
      "long context prefix cache saving");
  result.chunked_prefill_ms = require_finite(
      static_cast<double>(effective_prompt_tokens) * prefill_ms_per_token +
          scheduling_overhead_ms,
      "long context chunked prefill latency");
  result.decode_ms = require_finite(
      static_cast<double>(output_tokens) * decode_ms_per_token,
      "long context decode latency");
  result.ttft_ms = result.chunked_prefill_ms;
  result.kv_cache_gb = require_finite(
      static_cast<double>(total_sequence_tokens) * kv_bytes_per_token /
          (1024.0 * 1024.0 * 1024.0),
      "long context KV cache size");
  return result;
}

std::vector<DisaggregatedScenario> default_disaggregated_scenarios() {
  std::vector<DisaggregatedScenario> scenarios;
  const int prefill_workers[] = {1, 2, 4};
  const int decode_workers[] = {1, 2, 4};
  const int prompt_tokens[] = {512, 2048, 8192};
  const int output_tokens[] = {64, 256};
  const double qps_values[] = {1.0, 5.0, 20.0};
  const double bandwidth_values[] = {25.0, 100.0, 200.0};
  for (const int prefill : prefill_workers) {
    for (const int decode : decode_workers) {
      for (const int prompt : prompt_tokens) {
        for (const int output : output_tokens) {
          for (const double qps : qps_values) {
            for (const double bandwidth : bandwidth_values) {
              scenarios.push_back(
                  {prefill, decode, prompt, output, qps, bandwidth});
            }
          }
        }
      }
    }
  }
  return scenarios;
}

std::vector<SloScenario> default_slo_scenarios() {
  std::vector<SloScenario> scenarios;
  const double qps_values[] = {1.0, 5.0, 20.0, 100.0};
  const int prompt_tokens[] = {512, 2048};
  const int output_tokens[] = {128, 512};
  for (const double qps : qps_values) {
    for (const int prompt : prompt_tokens) {
      for (const int output : output_tokens) {
        scenarios.push_back({qps, prompt, output});
      }
    }
  }
  scenarios.push_back({80.0, 1024, 256});
  return scenarios;
}

std::vector<LongContextScenario> default_long_context_scenarios() {
  std::vector<LongContextScenario> scenarios;
  const int prompts[] = {2048, 8192, 32768, 131072};
  const int outputs[] = {128, 512};
  const int chunks[] = {1024, 2048, 4096};
  const double hit_rates[] = {0.0, 0.25, 0.75};
  for (const int prompt : prompts) {
    for (const int output : outputs) {
      for (const int chunk : chunks) {
        for (const double hit_rate : hit_rates) {
          scenarios.push_back(
              {prompt, output, chunk, hit_rate, 0.08, 1.20, 32768.0});
        }
      }
    }
  }
  scenarios.push_back({8192, 256, 2048, 0.25, 0.08, 1.20, 32768.0,
                       24.0, "release_kv_budget_8k"});
  return scenarios;
}

}  // namespace inferlab
