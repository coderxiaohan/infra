#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "inferlab/paged_kv_cache.hpp"

namespace inferlab {

struct StaticKVCacheConfig {
  int max_requests = 0;
  int max_sequence_length = 0;
  int bytes_per_token = 0;
};

struct StaticRequestAllocation {
  int token_count = 0;
  int slot = -1;
};

struct StaticAllocationStats {
  int max_requests = 0;
  int active_requests = 0;
  std::int64_t total_tokens = 0;
  std::int64_t used_bytes = 0;
  std::int64_t reserved_bytes = 0;
  double utilization = 0.0;
};

class StaticKVCacheManager {
 public:
  explicit StaticKVCacheManager(StaticKVCacheConfig config);

  bool add_request(RequestId request_id, int initial_tokens);
  bool append_tokens(RequestId request_id, int new_tokens);
  bool release_request(RequestId request_id);

  std::optional<StaticRequestAllocation> request(RequestId request_id) const;
  StaticAllocationStats stats() const;

 private:
  int acquire_slot();

  StaticKVCacheConfig config_;
  std::vector<int> free_slots_;
  std::unordered_map<RequestId, StaticRequestAllocation> requests_;
};

}  // namespace inferlab
