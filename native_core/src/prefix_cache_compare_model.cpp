#include "prefix_cache_compare_model.hpp"

#include "checked_index.hpp"
#include "paged_kv_probe_utils.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace inferlab::prefix_cache_compare {
namespace {

struct DerivedConfig {
  int total_tokens_per_request = 0;
  PagedKVCacheConfig cache_config;
};

struct ExecutionResult {
  AllocationStats no_prefix_stats;
  AllocationStats shared_prefix_stats;
};

bool same_cache_config(const PagedKVCacheConfig& lhs,
                       const PagedKVCacheConfig& rhs) {
  return lhs.num_blocks == rhs.num_blocks &&
         lhs.block_size_tokens == rhs.block_size_tokens &&
         lhs.bytes_per_token == rhs.bytes_per_token;
}

bool same_stats(const AllocationStats& lhs, const AllocationStats& rhs) {
  return lhs.total_blocks == rhs.total_blocks &&
         lhs.free_blocks == rhs.free_blocks &&
         lhs.used_blocks == rhs.used_blocks &&
         lhs.active_requests == rhs.active_requests &&
         lhs.total_tokens == rhs.total_tokens &&
         lhs.used_bytes == rhs.used_bytes &&
         lhs.reserved_bytes == rhs.reserved_bytes &&
         lhs.utilization == rhs.utilization;
}

bool same_profile(const PrefixCacheTrafficProfile& lhs,
                  const PrefixCacheTrafficProfile& rhs) {
  return lhs.num_requests == rhs.num_requests &&
         lhs.prefix_tokens == rhs.prefix_tokens &&
         lhs.suffix_tokens_per_request == rhs.suffix_tokens_per_request &&
         lhs.total_tokens_per_request == rhs.total_tokens_per_request &&
         lhs.block_size_tokens == rhs.block_size_tokens &&
         lhs.bytes_per_token == rhs.bytes_per_token &&
         lhs.prefix_blocks == rhs.prefix_blocks &&
         lhs.suffix_blocks_per_request == rhs.suffix_blocks_per_request &&
         lhs.no_prefix_blocks_per_request ==
             rhs.no_prefix_blocks_per_request &&
         lhs.no_prefix_physical_tokens == rhs.no_prefix_physical_tokens &&
         lhs.shared_prefix_physical_tokens ==
             rhs.shared_prefix_physical_tokens &&
         lhs.physical_tokens_saved == rhs.physical_tokens_saved &&
         lhs.no_prefix_prefill_write_bytes ==
             rhs.no_prefix_prefill_write_bytes &&
         lhs.shared_prefix_prefill_write_bytes ==
             rhs.shared_prefix_prefill_write_bytes &&
         lhs.prefix_write_bytes_saved == rhs.prefix_write_bytes_saved &&
         lhs.no_prefix_reserved_bytes == rhs.no_prefix_reserved_bytes &&
         lhs.shared_prefix_reserved_bytes ==
             rhs.shared_prefix_reserved_bytes &&
         lhs.reserved_bytes_saved == rhs.reserved_bytes_saved &&
         lhs.shared_reserved_ratio == rhs.shared_reserved_ratio &&
         lhs.no_prefix_request_block_table_entries ==
             rhs.no_prefix_request_block_table_entries &&
         lhs.shared_request_block_table_entries ==
             rhs.shared_request_block_table_entries &&
         lhs.shared_prefix_entry_block_table_entries ==
             rhs.shared_prefix_entry_block_table_entries &&
         lhs.decode_attention_kv_read_bytes_per_step ==
             rhs.decode_attention_kv_read_bytes_per_step &&
         lhs.decode_attention_shared_prefix_read_bytes_per_step ==
             rhs.decode_attention_shared_prefix_read_bytes_per_step;
}

DerivedConfig derive_config(const Config& config) {
  if (config.num_requests <= 0 || config.prefix_tokens <= 0 ||
      config.suffix_tokens_per_request <= 0 ||
      config.block_size_tokens <= 0 || config.bytes_per_token <= 0) {
    throw std::invalid_argument(
        "prefix cache comparison dimensions must be positive");
  }
  DerivedConfig derived;
  derived.total_tokens_per_request = internal::checked_add_int(
      config.prefix_tokens, config.suffix_tokens_per_request,
      "prefix cache total tokens");
  const int blocks_per_request =
      internal::checked_ceil_div_non_negative_int(
          derived.total_tokens_per_request, config.block_size_tokens,
          "prefix cache blocks per request");
  const int prefix_blocks =
      internal::checked_ceil_div_non_negative_int(
          config.prefix_tokens, config.block_size_tokens,
          "prefix cache prefix blocks");
  const int suffix_blocks =
      internal::checked_ceil_div_non_negative_int(
          config.suffix_tokens_per_request, config.block_size_tokens,
          "prefix cache suffix blocks");
  const int no_prefix_capacity = internal::checked_multiply_int(
      config.num_requests, blocks_per_request,
      "prefix cache no-prefix capacity");
  const int shared_capacity = internal::checked_add_int(
      prefix_blocks,
      internal::checked_multiply_int(
          config.num_requests, suffix_blocks,
          "prefix cache shared suffix capacity"),
      "prefix cache shared capacity");
  derived.cache_config = {
      probe::require_paged_kv_probe_capacity(
          std::max(no_prefix_capacity, shared_capacity),
          "prefix cache comparison capacity"),
      config.block_size_tokens,
      config.bytes_per_token,
  };
  return derived;
}

ExecutionResult execute_config(const Config& config,
                               const DerivedConfig& derived) {
  PagedKVCacheManager no_prefix(derived.cache_config);
  PagedKVCacheManager shared_prefix(derived.cache_config);
  constexpr PrefixId kPrefixId = 1;
  if (!shared_prefix.create_prefix(kPrefixId, config.prefix_tokens)) {
    throw std::runtime_error("failed to create shared prefix");
  }
  for (int request = 1; request <= config.num_requests; ++request) {
    if (!no_prefix.add_request(
            static_cast<RequestId>(request),
            derived.total_tokens_per_request)) {
      throw std::runtime_error("failed to add no-prefix request");
    }
    if (!shared_prefix.add_request_with_prefix(
            static_cast<RequestId>(request), kPrefixId,
            config.suffix_tokens_per_request)) {
      throw std::runtime_error("failed to add shared-prefix request");
    }
  }
  return {no_prefix.stats(), shared_prefix.stats()};
}

void validate_cross_result(const Config& config,
                           const AllocationStats& no_prefix,
                           const AllocationStats& shared_prefix,
                           const PrefixCacheTrafficProfile& profile) {
  probe::validate_paged_kv_probe_stats(
      no_prefix, "prefix compare no-prefix stats");
  probe::validate_paged_kv_probe_stats(
      shared_prefix, "prefix compare shared-prefix stats");
  if (profile.num_requests != config.num_requests ||
      profile.prefix_tokens != config.prefix_tokens ||
      profile.suffix_tokens_per_request !=
          config.suffix_tokens_per_request ||
      profile.block_size_tokens != config.block_size_tokens ||
      profile.bytes_per_token != config.bytes_per_token ||
      no_prefix.active_requests != profile.num_requests ||
      shared_prefix.active_requests != profile.num_requests ||
      no_prefix.total_tokens != profile.no_prefix_physical_tokens ||
      shared_prefix.total_tokens != profile.shared_prefix_physical_tokens ||
      no_prefix.reserved_bytes != profile.no_prefix_reserved_bytes ||
      shared_prefix.reserved_bytes != profile.shared_prefix_reserved_bytes) {
    throw std::logic_error(
        "prefix cache manager statistics disagree with traffic profile");
  }
}

}  // namespace

