#include "kv_allocation_compare_model.hpp"

#include "checked_index.hpp"
#include "paged_kv_probe_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <unordered_set>

namespace inferlab::kv_allocation_compare {
namespace {

struct ExecutionResult {
  StaticAllocationStats static_stats;
  AllocationStats paged_stats;
};

const std::vector<WorkloadRow>& canonical_workload() {
  static const std::vector<WorkloadRow> workload = {
      {1, 7},
      {2, 23},
      {3, 5},
      {4, 31},
  };
  return workload;
}

bool same_workload(const std::vector<WorkloadRow>& lhs,
                   const std::vector<WorkloadRow>& rhs) {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < lhs.size(); ++index) {
    if (lhs[index].request_id != rhs[index].request_id ||
        lhs[index].tokens != rhs[index].tokens) {
      return false;
    }
  }
  return true;
}

bool same_static_stats(const StaticAllocationStats& lhs,
                       const StaticAllocationStats& rhs) {
  return lhs.max_requests == rhs.max_requests &&
         lhs.active_requests == rhs.active_requests &&
         lhs.total_tokens == rhs.total_tokens &&
         lhs.used_bytes == rhs.used_bytes &&
         lhs.reserved_bytes == rhs.reserved_bytes &&
         lhs.utilization == rhs.utilization;
}

bool same_paged_stats(const AllocationStats& lhs,
                      const AllocationStats& rhs) {
  return lhs.total_blocks == rhs.total_blocks &&
         lhs.free_blocks == rhs.free_blocks &&
         lhs.used_blocks == rhs.used_blocks &&
         lhs.active_requests == rhs.active_requests &&
         lhs.total_tokens == rhs.total_tokens &&
         lhs.used_bytes == rhs.used_bytes &&
         lhs.reserved_bytes == rhs.reserved_bytes &&
         lhs.utilization == rhs.utilization;
}

void validate_static_stats(const StaticAllocationStats& stats) {
  if (stats.max_requests <= 0 ||
      stats.max_requests > probe::kMaxPagedKVProbeBlocks ||
      stats.active_requests < 0 ||
      stats.active_requests > stats.max_requests || stats.total_tokens < 0 ||
      stats.used_bytes < 0 || stats.reserved_bytes < 0 ||
      stats.used_bytes > stats.reserved_bytes ||
      !std::isfinite(stats.utilization) || stats.utilization < 0.0 ||
      stats.utilization > 1.0 ||
      (stats.active_requests == 0 &&
       (stats.total_tokens != 0 || stats.used_bytes != 0 ||
        stats.reserved_bytes != 0 || stats.utilization != 0.0)) ||
      (stats.reserved_bytes == 0 && stats.utilization != 0.0)) {
    throw std::logic_error(
        "KV allocation static stats are inconsistent");
  }
}

int planned_total_blocks(const Config& config) {
  if (config.max_requests <= 0 || config.max_sequence_length <= 0 ||
      config.block_size_tokens <= 0 || config.bytes_per_token <= 0) {
    throw std::invalid_argument(
        "KV allocation config dimensions must be positive");
  }
  const int blocks_per_request =
      internal::checked_ceil_div_non_negative_int(
          config.max_sequence_length, config.block_size_tokens,
          "KV allocation blocks per request");
  const int total_blocks = internal::checked_multiply_int(
      config.max_requests, blocks_per_request,
      "KV allocation total blocks");
  return probe::require_paged_kv_probe_capacity(
      total_blocks, "KV allocation total blocks");
}

std::int64_t validate_workload(const Config& config,
                               const std::vector<WorkloadRow>& workload) {
  if (workload.empty()) {
    throw std::logic_error("KV allocation workload must not be empty");
  }
  const int required_requests = internal::checked_int_from_size(
      workload.size(), "KV allocation workload request count");
  int required_sequence_length = 0;
  std::int64_t expected_tokens = 0;
  std::unordered_set<RequestId> request_ids;
  request_ids.reserve(workload.size());
  for (const auto& row : workload) {
    if (row.request_id <= 0 || row.tokens < 0 ||
        !request_ids.insert(row.request_id).second) {
      throw std::logic_error("KV allocation workload is invalid");
    }
    required_sequence_length =
        std::max(required_sequence_length, row.tokens);
    expected_tokens = internal::checked_add_i64(
        expected_tokens, static_cast<std::int64_t>(row.tokens),
        "KV allocation workload tokens");
  }
  if (config.max_requests < required_requests ||
      config.max_sequence_length < required_sequence_length) {
    throw std::invalid_argument(
        "KV allocation cache limits do not fit the built-in workload");
  }
  return expected_tokens;
}

ExecutionResult execute_workload(
    const Config& config, const int total_blocks,
    const std::vector<WorkloadRow>& workload) {
  StaticKVCacheManager static_cache(
      {config.max_requests, config.max_sequence_length,
       config.bytes_per_token});
  PagedKVCacheManager paged_cache(
      {total_blocks, config.block_size_tokens, config.bytes_per_token});
  for (const auto& row : workload) {
    if (!static_cache.add_request(row.request_id, row.tokens)) {
      throw std::runtime_error("static cache failed to add request");
    }
    if (!paged_cache.add_request(row.request_id, row.tokens)) {
      throw std::runtime_error("paged cache failed to add request");
    }
  }
  return {static_cache.stats(), paged_cache.stats()};
}

void validate_cross_cache_result(
    const std::vector<WorkloadRow>& workload,
    const std::int64_t expected_tokens,
    const StaticAllocationStats& static_stats,
    const AllocationStats& paged_stats) {
  validate_static_stats(static_stats);
  probe::validate_paged_kv_probe_stats(
      paged_stats, "KV allocation paged stats");
  const int workload_size = internal::checked_int_from_size(
      workload.size(), "KV allocation workload size");
  if (static_stats.active_requests != workload_size ||
      paged_stats.active_requests != workload_size ||
      static_stats.total_tokens != expected_tokens ||
      paged_stats.total_tokens != expected_tokens) {
    throw std::logic_error(
        "KV allocation cache statistics disagree with workload");
  }
}

}  // namespace

