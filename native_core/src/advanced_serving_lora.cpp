#include "inferlab/advanced_serving_lora.hpp"

#include <algorithm>
#include <cmath>

#include "advanced_serving_internal.hpp"

namespace inferlab {

using advanced_serving_internal::clamp_unit_interval;
using advanced_serving_internal::finite_product;
using advanced_serving_internal::positive_or_default;
using advanced_serving_internal::positive_or_one;
using advanced_serving_internal::require_finite;

LoraAdapterCacheResult estimate_lora_adapter_cache(
    const LoraAdapterCacheScenario& scenario) {
  const int adapters = positive_or_one(scenario.adapter_count);
  const int active = std::min(
      adapters, positive_or_one(scenario.active_adapters_per_minute));
  const int slots = positive_or_one(scenario.cache_slots);
  const double adapter_mb =
      positive_or_default(scenario.adapter_size_mb, 256.0);
  const double base_gb =
      positive_or_default(scenario.base_model_memory_gb, 1.0);
  const double gpu_gb =
      positive_or_default(scenario.gpu_memory_gb, base_gb + 1.0);
  const double load_ms =
      positive_or_default(scenario.adapter_load_ms, 80.0);
  const double qps = positive_or_default(scenario.request_qps, 1.0);
  const double hot_fraction = clamp_unit_interval(require_finite(
      scenario.hot_adapter_fraction, "LoRA cache hot-adapter fraction"));

  const double working_set = static_cast<double>(active);
  const double hot_set = require_finite(
      hot_fraction == 0.0 ? 0.0 : std::ceil(working_set * hot_fraction),
      "LoRA cache hot working set");
  const double cold_set = std::max(0.0, working_set - hot_set);
  const double hot_hit = hot_set == 0.0
      ? 1.0
      : std::min(1.0, static_cast<double>(slots) / hot_set);
  const double remaining_slots =
      std::max(0.0, static_cast<double>(slots) - hot_set);
  const double cold_hit =
      cold_set <= 0.0 ? 1.0 : std::min(1.0, remaining_slots / cold_set);
  const double hit_rate = clamp_unit_interval(require_finite(
      hot_fraction * hot_hit + (1.0 - hot_fraction) * cold_hit,
      "LoRA cache hit rate"));
  const double evictions = require_finite(
      std::max(0.0, working_set - static_cast<double>(slots)) *
          (1.0 - hot_fraction * 0.45),
      "LoRA cache evictions");
  const double cached_adapters = std::min(
      static_cast<double>(adapters), static_cast<double>(slots));
  const double adapter_memory = finite_product(
      cached_adapters, adapter_mb, "LoRA cache adapter memory");
  const double headroom = require_finite(
      gpu_gb - base_gb - adapter_memory / 1024.0,
      "LoRA cache GPU memory headroom");
  const double miss_rate = 1.0 - hit_rate;
  const double load_multiplier =
      std::min(4.0, std::max(1.0, qps / 16.0));
  const double load_penalty = finite_product(
      finite_product(load_ms, miss_rate, "LoRA cache miss load penalty"),
      load_multiplier, "LoRA cache p99 load penalty");
  const double fragmentation = clamp_unit_interval(require_finite(
      working_set / std::max(1.0, static_cast<double>(slots)) * 0.20 +
          miss_rate * 0.55 + std::max(0.0, -headroom) * 0.08,
      "LoRA cache mixed-batch fragmentation"));
  const double gpu_memory_mb = finite_product(
      gpu_gb, 1024.0, "LoRA cache GPU memory in MiB");
  const double pressure = clamp_unit_interval(require_finite(
      miss_rate * 0.45 +
          std::max(0.0, adapter_memory / std::max(1.0, gpu_memory_mb)) *
              0.35 +
          std::max(0.0, -headroom) * 0.20 +
          evictions / std::max(1.0, working_set) * 0.25,
      "LoRA cache adapter pressure"));

  LoraAdapterCacheResult result;
  result.scenario = scenario;
  result.scenario.adapter_count = adapters;
  result.scenario.active_adapters_per_minute = active;
  result.scenario.cache_slots = slots;
  result.scenario.adapter_size_mb = adapter_mb;
  result.scenario.base_model_memory_gb = base_gb;
  result.scenario.gpu_memory_gb = gpu_gb;
  result.scenario.adapter_load_ms = load_ms;
  result.scenario.request_qps = qps;
  result.scenario.hot_adapter_fraction = hot_fraction;
  result.adapter_memory_mb = adapter_memory;
  result.gpu_memory_headroom_gb = headroom;
  result.cache_hit_rate = hit_rate;
  result.evictions_per_minute = evictions;
  result.load_penalty_p99_ms = load_penalty;
  result.mixed_batch_fragmentation_score = fragmentation;
  result.adapter_pressure_score = pressure;
  result.cache_policy =
      hit_rate >= 0.85 ? "pin_hot_adapters_lru_cold_tail" :
      (headroom < 0.0 ? "evict_cold_adapters_and_reduce_slots" :
       "increase_adapter_cache_slots");
  result.recommendation =
      pressure >= 0.70 ? "split_adapter_pool_or_increase_gpu_memory" :
      (fragmentation >= 0.45 ? "batch_by_adapter_then_merge_decode" :
       "keep_multi_lora_batching_with_hot_adapter_pinning");
  return result;
}

std::vector<LoraAdapterCacheScenario>
default_lora_adapter_cache_scenarios() {
  return {
      {"vllm_hot_16_adapters", "vllm", 64, 18, 16, 192.0, 15.5, 24.0, 65.0, 80.0, 0.72},
      {"sglang_many_small_adapters", "sglang", 128, 40, 24, 96.0, 14.0, 24.0, 45.0, 120.0, 0.55},
      {"tensorrt_llm_large_adapters", "tensorrt_llm", 32, 12, 8, 512.0, 38.0, 80.0, 110.0, 42.0, 0.68},
      {"lmdeploy_tenant_churn", "lmdeploy", 96, 48, 12, 256.0, 28.0, 40.0, 95.0, 65.0, 0.35},
      {"native_cache_pressure", "native", 40, 30, 6, 384.0, 20.0, 24.0, 120.0, 35.0, 0.25},
      {"native_release_isolation_cache", "native", 64, 18, 12, 192.0,
       15.5, 24.0, 65.0, 80.0, 0.72,
       "base_model_id:adapter_id:adapter_revision:template_hash", false,
       true},
  };
}

}  // namespace inferlab
