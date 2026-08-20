#pragma once

#include <vector>

#include "inferlab/paged_attention.hpp"

namespace inferlab {

struct CudaPagedKVResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> gather_paged_kv_cache_cuda_runtime(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape);

CudaPagedKVResult gather_paged_kv_cache_cuda_runtime_profiled(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape);

}  // namespace inferlab
