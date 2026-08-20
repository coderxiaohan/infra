#pragma once

#include <vector>

#include "inferlab/quantization.hpp"

namespace inferlab {

struct CudaQuantizationResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> fused_dequant_matvec_cuda_runtime(
    const QuantizedMatrix& matrix, const std::vector<float>& activation);

CudaQuantizationResult fused_dequant_matvec_cuda_runtime_profiled(
    const QuantizedMatrix& matrix, const std::vector<float>& activation);

}  // namespace inferlab
