#pragma once

#include <string>
#include <vector>

namespace inferlab {

struct RadixPrefixCacheScenario {
  std::string name;
  std::string backend;
  int requests = 1000;
  int avg_prompt_tokens = 2048;
  int shared_prefix_tokens = 1024;
  int unique_suffix_tokens = 1024;
  double prefix_reuse_rate = 0.50;
  double node_overhead_bytes = 96.0;
  double kv_bytes_per_token = 524288.0;
  double prefill_ms_per_token = 0.08;
  double radix_lookup_us_per_node = 0.50;
  int adapter_residency_limit = 0;
  std::string prefix_cache_key_scope;
  bool cross_adapter_prefix_sharing_allowed = true;
  bool release_isolation_fixture = false;
};

struct RadixPrefixCacheResult {
  RadixPrefixCacheScenario scenario;
  int estimated_nodes = 0;
  double reused_tokens = 0.0;
  double saved_prefill_ms = 0.0;
  double lookup_overhead_ms = 0.0;
  double radix_metadata_mb = 0.0;
  double saved_kv_cache_gb = 0.0;
  double net_ttft_saving_ms = 0.0;
  double cache_efficiency_score = 0.0;
  std::string recommendation;
  int shared_blocks = 0;
  int suffix_blocks = 0;
  int split_count = 0;
  double exact_prefix_hit_rate = 0.0;
  double hash_prefix_hit_rate = 0.0;
  double radix_prefix_hit_rate = 0.0;
  double block_sharing_ratio = 0.0;
  double fragmentation_ratio = 0.0;
  double eviction_cost_ms = 0.0;
};

RadixPrefixCacheResult estimate_radix_prefix_cache(
    const RadixPrefixCacheScenario& scenario);

std::vector<RadixPrefixCacheScenario> default_radix_prefix_cache_scenarios();

}  // namespace inferlab
