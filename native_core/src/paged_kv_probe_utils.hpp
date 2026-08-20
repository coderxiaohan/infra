#pragma once

#include "inferlab/paged_kv_cache.hpp"

#include "checked_index.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace inferlab::probe {

inline constexpr int kMaxPagedKVProbeBlocks = 1048576;

inline int require_paged_kv_probe_capacity(const int block_count,
                                           const char* name) {
  if (block_count <= 0 || block_count > kMaxPagedKVProbeBlocks) {
    throw std::invalid_argument(
        std::string(name) + " must be between 1 and " +
        std::to_string(kMaxPagedKVProbeBlocks));
  }
  return block_count;
}

inline void validate_paged_kv_probe_stats(const AllocationStats& stats,
                                          const char* name) {
  if (stats.total_blocks <= 0 ||
      stats.total_blocks > kMaxPagedKVProbeBlocks || stats.free_blocks < 0 ||
      stats.used_blocks < 0 || stats.active_requests < 0 ||
      stats.total_tokens < 0 || stats.used_bytes < 0 ||
      stats.reserved_bytes < 0 || !std::isfinite(stats.utilization) ||
      stats.utilization < 0.0 || stats.utilization > 1.0) {
    throw std::logic_error(std::string(name) +
                           " contains invalid allocation statistics");
  }
  if (internal::checked_add_int(stats.free_blocks, stats.used_blocks,
                                "paged KV probe block accounting") !=
          stats.total_blocks ||
      stats.used_bytes > stats.reserved_bytes ||
      (stats.used_blocks == 0 &&
       (stats.total_tokens != 0 || stats.used_bytes != 0 ||
        stats.reserved_bytes != 0 || stats.utilization != 0.0)) ||
      (stats.reserved_bytes == 0 && stats.utilization != 0.0)) {
    throw std::logic_error(std::string(name) +
                           " has inconsistent allocation statistics");
  }
}

inline void require_empty_paged_kv_probe_cache(const AllocationStats& stats,
                                               const char* name) {
  validate_paged_kv_probe_stats(stats, name);
  if (stats.active_requests != 0 || stats.used_blocks != 0 ||
      stats.total_tokens != 0) {
    throw std::logic_error(std::string(name) +
                           " completed with live KV state");
  }
}

inline double paged_kv_probe_ratio(const std::int64_t numerator,
                                   const std::int64_t denominator,
                                   const char* name) {
  if (numerator < 0 || denominator < 0) {
    throw std::invalid_argument(std::string(name) +
                                " values must be non-negative");
  }
  if (denominator == 0) {
    if (numerator != 0) {
      throw std::logic_error(std::string(name) +
                             " has a zero denominator");
    }
    return 0.0;
  }
  const double ratio = static_cast<double>(numerator) /
                       static_cast<double>(denominator);
  if (!std::isfinite(ratio)) {
    throw std::overflow_error(std::string(name) + " is non-finite");
  }
  return ratio;
}

}  // namespace inferlab::probe
