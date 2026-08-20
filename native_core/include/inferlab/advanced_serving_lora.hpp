#pragma once

#include <string>
#include <vector>

namespace inferlab {

struct LoraAdapterCacheScenario {
  std::string name;
  std::string backend;
  int adapter_count = 1;
  int active_adapters_per_minute = 1;
  int cache_slots = 1;
  double adapter_size_mb = 256.0;
  double base_model_memory_gb = 14.0;
  double gpu_memory_gb = 24.0;
  double adapter_load_ms = 80.0;
  double request_qps = 10.0;
  double hot_adapter_fraction = 0.60;
  std::string prefix_cache_key_scope;
  bool cross_adapter_prefix_sharing_allowed = true;
  bool release_isolation_fixture = false;
};

struct LoraAdapterCacheResult {
  LoraAdapterCacheScenario scenario;
  double adapter_memory_mb = 0.0;
  double gpu_memory_headroom_gb = 0.0;
  double cache_hit_rate = 0.0;
  double evictions_per_minute = 0.0;
  double load_penalty_p99_ms = 0.0;
  double mixed_batch_fragmentation_score = 0.0;
  double adapter_pressure_score = 0.0;
  std::string cache_policy;
  std::string recommendation;
};

LoraAdapterCacheResult estimate_lora_adapter_cache(
    const LoraAdapterCacheScenario& scenario);

std::vector<LoraAdapterCacheScenario> default_lora_adapter_cache_scenarios();

}  // namespace inferlab
