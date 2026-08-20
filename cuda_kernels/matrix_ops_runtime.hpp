#pragma once

#include <vector>

namespace inferlab {

struct CudaMatrixOpResult {
  std::vector<float> output;
  float elapsed_ms = 0.0F;
};

std::vector<float> matvec_cuda_runtime(const std::vector<float>& matrix,
                                       int rows, int cols,
                                       const std::vector<float>& vector);

CudaMatrixOpResult matvec_cuda_runtime_profiled(
    const std::vector<float>& matrix, int rows, int cols,
    const std::vector<float>& vector);

std::vector<float> matvec_shared_cuda_runtime(
    const std::vector<float>& matrix, int rows, int cols,
    const std::vector<float>& vector);

CudaMatrixOpResult matvec_shared_cuda_runtime_profiled(
    const std::vector<float>& matrix, int rows, int cols,
    const std::vector<float>& vector);

std::vector<float> matmul_cuda_runtime(const std::vector<float>& a, int m,
                                       int k, const std::vector<float>& b,
                                       int n);

CudaMatrixOpResult matmul_cuda_runtime_profiled(
    const std::vector<float>& a, int m, int k, const std::vector<float>& b,
    int n);

std::vector<float> matmul_tiled_cuda_runtime(const std::vector<float>& a,
                                             int m, int k,
                                             const std::vector<float>& b,
                                             int n);

CudaMatrixOpResult matmul_tiled_cuda_runtime_profiled(
    const std::vector<float>& a, int m, int k, const std::vector<float>& b,
    int n);

std::vector<float> row_softmax_cuda_runtime(const std::vector<float>& matrix,
                                            int rows, int cols);

CudaMatrixOpResult row_softmax_cuda_runtime_profiled(
    const std::vector<float>& matrix, int rows, int cols);

std::vector<float> row_softmax_block_cuda_runtime(
    const std::vector<float>& matrix, int rows, int cols);

CudaMatrixOpResult row_softmax_block_cuda_runtime_profiled(
    const std::vector<float>& matrix, int rows, int cols);

}  // namespace inferlab
