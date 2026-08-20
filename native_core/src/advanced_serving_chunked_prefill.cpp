#include "inferlab/advanced_serving_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::finite_unit_interval;
using advanced_serving_internal::non_negative_finite_or_zero;
using advanced_serving_internal::positive_or_one;
using advanced_serving_internal::require_finite;

ChunkedPrefillSchedulerResult estimate_chunked_prefill_scheduler(
    const ChunkedPrefillSchedulerScenario& scenario) {
  const int prompt_tokens = std::max(0, scenario.prompt_tokens);
  const int output_tokens = std::max(0, scenario.output_tokens);
  const int chunk_size = positive_or_one(scenario.chunk_size);
  const int max_batch_tokens = std::max(
      chunk_size, std::max(0, scenario.max_prefill_batch_tokens));
  const int decode_streams = std::max(0, scenario.concurrent_decode_streams);
  if (output_tokens > 0 && decode_streams == 0) {
    throw std::invalid_argument(
        "chunked prefill decode streams must be positive when decode work exists");
  }
  const double prefix_hit =
      finite_unit_interval(scenario.prefix_cache_hit_rate,
                           "chunked prefill prefix cache hit rate");
  const double prefill_ms_per_token =
      non_negative_finite_or_zero(
          scenario.prefill_ms_per_token,
          "chunked prefill milliseconds per token");
  const double decode_ms_per_token =
      non_negative_finite_or_zero(
          scenario.decode_ms_per_token,
          "chunked prefill decode milliseconds per token");
  const double scheduler_tick =
      non_negative_finite_or_zero(scenario.scheduler_tick_ms,
                                  "chunked prefill scheduler tick");
  const double launch_overhead =
      non_negative_finite_or_zero(
          scenario.chunk_launch_overhead_ms,
          "chunked prefill launch overhead");
  const double kv_bytes =
      non_negative_finite_or_zero(scenario.kv_bytes_per_token,
                                  "chunked prefill KV bytes per token");
  const double kv_budget =
      non_negative_finite_or_zero(scenario.kv_budget_gb,
                                  "chunked prefill KV budget");

  const int cached_tokens =
      internal::checked_non_negative_floor_double_to_int(
          finite_product(static_cast<double>(prompt_tokens), prefix_hit,
                         "chunked prefill cached token estimate"),
          "chunked prefill cached tokens");
  const int effective_prompt = prompt_tokens - cached_tokens;
  const int chunk_count = internal::checked_ceil_div_non_negative_int(
      effective_prompt, chunk_size, "chunked prefill chunk count");
  const int chunks_per_wave =
      std::max(1, max_batch_tokens / chunk_size);
  const int waves = internal::checked_ceil_div_non_negative_int(
      chunk_count, chunks_per_wave, "chunked prefill waves");
  const double first_chunk_tokens =
      static_cast<double>(std::min(chunk_size, effective_prompt));
  const double first_chunk_ttft =
      effective_prompt == 0
          ? 0.0
          : require_finite(first_chunk_tokens * prefill_ms_per_token +
                               launch_overhead + scheduler_tick,
                           "chunked prefill first chunk TTFT");
  const double raw_prefill = require_finite(
      static_cast<double>(effective_prompt) * prefill_ms_per_token,
      "chunked prefill raw latency");
  const double scheduler_gap = require_finite(
      static_cast<double>(waves) * scheduler_tick +
          static_cast<double>(chunk_count) * launch_overhead,
      "chunked prefill scheduler gap");
  const double total_prefill = require_finite(
      raw_prefill + scheduler_gap, "chunked prefill total latency");
  if (first_chunk_ttft > total_prefill) {
    throw std::logic_error(
        "chunked prefill first-chunk TTFT exceeds total prefill latency");
  }
  const double decode_window =
      output_tokens == 0
          ? 0.0
          : require_finite(
                static_cast<double>(output_tokens) * decode_ms_per_token /
                    std::sqrt(static_cast<double>(decode_streams)),
                "chunked prefill decode window");
  const double decode_stall =
      output_tokens == 0
          ? 0.0
          : require_finite(
                std::max(0.0, total_prefill - first_chunk_ttft -
                                  decode_window * 0.35),
                "chunked prefill decode stall");
  const double prefix_saved = require_finite(
      static_cast<double>(cached_tokens) * prefill_ms_per_token,
      "chunked prefill prefix saving");
  const int total_sequence_tokens = internal::checked_add_int(
      prompt_tokens, output_tokens,
      "chunked prefill total sequence tokens");
  const double kv_cache = require_finite(
      static_cast<double>(total_sequence_tokens) * kv_bytes /
          (1024.0 * 1024.0 * 1024.0),
      "chunked prefill KV cache size");
  const double kv_utilization =
      kv_budget > 0.0
          ? require_finite(std::min(4.0, kv_cache / kv_budget),
                           "chunked prefill KV budget utilization")
          : (kv_cache > 0.0 ? 4.0 : 0.0);
  const double efficiency = finite_unit_interval(
      1.0 - scheduler_gap / std::max(1.0, total_prefill) -
          decode_stall /
              std::max(1.0, total_prefill + decode_window) * 0.35,
      "chunked prefill throughput efficiency");
  const double prefix_miss_pressure =
      prompt_tokens == 0 ? 0.0 : (1.0 - prefix_hit) * 0.08;
  const double pressure = finite_unit_interval(
      std::min(1.0, kv_utilization) * 0.30 +
          std::min(1.0, chunk_count / 64.0) * 0.20 +
          scheduler_gap / std::max(1.0, total_prefill) * 0.22 +
          decode_stall /
              std::max(1.0, total_prefill + decode_window) * 0.20 +
          prefix_miss_pressure,
      "chunked prefill pressure score");
  const std::int64_t decode_batch_hint =
      static_cast<std::int64_t>(max_batch_tokens / chunk_size) +
      static_cast<std::int64_t>(decode_streams / 2);
  const int decode_maximal_batch =
      output_tokens == 0
          ? 0
          : internal::checked_int_from_i64(
                std::max<std::int64_t>(
                    1,
                    std::min<std::int64_t>(decode_streams,
                                           decode_batch_hint)),
                "chunked prefill decode maximal batch size");
  const double decode_starvation =
      output_tokens == 0
          ? 0.0
          : finite_unit_interval(
                decode_stall /
                        std::max(1.0, total_prefill + decode_window) * 0.70 +
                    scheduler_gap / std::max(1.0, total_prefill) * 0.20 +
                    std::max(0.0, kv_utilization - 1.0) * 0.10,
                "chunked prefill decode starvation risk");
  const double sarathi_score =
      output_tokens == 0
          ? 1.0
          : finite_unit_interval(
                1.0 - decode_starvation * 0.60 -
                    scheduler_gap / std::max(1.0, total_prefill) * 0.25 -
                    std::max(0.0, kv_utilization - 1.0) * 0.15,
                "chunked prefill Sarathi score");
  const double fairness =
      output_tokens == 0
          ? 1.0
          : finite_unit_interval(
                1.0 - std::abs(static_cast<double>(chunk_size) /
                                   max_batch_tokens -
                               0.25) *
                          0.65 -
                    decode_starvation * 0.35,
                "chunked prefill fairness estimate");
  const double p99_risk =
      total_sequence_tokens == 0
          ? 0.0
          : finite_unit_interval(
                decode_starvation * 0.40 +
                    std::min(1.0, chunk_count / 80.0) * 0.25 +
                    std::min(1.0, scheduler_tick / 8.0) * 0.20 +
                    std::min(1.0, kv_utilization) * 0.15,
                "chunked prefill P99 risk");

  ChunkedPrefillSchedulerResult result;
  result.scenario = scenario;
  result.scenario.prompt_tokens = prompt_tokens;
  result.scenario.output_tokens = output_tokens;
  result.scenario.chunk_size = chunk_size;
  result.scenario.max_prefill_batch_tokens = max_batch_tokens;
  result.scenario.concurrent_decode_streams = decode_streams;
  result.scenario.prefix_cache_hit_rate = prefix_hit;
  result.scenario.prefill_ms_per_token = prefill_ms_per_token;
  result.scenario.decode_ms_per_token = decode_ms_per_token;
  result.scenario.scheduler_tick_ms = scheduler_tick;
  result.scenario.chunk_launch_overhead_ms = launch_overhead;
  result.scenario.kv_bytes_per_token = kv_bytes;
  result.scenario.kv_budget_gb = kv_budget;
  result.effective_prompt_tokens = effective_prompt;
  result.chunk_count = chunk_count;
  result.prefill_waves = waves;
  result.first_chunk_ttft_ms = first_chunk_ttft;
  result.total_prefill_ms = total_prefill;
  result.scheduler_gap_ms = scheduler_gap;
  result.decode_stall_ms = decode_stall;
  result.prefix_cache_saved_ms = prefix_saved;
  result.kv_cache_gb = kv_cache;
  result.kv_budget_utilization = kv_utilization;
  result.throughput_efficiency = efficiency;
  result.long_context_pressure_score = pressure;
  result.sarathi_stall_free_score = sarathi_score;
  result.decode_maximal_batch_size = decode_maximal_batch;
  result.decode_starvation_risk = decode_starvation;
  result.jain_fairness_estimate = fairness;
  result.p99_risk_score = p99_risk;
  const int timeline_chunks = std::min(chunk_count, 8);
  for (int i = 0; i < timeline_chunks; ++i) {
    const std::int64_t token_offset =
        static_cast<std::int64_t>(i) * chunk_size;
    const int remaining_tokens = internal::checked_int_from_i64(
        static_cast<std::int64_t>(effective_prompt) - token_offset,
        "chunked prefill timeline remaining tokens");
    const int chunk_tokens =
        std::min(chunk_size, std::max(0, remaining_tokens));
    std::ostringstream item;
    item << "chunk_" << i << ":start_ms=" << std::fixed
         << std::setprecision(3)
         << static_cast<double>(i / chunks_per_wave) * scheduler_tick +
                static_cast<double>(i) * launch_overhead
         << ",tokens=" << chunk_tokens
         << ",decode_batch=" << decode_maximal_batch
         << ",policy="
         << (output_tokens > 0 ? "sarathi_decode_maximal" : "prefill_only");
    result.prefill_chunk_timeline.push_back(item.str());
  }
  result.chunk_policy =
      effective_prompt == 0
          ? "no_prefill_work"
          : (output_tokens == 0
                 ? "prefill_only"
                 : (sarathi_score >= 0.78
                        ? "sarathi_stall_free_decode_maximal"
                        : (kv_utilization > 1.0
                               ? "reduce_chunk_size_and_enable_kv_tiering"
                               : (decode_stall > first_chunk_ttft
                                      ? "interleave_prefill_decode_microbatches"
                                      : (prefix_hit >= 0.50
                                             ? "prefix_cache_first_chunked_prefill"
                                             : "chunked_prefill_with_admission_guard")))));
  result.recommendation =
      total_sequence_tokens == 0
          ? "no_prefill_or_decode_work"
          : (effective_prompt == 0
                 ? (output_tokens > 0
                        ? "reuse_prefix_and_profile_decode_only"
                        : "reuse_prefix_without_prefill")
                 : (p99_risk >= 0.72
                        ? "reduce_chunk_size_or_reserve_decode_slots_for_p99"
                        : (pressure >= 0.70
                               ? "split_prefill_decode_or_raise_kv_budget"
                               : (efficiency < 0.72
                                      ? "increase_prefill_batch_tokens_or_reduce_scheduler_tick"
                                      : "keep_chunked_prefill_and_profile_real_ttft"))));
  return result;
}

