#include "inferlab/paged_kv_cache.hpp"

#include <algorithm>
#include <functional>
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

PagedKVCacheManager::PagedKVCacheManager(PagedKVCacheConfig config)
    : config_(config) {
  if (config_.num_blocks <= 0) {
    throw std::invalid_argument("num_blocks must be positive");
  }
  if (config_.block_size_tokens <= 0) {
    throw std::invalid_argument("block_size_tokens must be positive");
  }
  if (config_.bytes_per_token <= 0) {
    throw std::invalid_argument("bytes_per_token must be positive");
  }
  free_blocks_.reserve(static_cast<std::size_t>(config_.num_blocks));
  for (int block = config_.num_blocks - 1; block >= 0; --block) {
    free_blocks_.push_back(block);
  }
}

bool PagedKVCacheManager::add_request(const RequestId request_id,
                                      const int initial_tokens) {
  if (request_id <= 0 || initial_tokens < 0 || contains(request_id)) {
    return false;
  }

  RequestBlocks request;
  request.request_id = request_id;
  if (!ensure_private_capacity(request, initial_tokens)) {
    return false;
  }
  request.private_token_count = initial_tokens;
  request.token_count = initial_tokens;
  requests_.emplace(request_id, std::move(request));
  return true;
}

bool PagedKVCacheManager::create_prefix(const PrefixId prefix_id,
                                        const int prefix_tokens) {
  if (prefix_id <= 0 || prefix_tokens <= 0 ||
      prefixes_.find(prefix_id) != prefixes_.end()) {
    return false;
  }
  const int needed_blocks = blocks_required(prefix_tokens);
  if (needed_blocks >
      internal::checked_int_from_size(free_blocks_.size(), "free block count")) {
    return false;
  }

  PrefixCacheEntry entry;
  entry.prefix_id = prefix_id;
  entry.token_count = prefix_tokens;
  entry.block_table.reserve(static_cast<std::size_t>(needed_blocks));
  for (int i = 0; i < needed_blocks; ++i) {
    entry.block_table.push_back(acquire_block());
  }
  prefixes_.emplace(prefix_id, std::move(entry));
  return true;
}

bool PagedKVCacheManager::add_request_with_prefix(
    const RequestId request_id, const PrefixId prefix_id,
    const int suffix_tokens) {
  if (request_id <= 0 || suffix_tokens < 0 || contains(request_id)) {
    return false;
  }
  auto prefix_it = prefixes_.find(prefix_id);
  if (prefix_it == prefixes_.end()) {
    return false;
  }

  RequestBlocks request;
  request.request_id = request_id;
  request.prefix_id = prefix_id;
  request.shared_prefix_tokens = prefix_it->second.token_count;
  request.private_block_start = internal::checked_int_from_size(
      prefix_it->second.block_table.size(), "prefix block table size");
  request.block_table = prefix_it->second.block_table;

  int total_tokens = 0;
  if (!checked_add_int_to(request.shared_prefix_tokens, suffix_tokens,
                          "prefixed request tokens", total_tokens)) {
    return false;
  }
  int next_prefix_ref_count = 0;
  if (!checked_add_int_to(prefix_it->second.ref_count, 1,
                          "prefix reference count", next_prefix_ref_count)) {
    return false;
  }
  if (!ensure_private_capacity(request, suffix_tokens)) {
    return false;
  }

  request.private_token_count = suffix_tokens;
  request.token_count = total_tokens;
  requests_.emplace(request_id, std::move(request));
  prefix_it->second.ref_count = next_prefix_ref_count;
  return true;
}

bool PagedKVCacheManager::append_tokens(const RequestId request_id,
                                        const int new_tokens) {
  if (new_tokens < 0) {
    return false;
  }
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return false;
  }

  auto& request = it->second;
  int target_private_tokens = 0;
  if (!checked_add_int_to(request.private_token_count, new_tokens,
                          "request private tokens", target_private_tokens)) {
    return false;
  }
  int total_tokens = 0;
  if (!checked_add_int_to(request.shared_prefix_tokens, target_private_tokens,
                          "request total tokens", total_tokens)) {
    return false;
  }
  if (!ensure_private_capacity(request, target_private_tokens)) {
    return false;
  }
  request.private_token_count = target_private_tokens;
  request.token_count = total_tokens;
  return true;
}

bool PagedKVCacheManager::release_request(const RequestId request_id) {
  auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return false;
  }
  const auto& request = it->second;
  const std::vector<BlockId> private_blocks(
      request.block_table.begin() + request.private_block_start,
      request.block_table.end());
  release_blocks(private_blocks);

  if (request.prefix_id.has_value()) {
    auto prefix_it = prefixes_.find(*request.prefix_id);
    if (prefix_it != prefixes_.end()) {
      if (prefix_it->second.ref_count <= 1) {
        release_blocks(prefix_it->second.block_table);
        prefixes_.erase(prefix_it);
      } else {
        --prefix_it->second.ref_count;
      }
    }
  }

  requests_.erase(it);
  return true;
}

