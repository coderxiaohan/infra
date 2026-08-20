#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace inferlab {

using RequestId = std::int64_t;
using PrefixId = std::int64_t;
using BlockId = int;

struct PagedKVCacheConfig {
  int num_blocks = 0;
  int block_size_tokens = 0;
  int bytes_per_token = 0;
};

struct RequestBlocks {
  RequestId request_id = 0;
  int token_count = 0;
  int shared_prefix_tokens = 0;
  int private_token_count = 0;
  int private_block_start = 0;
  std::optional<PrefixId> prefix_id;
  std::vector<BlockId> block_table;
};

struct PrefixCacheEntry {
  PrefixId prefix_id = 0;
  int token_count = 0;
  int ref_count = 0;
  std::vector<BlockId> block_table;
};

struct BlockLocation {
  BlockId block_id = -1;
  int block_offset = 0;
};

struct AllocationStats {
  int total_blocks = 0;
  int free_blocks = 0;
  int used_blocks = 0;
  int active_requests = 0;
  std::int64_t total_tokens = 0;
  std::int64_t used_bytes = 0;
  std::int64_t reserved_bytes = 0;
  double utilization = 0.0;
};

struct PrefixCacheTrafficProfile {
  int num_requests = 0;
  int prefix_tokens = 0;
  int suffix_tokens_per_request = 0;
  int total_tokens_per_request = 0;
  int block_size_tokens = 0;
  int bytes_per_token = 0;
  int prefix_blocks = 0;
  int suffix_blocks_per_request = 0;
  int no_prefix_blocks_per_request = 0;
  std::int64_t no_prefix_physical_tokens = 0;
  std::int64_t shared_prefix_physical_tokens = 0;
  std::int64_t physical_tokens_saved = 0;
  std::int64_t no_prefix_prefill_write_bytes = 0;
  std::int64_t shared_prefix_prefill_write_bytes = 0;
  std::int64_t prefix_write_bytes_saved = 0;
  std::int64_t no_prefix_reserved_bytes = 0;
  std::int64_t shared_prefix_reserved_bytes = 0;
  std::int64_t reserved_bytes_saved = 0;
  double shared_reserved_ratio = 0.0;
  std::int64_t no_prefix_request_block_table_entries = 0;
  std::int64_t shared_request_block_table_entries = 0;
  std::int64_t shared_prefix_entry_block_table_entries = 0;
  std::int64_t decode_attention_kv_read_bytes_per_step = 0;
  std::int64_t decode_attention_shared_prefix_read_bytes_per_step = 0;
};

class PagedKVCacheManager {
 public:
  explicit PagedKVCacheManager(PagedKVCacheConfig config);

  bool add_request(RequestId request_id, int initial_tokens);
  bool create_prefix(PrefixId prefix_id, int prefix_tokens);
  bool add_request_with_prefix(RequestId request_id, PrefixId prefix_id,
                               int suffix_tokens);
  bool append_tokens(RequestId request_id, int new_tokens);
  bool release_request(RequestId request_id);

  bool contains(RequestId request_id) const;
  std::optional<RequestBlocks> request_blocks(RequestId request_id) const;
  std::optional<PrefixCacheEntry> prefix_entry(PrefixId prefix_id) const;
  std::optional<BlockLocation> locate_token(RequestId request_id,
                                            int token_index) const;
  AllocationStats stats() const;
  // Reports whether a request can ever fit in an empty cache. Current free
  // capacity is enforced by add_request(), which may return backpressure.
  bool can_fit_request(int initial_tokens) const;

 private:
  int blocks_required(int tokens) const;
  bool ensure_private_capacity(RequestBlocks& request,
                               int target_private_tokens);
  BlockId acquire_block();
  void release_blocks(const std::vector<BlockId>& blocks);

  PagedKVCacheConfig config_;
  std::vector<BlockId> free_blocks_;
  std::unordered_map<RequestId, RequestBlocks> requests_;
  std::unordered_map<PrefixId, PrefixCacheEntry> prefixes_;
};

PrefixCacheTrafficProfile profile_prefix_cache_traffic(
    int num_requests, int prefix_tokens, int suffix_tokens_per_request,
    int block_size_tokens, int bytes_per_token);

}  // namespace inferlab