std::vector<ChunkedPrefillSchedulerScenario>
default_chunked_prefill_scheduler_scenarios() {
  return {
      {"vllm_32k_chunked_prefill", "vllm", 32768, 512, 2048, 16384, 24,
       0.35, 0.070, 1.05, 1.5, 0.30, 524288.0, 40.0},
      {"sglang_128k_radix_prefill", "sglang", 131072, 1024, 4096, 32768,
       32, 0.62, 0.065, 1.00, 1.2, 0.28, 524288.0, 80.0},
      {"trtllm_64k_engine_prefill", "tensorrt_llm", 65536, 768, 8192,
       32768, 16, 0.25, 0.050, 0.85, 1.0, 0.22, 786432.0, 96.0},
      {"kserve_llmd_remote_prefill", "kserve_llm_d", 98304, 512, 4096,
       24576, 20, 0.45, 0.080, 1.10, 2.5, 0.45, 655360.0, 64.0},
      {"native_low_budget_32k", "native", 32768, 256, 1024, 8192, 8,
       0.10, 0.090, 1.25, 3.0, 0.60, 524288.0, 24.0},
      {"release_kv_budget_8k", "native", 8192, 256, 2048, 8192, 8,
       0.25, 0.080, 1.20, 2.0, 0.35, 32768.0, 24.0},
  };
}

}  // namespace inferlab