bool PagedKVCacheManager::contains(const RequestId request_id) const {
  return requests_.find(request_id) != requests_.end();
}

std::optional<RequestBlocks> PagedKVCacheManager::request_blocks(
    const RequestId request_id) const {
  const auto it = requests_.find(request_id);
  if (it == requests_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<PrefixCacheEntry> PagedKVCacheManager::prefix_entry(
    const PrefixId prefix_id) const {
  const auto it = prefixes_.find(prefix_id);
  if (it == prefixes_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::optional<BlockLocation> PagedKVCacheManager::locate_token(
    const RequestId request_id, const int token_index) const {
  const auto it = requests_.find(request_id);
  if (it == requests_.end() || token_index < 0 ||
      token_index >= it->second.token_count) {
    return std::nullopt;
  }
  const auto& request = it->second;
  int logical_block = 0;
  int block_offset = 0;
  if (token_index < request.shared_prefix_tokens) {
    logical_block = token_index / config_.block_size_tokens;
    block_offset = token_index % config_.block_size_tokens;
  } else {
    const int private_token_index = token_index - request.shared_prefix_tokens;
    if (!checked_add_int_to(request.private_block_start,
                            private_token_index / config_.block_size_tokens,
                            "paged KV private logical block",
                            logical_block)) {
      return std::nullopt;
    }
    block_offset = private_token_index % config_.block_size_tokens;
  }
  const auto logical_block_index = static_cast<std::size_t>(logical_block);
  if (logical_block < 0 || logical_block_index >= request.block_table.size()) {
    return std::nullopt;
  }
  return BlockLocation{
      request.block_table[logical_block_index],
      block_offset,
  };
}

AllocationStats PagedKVCacheManager::stats() const {
  AllocationStats stats;
  stats.total_blocks = config_.num_blocks;
  stats.free_blocks =
      internal::checked_int_from_size(free_blocks_.size(), "free block count");
  stats.used_blocks = stats.total_blocks - stats.free_blocks;
  stats.active_requests =
      internal::checked_int_from_size(requests_.size(), "active request count");

  for (const auto& [_, prefix] : prefixes_) {
    stats.total_tokens = internal::checked_add_i64(
        stats.total_tokens,
        checked_i64_from_int(prefix.token_count, "prefix tokens"),
        "paged KV total tokens");
  }
  for (const auto& [_, request] : requests_) {
    stats.total_tokens = internal::checked_add_i64(
        stats.total_tokens,
        checked_i64_from_int(request.private_token_count, "request private tokens"),
        "paged KV total tokens");
  }

  stats.used_bytes = internal::checked_product_i64(
      stats.total_tokens, checked_i64_from_int(config_.bytes_per_token,
                                               "bytes_per_token"),
      "paged KV used bytes");
  stats.reserved_bytes = internal::checked_product_i64(
      checked_i64_from_int(stats.used_blocks, "used blocks"),
      checked_i64_from_int(config_.block_size_tokens, "block_size_tokens"),
      checked_i64_from_int(config_.bytes_per_token, "bytes_per_token"),
      "paged KV reserved bytes");
  if (stats.reserved_bytes > 0) {
    stats.utilization =
        static_cast<double>(stats.used_bytes) / stats.reserved_bytes;
  }
  return stats;
}

int PagedKVCacheManager::blocks_required(const int tokens) const {
  return internal::checked_ceil_div_non_negative_int(
      tokens, config_.block_size_tokens, "paged KV required blocks");
}

bool PagedKVCacheManager::can_fit_request(const int initial_tokens) const {
  if (initial_tokens < 0) {
    return false;
  }
  return blocks_required(initial_tokens) <= config_.num_blocks;
}

bool PagedKVCacheManager::ensure_private_capacity(
    RequestBlocks& request, const int target_private_tokens) {
  if (target_private_tokens < 0) {
    return false;
  }
  int target_blocks = 0;
  if (!checked_add_int_to(request.private_block_start,
                          blocks_required(target_private_tokens),
                          "paged KV target blocks", target_blocks)) {
    return false;
  }
  int current_blocks = 0;
  try {
    current_blocks = internal::checked_int_from_size(
        request.block_table.size(), "paged KV current block count");
  } catch (const std::invalid_argument&) {
    return false;
  }
  const int needed_blocks = target_blocks - current_blocks;
  if (needed_blocks <= 0) {
    return true;
  }
  if (needed_blocks >
      internal::checked_int_from_size(free_blocks_.size(), "free block count")) {
    return false;
  }

  // Reserve before mutating the free list so allocation failure leaves both
  // the request block table and manager capacity unchanged.
  request.block_table.reserve(static_cast<std::size_t>(target_blocks));
  for (int i = 0; i < needed_blocks; ++i) {
    request.block_table.push_back(acquire_block());
  }
  return true;
}

BlockId PagedKVCacheManager::acquire_block() {
  if (free_blocks_.empty()) {
    throw std::runtime_error("no free KV cache blocks");
  }
  const auto block = free_blocks_.back();
  free_blocks_.pop_back();
  return block;
}

void PagedKVCacheManager::release_blocks(const std::vector<BlockId>& blocks) {
  for (const auto block : blocks) {
    free_blocks_.push_back(block);
  }
  std::sort(free_blocks_.begin(), free_blocks_.end(), std::greater<BlockId>());
}

PrefixCacheTrafficProfile profile_prefix_cache_traffic(
    const int num_requests, const int prefix_tokens,
    const int suffix_tokens_per_request, const int block_size_tokens,
    const int bytes_per_token) {
  if (num_requests <= 0) {
    throw std::invalid_argument("num_requests must be positive");
  }
  if (prefix_tokens <= 0) {
    throw std::invalid_argument("prefix_tokens must be positive");
  }
  if (suffix_tokens_per_request < 0) {
    throw std::invalid_argument("suffix_tokens_per_request must be non-negative");
  }
  if (block_size_tokens <= 0 || bytes_per_token <= 0) {
    throw std::invalid_argument(
        "block_size_tokens and bytes_per_token must be positive");
  }

  const auto blocks_required = [block_size_tokens](const int tokens) {
    if (tokens <= 0) {
      return 0;
    }
    return internal::checked_ceil_div_non_negative_int(
        tokens, block_size_tokens, "prefix cache required blocks");
  };

  PrefixCacheTrafficProfile profile;
  profile.num_requests = num_requests;
  profile.prefix_tokens = prefix_tokens;
  profile.suffix_tokens_per_request = suffix_tokens_per_request;
  profile.total_tokens_per_request = internal::checked_add_int(
      prefix_tokens, suffix_tokens_per_request, "prefix total tokens");
  profile.block_size_tokens = block_size_tokens;
  profile.bytes_per_token = bytes_per_token;
  profile.prefix_blocks = blocks_required(prefix_tokens);
  profile.suffix_blocks_per_request = blocks_required(suffix_tokens_per_request);
  profile.no_prefix_blocks_per_request =
      blocks_required(profile.total_tokens_per_request);

  profile.no_prefix_physical_tokens =
      internal::checked_product_i64(num_requests, profile.total_tokens_per_request,
                                    "no-prefix physical tokens");
  profile.shared_prefix_physical_tokens = internal::checked_add_i64(
      prefix_tokens,
      internal::checked_product_i64(num_requests, suffix_tokens_per_request,
                                    "shared suffix physical tokens"),
      "shared prefix physical tokens");
  profile.physical_tokens_saved =
      profile.no_prefix_physical_tokens - profile.shared_prefix_physical_tokens;

  profile.no_prefix_prefill_write_bytes =
      internal::checked_product_i64(profile.no_prefix_physical_tokens,
                                    bytes_per_token,
                                    "no-prefix prefill write bytes");
  profile.shared_prefix_prefill_write_bytes =
      internal::checked_product_i64(profile.shared_prefix_physical_tokens,
                                    bytes_per_token,
                                    "shared prefix prefill write bytes");
  profile.prefix_write_bytes_saved =
      profile.no_prefix_prefill_write_bytes -
      profile.shared_prefix_prefill_write_bytes;

  profile.no_prefix_reserved_bytes =
      internal::checked_product_i64(num_requests,
                                    profile.no_prefix_blocks_per_request,
                                    block_size_tokens, bytes_per_token,
                                    "no-prefix reserved bytes");
  profile.shared_prefix_reserved_bytes =
      internal::checked_product_i64(
          internal::checked_add_i64(
              profile.prefix_blocks,
              internal::checked_product_i64(num_requests,
                                            profile.suffix_blocks_per_request,
                                            "shared suffix blocks"),
              "shared prefix reserved blocks"),
          block_size_tokens, bytes_per_token,
          "shared prefix reserved bytes");
  profile.reserved_bytes_saved =
      profile.no_prefix_reserved_bytes - profile.shared_prefix_reserved_bytes;
  if (profile.no_prefix_reserved_bytes > 0) {
    profile.shared_reserved_ratio =
        static_cast<double>(profile.shared_prefix_reserved_bytes) /
        static_cast<double>(profile.no_prefix_reserved_bytes);
  }

  profile.no_prefix_request_block_table_entries =
      internal::checked_product_i64(num_requests,
                                    profile.no_prefix_blocks_per_request,
                                    "no-prefix block table entries");
  profile.shared_request_block_table_entries =
      internal::checked_product_i64(
          num_requests,
          internal::checked_add_int(profile.prefix_blocks,
                                    profile.suffix_blocks_per_request,
                                    "shared request block table entries"),
          "shared request block table entries");
  profile.shared_prefix_entry_block_table_entries = profile.prefix_blocks;

  profile.decode_attention_kv_read_bytes_per_step =
      internal::checked_product_i64(num_requests,
                                    profile.total_tokens_per_request,
                                    bytes_per_token,
                                    "decode attention KV read bytes");
  profile.decode_attention_shared_prefix_read_bytes_per_step =
      internal::checked_product_i64(num_requests, prefix_tokens, bytes_per_token,
                                    "decode attention shared prefix bytes");

  return profile;
}

}  // namespace inferlab
