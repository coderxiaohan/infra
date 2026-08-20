#pragma once

#include "inferlab/paged_kv_cache.hpp"
#include "inferlab/static_kv_cache.hpp"

#include <cstdint>
#include <vector>

namespace inferlab::kv_allocation_compare {

struct WorkloadRow {
  RequestId request_id = 0;
  int tokens = 0;
};

struct Config {
  int max_requests = 0;
  int max_sequence_length = 0;
  int block_size_tokens = 0;
  int bytes_per_token = 0;
};

struct Plan {
  Config config;
  int paged_total_blocks = 0;
  std::vector<WorkloadRow> workload;
  StaticAllocationStats static_stats;
  AllocationStats paged_stats;
  std::int64_t expected_tokens = 0;
  std::int64_t reserved_bytes_delta = 0;
  double reserved_bytes_ratio = 0.0;
};

Plan build_plan(const Config& config);

void validate_plan(const Plan& plan);

}  // namespace inferlab::kv_allocation_compare
