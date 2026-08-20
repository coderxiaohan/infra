#include "inferlab/static_kv_cache.hpp"

#include <stdexcept>
#include <string>

#include "checked_index.hpp"

namespace inferlab {
namespace {

bool checked_add_int_to(const int lhs, const int rhs, const char* name,
                        int& output) {
  try {
    output = internal::checked_add_int(lhs, rhs, name);
  } catch (const std::invalid_argument&) {
    return false;
  }
  return true;
}

std::int64_t checked_i64_from_int(const int value, const char* name) {
  if (value < 0) {
    throw std::invalid_argument(std::string(name) + " must be non-negative");
  }
  return static_cast<std::int64_t>(value);
}

}  // namespace

StaticKVCacheManager::StaticKVCacheManager(StaticKVCacheConfig config)
    : config_(config) {
  if (config_.max_requests <= 0) {
    throw std::invalid_argument("max_requests must be positive");
  }
  if (config_.max_sequence_length <= 0) {
    throw std::invalid_argument("max_sequence_length must be positive");
  }
  if (config_.bytes_per_token <= 0) {
    throw std::invalid_argument("bytes_per_token must be positive");
  }

  free_slots_.reserve(static_cast<std::size_t>(config_.max_requests));
  for (int slot = config_.max_requests - 1; slot >= 0; --slot) {
    free_slots_.push_back(slot);
  }
}

bool StaticKVCacheManager::add_request(const RequestId request_id,
                                       const int initial_tokens) {
  if (request_id <= 0 || initial_tokens < 0 ||
      initial_tokens > config_.max_sequence_length ||
      requests_.find(request_id) != requests_.end() || free_slots_.empty()) {
    return false;
  }

  requests_.emplace(request_id,
                    StaticRequestAllocation{initial_tokens, acquire_slot()});
  return true;
}

bool StaticKVCacheManager::append_tokens(const RequestId request_id,
                                         const int new_tokens) {
  if (new_tokens < 0) {
    return false;
  }
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return false;
  }

  int target_tokens = 0;
  if (!checked_add_int_to(it->second.token_count, new_tokens,
                          "static KV target tokens", target_tokens)) {
    return false;
  }
  if (target_tokens > config_.max_sequence_length) {
    return false;
  }
  it->second.token_count = target_tokens;
  return true;
}

bool StaticKVCacheManager::release_request(const RequestId request_id) {
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return false;
  }
  free_slots_.push_back(it->second.slot);
  requests_.erase(it);
  return true;
}

std::optional<StaticRequestAllocation> StaticKVCacheManager::request(
    const RequestId request_id) const {
  const auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return std::nullopt;
  }
  return it->second;
}

StaticAllocationStats StaticKVCacheManager::stats() const {
  StaticAllocationStats stats;
  stats.max_requests = config_.max_requests;
  stats.active_requests =
      internal::checked_int_from_size(requests_.size(), "static KV active requests");

  for (const auto& [_, request] : requests_) {
    stats.total_tokens = internal::checked_add_i64(
        stats.total_tokens, checked_i64_from_int(request.token_count,
                                                 "static KV request tokens"),
        "static KV total tokens");
  }

  stats.used_bytes = internal::checked_product_i64(
      stats.total_tokens,
      checked_i64_from_int(config_.bytes_per_token, "static KV bytes_per_token"),
      "static KV used bytes");
  stats.reserved_bytes = internal::checked_product_i64(
      checked_i64_from_int(stats.active_requests, "static KV active requests"),
      checked_i64_from_int(config_.max_sequence_length,
                           "static KV max sequence length"),
      checked_i64_from_int(config_.bytes_per_token,
                           "static KV bytes_per_token"),
      "static KV reserved bytes");
  if (stats.reserved_bytes > 0) {
    stats.utilization =
        static_cast<double>(stats.used_bytes) / stats.reserved_bytes;
  }
  return stats;
}

int StaticKVCacheManager::acquire_slot() {
  if (free_slots_.empty()) {
    throw std::runtime_error("no free static KV cache slots");
  }
  const auto slot = free_slots_.back();
  free_slots_.pop_back();
  return slot;
}

}  // namespace inferlab
