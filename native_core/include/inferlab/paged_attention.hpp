#pragma once

#include <vector>

#include "inferlab/paged_kv_cache.hpp"

namespace inferlab {

struct PagedKVShape {
  int num_heads = 0;
  int head_dim = 0;
  int block_size_tokens = 0;
};

// Gathers logical tokens in head-major order while respecting shared-prefix
// blocks and the separate private block segment in RequestBlocks.
std::vector<float> gather_paged_kv_cache(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape);

}  // namespace inferlab
