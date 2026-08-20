#pragma once

#include <vector>

namespace inferlab {

struct CudaVectorAddResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> vector_add_cuda_runtime(const std::vector<float>& a,
                                           const std::vector<float>& b);

CudaVectorAddResult vector_add_cuda_runtime_profiled(
    const std::vector<float>& a, const std::vector<float>& b);

}  // namespace inferlab
