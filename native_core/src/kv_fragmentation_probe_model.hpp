#pragma once

#include "inferlab/paged_kv_cache.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace inferlab::kv_fragmentation_probe {

struct Snapshot {
  std::string phase;
  AllocationStats stats;
};

struct Plan {
  PagedKVCacheConfig config;
  std::vector<Snapshot> snapshots;
  std::vector<BlockId> released_blocks;
  std::vector<BlockId> reuse_blocks;
  int reused_released_blocks = 0;
  bool pressure_attempt_succeeded = false;
  int failed_allocations = 0;
  std::int64_t peak_reserved_bytes = 0;
};

Plan build_plan(const PagedKVCacheConfig& config);

void validate_plan(const Plan& plan);

}  // namespace inferlab::kv_fragmentation_probe