Plan build_plan(const Config& config) {
  const auto derived = derive_config(config);
  const auto execution = execute_config(config, derived);
  Plan plan;
  plan.config = config;
  plan.cache_config = derived.cache_config;
  plan.no_prefix_stats = execution.no_prefix_stats;
  plan.shared_prefix_stats = execution.shared_prefix_stats;
  plan.traffic_profile = profile_prefix_cache_traffic(
      config.num_requests, config.prefix_tokens,
      config.suffix_tokens_per_request, config.block_size_tokens,
      config.bytes_per_token);
  validate_cross_result(config, plan.no_prefix_stats,
                        plan.shared_prefix_stats, plan.traffic_profile);
  plan.reserved_bytes_delta = internal::checked_add_i64(
      plan.no_prefix_stats.reserved_bytes,
      -plan.shared_prefix_stats.reserved_bytes,
      "prefix cache reserved byte delta");
  plan.reserved_bytes_ratio = probe::paged_kv_probe_ratio(
      plan.shared_prefix_stats.reserved_bytes,
      plan.no_prefix_stats.reserved_bytes,
      "prefix cache shared/no-prefix reserved ratio");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  const auto derived = derive_config(plan.config);
  if (!same_cache_config(plan.cache_config, derived.cache_config)) {
    throw std::logic_error(
        "prefix cache comparison cache config is inconsistent");
  }
  validate_cross_result(plan.config, plan.no_prefix_stats,
                        plan.shared_prefix_stats, plan.traffic_profile);
  const auto expected_profile = profile_prefix_cache_traffic(
      plan.config.num_requests, plan.config.prefix_tokens,
      plan.config.suffix_tokens_per_request,
      plan.config.block_size_tokens, plan.config.bytes_per_token);
  const auto expected_execution = execute_config(plan.config, derived);
  if (!same_profile(plan.traffic_profile, expected_profile) ||
      !same_stats(plan.no_prefix_stats,
                  expected_execution.no_prefix_stats) ||
      !same_stats(plan.shared_prefix_stats,
                  expected_execution.shared_prefix_stats)) {
    throw std::logic_error(
        "prefix cache comparison result is not canonical");
  }
  const auto expected_delta = internal::checked_add_i64(
      plan.no_prefix_stats.reserved_bytes,
      -plan.shared_prefix_stats.reserved_bytes,
      "prefix cache reserved byte delta");
  const auto expected_ratio = probe::paged_kv_probe_ratio(
      plan.shared_prefix_stats.reserved_bytes,
      plan.no_prefix_stats.reserved_bytes,
      "prefix cache shared/no-prefix reserved ratio");
  if (plan.traffic_profile.reserved_bytes_saved != expected_delta ||
      !std::isfinite(plan.traffic_profile.shared_reserved_ratio) ||
      plan.traffic_profile.shared_reserved_ratio != expected_ratio ||
      plan.reserved_bytes_delta != expected_delta ||
      plan.reserved_bytes_ratio != expected_ratio) {
    throw std::logic_error(
        "prefix cache comparison savings are inconsistent");
  }
}

}  // namespace inferlab::prefix_cache_compare
