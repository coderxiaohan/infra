#pragma once

#include <vector>

namespace inferlab {

struct CudaNormalizationResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> rms_norm_cuda_runtime(const std::vector<float>& matrix,
                                         int rows, int cols,
                                         const std::vector<float>& gamma,
                                         float epsilon);

CudaNormalizationResult rms_norm_cuda_runtime_profiled(
    const std::vector<float>& matrix, int rows, int cols,
    const std::vector<float>& gamma, float epsilon);

}  // namespace inferlab
