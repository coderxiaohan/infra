#pragma once

#include "inferlab/paged_kv_cache.hpp"

#include <cstdint>

namespace inferlab::prefix_cache_compare {

struct Config {
  int num_requests = 0;
  int prefix_tokens = 0;
  int suffix_tokens_per_request = 0;
  int block_size_tokens = 0;
  int bytes_per_token = 0;
};

struct Plan {
  Config config;
  PagedKVCacheConfig cache_config;
  AllocationStats no_prefix_stats;
  AllocationStats shared_prefix_stats;
  PrefixCacheTrafficProfile traffic_profile;
  std::int64_t reserved_bytes_delta = 0;
  double reserved_bytes_ratio = 0.0;
};

Plan build_plan(const Config& config);

void validate_plan(const Plan& plan);

}  // namespace inferlab::prefix_cache_compare