Plan build_plan(const Config& config) {
  Plan plan;
  plan.config = config;
  plan.paged_total_blocks = planned_total_blocks(config);
  plan.workload = canonical_workload();
  plan.expected_tokens = validate_workload(config, plan.workload);
  const auto result =
      execute_workload(config, plan.paged_total_blocks, plan.workload);
  plan.static_stats = result.static_stats;
  plan.paged_stats = result.paged_stats;
  validate_cross_cache_result(plan.workload, plan.expected_tokens,
                              plan.static_stats, plan.paged_stats);
  plan.reserved_bytes_delta = internal::checked_add_i64(
      plan.static_stats.reserved_bytes,
      -plan.paged_stats.reserved_bytes,
      "KV allocation reserved byte delta");
  plan.reserved_bytes_ratio = probe::paged_kv_probe_ratio(
      plan.paged_stats.reserved_bytes,
      plan.static_stats.reserved_bytes,
      "KV allocation paged/static reserved ratio");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  const int expected_total_blocks = planned_total_blocks(plan.config);
  if (plan.paged_total_blocks != expected_total_blocks ||
      !same_workload(plan.workload, canonical_workload())) {
    throw std::logic_error(
        "KV allocation plan identity does not match canonical fixture");
  }
  const std::int64_t expected_tokens =
      validate_workload(plan.config, plan.workload);
  if (plan.expected_tokens != expected_tokens) {
    throw std::logic_error(
        "KV allocation expected token total is inconsistent");
  }
  validate_cross_cache_result(plan.workload, plan.expected_tokens,
                              plan.static_stats, plan.paged_stats);
  const auto expected_result = execute_workload(
      plan.config, plan.paged_total_blocks, plan.workload);
  if (!same_static_stats(plan.static_stats,
                         expected_result.static_stats) ||
      !same_paged_stats(plan.paged_stats, expected_result.paged_stats)) {
    throw std::logic_error(
        "KV allocation result does not match canonical cache execution");
  }
  const auto expected_delta = internal::checked_add_i64(
      plan.static_stats.reserved_bytes,
      -plan.paged_stats.reserved_bytes,
      "KV allocation reserved byte delta");
  const auto expected_ratio = probe::paged_kv_probe_ratio(
      plan.paged_stats.reserved_bytes,
      plan.static_stats.reserved_bytes,
      "KV allocation paged/static reserved ratio");
  if (plan.reserved_bytes_delta != expected_delta ||
      plan.reserved_bytes_ratio != expected_ratio) {
    throw std::logic_error(
        "KV allocation savings summary is inconsistent");
  }
}

}  // namespace inferlab::kv_allocation_compare
