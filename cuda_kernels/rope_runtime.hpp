#pragma once

#include <vector>

namespace inferlab {

struct CudaRopeResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> apply_rope_cuda_runtime(const std::vector<float>& input,
                                           int seq_len, int dim, float base,
                                           int position_offset);

CudaRopeResult apply_rope_cuda_runtime_profiled(
    const std::vector<float>& input, int seq_len, int dim, float base,
    int position_offset);

}  // namespace inferlab
