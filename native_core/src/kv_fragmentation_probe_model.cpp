#include "kv_fragmentation_probe_model.hpp"

#include "checked_index.hpp"
#include "paged_kv_probe_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace inferlab::kv_fragmentation_probe {
namespace {

constexpr int kReuseRequestTokens = 13;
constexpr RequestId kReuseRequestId = 100;
constexpr RequestId kPressureRequestId = 200;

const std::vector<int>& initial_tokens() {
  static const std::vector<int> tokens = {4, 8, 5, 12, 1, 7};
  return tokens;
}

const std::vector<std::string>& expected_phases() {
  static const std::vector<std::string> phases = {
      "initial",
      "after_initial_allocations",
      "after_releasing_middle_requests",
      "after_reuse_allocation",
      "after_oversized_attempt",
      "after_final_release",
  };
  return phases;
}

void validate_config(const PagedKVCacheConfig& config) {
  if (config.block_size_tokens <= 0 || config.bytes_per_token <= 0) {
    throw std::invalid_argument(
        "KV fragmentation cache resources must be positive");
  }
  (void)probe::require_paged_kv_probe_capacity(
      config.num_blocks, "KV fragmentation configured blocks");
}

struct ScenarioCapacity {
  int initial_blocks = 0;
  int released_block_capacity = 0;
  int reuse_blocks_required = 0;
  int pressure_attempt_tokens = 0;
};

ScenarioCapacity plan_capacity(const PagedKVCacheConfig& config) {
  validate_config(config);
  ScenarioCapacity capacity;
  for (const auto tokens : initial_tokens()) {
    capacity.initial_blocks = internal::checked_add_int(
        capacity.initial_blocks,
        internal::checked_ceil_div_non_negative_int(
            tokens, config.block_size_tokens,
            "KV fragmentation initial request blocks"),
        "KV fragmentation initial blocks");
  }
  capacity.released_block_capacity = internal::checked_add_int(
      internal::checked_ceil_div_non_negative_int(
          initial_tokens()[1], config.block_size_tokens,
          "KV fragmentation first released request blocks"),
      internal::checked_ceil_div_non_negative_int(
          initial_tokens()[3], config.block_size_tokens,
          "KV fragmentation second released request blocks"),
      "KV fragmentation released block capacity");
  capacity.reuse_blocks_required =
      internal::checked_ceil_div_non_negative_int(
          kReuseRequestTokens, config.block_size_tokens,
          "KV fragmentation reuse request blocks");
  capacity.pressure_attempt_tokens = internal::checked_multiply_int(
      config.block_size_tokens, 4,
      "KV fragmentation pressure attempt tokens");
  if (capacity.released_block_capacity > capacity.initial_blocks) {
    throw std::logic_error(
        "KV fragmentation release capacity exceeds initial allocation");
  }
  const int used_after_holes =
      capacity.initial_blocks - capacity.released_block_capacity;
  const int required_for_reuse = internal::checked_add_int(
      used_after_holes, capacity.reuse_blocks_required,
      "KV fragmentation required reuse capacity");
  if (capacity.initial_blocks > config.num_blocks ||
      required_for_reuse > config.num_blocks) {
    throw std::invalid_argument(
        "KV fragmentation config cannot execute the built-in scenario");
  }
  return capacity;
}

std::int64_t peak_reserved_bytes(
    const std::vector<Snapshot>& snapshots) {
  std::int64_t peak = 0;
  for (const auto& snapshot : snapshots) {
    peak = std::max(peak, snapshot.stats.reserved_bytes);
  }
  return peak;
}

int count_reused_blocks(const std::vector<BlockId>& released_blocks,
                        const std::vector<BlockId>& reuse_blocks) {
  std::unordered_set<BlockId> released;
  released.reserve(released_blocks.size());
  for (const auto block : released_blocks) {
    if (!released.insert(block).second) {
      throw std::logic_error(
          "KV fragmentation released block sample contains duplicates");
    }
  }
  std::unordered_set<BlockId> reuse;
  reuse.reserve(reuse_blocks.size());
  int reused = 0;
  for (const auto block : reuse_blocks) {
    if (!reuse.insert(block).second) {
      throw std::logic_error(
          "KV fragmentation reuse block table contains duplicates");
    }
    if (released.find(block) != released.end()) {
      reused = internal::checked_add_int(
          reused, 1, "KV fragmentation reused released blocks");
    }
  }
  return reused;
}

Plan execute_plan(const PagedKVCacheConfig& config) {
  const auto capacity = plan_capacity(config);
  PagedKVCacheManager cache(config);
  Plan plan;
  plan.config = config;
  plan.snapshots.reserve(expected_phases().size());
  plan.snapshots.push_back({"initial", cache.stats()});

  for (std::size_t i = 0U; i < initial_tokens().size(); ++i) {
    const auto request_id = static_cast<RequestId>(
        internal::checked_int_from_size(
            i + 1U, "KV fragmentation initial request id"));
    if (!cache.add_request(request_id, initial_tokens()[i])) {
      throw std::runtime_error("initial allocation failed");
    }
  }
  plan.snapshots.push_back(
      {"after_initial_allocations", cache.stats()});

  plan.released_blocks.reserve(
      static_cast<std::size_t>(capacity.released_block_capacity));
  const RequestId released_request_ids[] = {2, 4};
  for (const auto request_id : released_request_ids) {
    const auto blocks = cache.request_blocks(request_id);
    if (!blocks.has_value()) {
      throw std::runtime_error("request missing before release");
    }
    plan.released_blocks.insert(
        plan.released_blocks.end(), blocks->block_table.begin(),
        blocks->block_table.end());
    if (!cache.release_request(request_id)) {
      throw std::runtime_error("release failed");
    }
  }
  plan.snapshots.push_back(
      {"after_releasing_middle_requests", cache.stats()});

  if (!cache.add_request(kReuseRequestId, kReuseRequestTokens)) {
    throw std::runtime_error("reuse allocation failed");
  }
  const auto reuse_request = cache.request_blocks(kReuseRequestId);
  if (!reuse_request.has_value()) {
    throw std::runtime_error("reuse request missing");
  }
  plan.reuse_blocks = reuse_request->block_table;
  if (plan.reuse_blocks.size() !=
      static_cast<std::size_t>(capacity.reuse_blocks_required)) {
    throw std::logic_error(
        "reuse request block count disagrees with capacity plan");
  }
  plan.snapshots.push_back({"after_reuse_allocation", cache.stats()});

  plan.pressure_attempt_succeeded =
      cache.add_request(
          kPressureRequestId, capacity.pressure_attempt_tokens);
  plan.failed_allocations = plan.pressure_attempt_succeeded ? 0 : 1;
  plan.snapshots.push_back({"after_oversized_attempt", cache.stats()});

  const RequestId final_release_request_ids[] = {
      1, 3, 5, 6, kReuseRequestId};
  for (const auto request_id : final_release_request_ids) {
    if (!cache.release_request(request_id)) {
      throw std::runtime_error("final release failed");
    }
  }
  if (plan.pressure_attempt_succeeded &&
      !cache.release_request(kPressureRequestId)) {
    throw std::runtime_error("pressure request final release failed");
  }
  plan.snapshots.push_back({"after_final_release", cache.stats()});

  plan.reused_released_blocks =
      count_reused_blocks(plan.released_blocks, plan.reuse_blocks);
  plan.peak_reserved_bytes = peak_reserved_bytes(plan.snapshots);
  return plan;
}

bool same_stats(const AllocationStats& left,
                const AllocationStats& right) {
  return left.total_blocks == right.total_blocks &&
         left.free_blocks == right.free_blocks &&
         left.used_blocks == right.used_blocks &&
         left.active_requests == right.active_requests &&
         left.total_tokens == right.total_tokens &&
         left.used_bytes == right.used_bytes &&
         left.reserved_bytes == right.reserved_bytes &&
         left.utilization == right.utilization;
}

bool same_snapshots(const std::vector<Snapshot>& left,
                    const std::vector<Snapshot>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (std::size_t i = 0U; i < left.size(); ++i) {
    if (left[i].phase != right[i].phase ||
        !same_stats(left[i].stats, right[i].stats)) {
      return false;
    }
  }
  return true;
}

void validate_structure(const Plan& plan) {
  validate_config(plan.config);
  if (plan.snapshots.size() != expected_phases().size()) {
    throw std::logic_error(
        "KV fragmentation probe snapshot count is invalid");
  }
  for (std::size_t i = 0U; i < plan.snapshots.size(); ++i) {
    if (plan.snapshots[i].phase != expected_phases()[i]) {
      throw std::logic_error(
          "KV fragmentation probe snapshot phases are misaligned");
    }
    probe::validate_paged_kv_probe_stats(
        plan.snapshots[i].stats, "KV fragmentation snapshot");
  }
  probe::require_empty_paged_kv_probe_cache(
      plan.snapshots.back().stats, "KV fragmentation final cache");
  const int expected_reused =
      count_reused_blocks(plan.released_blocks, plan.reuse_blocks);
  if (plan.failed_allocations !=
          (plan.pressure_attempt_succeeded ? 0 : 1) ||
      plan.reused_released_blocks != expected_reused ||
      plan.reused_released_blocks < 0 ||
      plan.reused_released_blocks >
          internal::checked_int_from_size(
              plan.reuse_blocks.size(),
              "KV fragmentation reuse block count") ||
      plan.peak_reserved_bytes !=
          peak_reserved_bytes(plan.snapshots)) {
    throw std::logic_error(
        "KV fragmentation outcome accounting is inconsistent");
  }
}

}  // namespace

Plan build_plan(const PagedKVCacheConfig& config) {
  auto plan = execute_plan(config);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_structure(plan);
  const auto expected = execute_plan(plan.config);
  if (!same_snapshots(plan.snapshots, expected.snapshots) ||
      plan.released_blocks != expected.released_blocks ||
      plan.reuse_blocks != expected.reuse_blocks ||
      plan.reused_released_blocks != expected.reused_released_blocks ||
      plan.pressure_attempt_succeeded !=
          expected.pressure_attempt_succeeded ||
      plan.failed_allocations != expected.failed_allocations ||
      plan.peak_reserved_bytes != expected.peak_reserved_bytes) {
    throw std::logic_error(
        "KV fragmentation plan is not the canonical scenario result");
  }
}

}  // namespace inferlab::kv_fragmentation_probe
