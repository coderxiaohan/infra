#include "inferlab/matrix_ops.hpp"
#include "test_error_redaction.hpp"

#include <cuda_runtime.h>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" __global__ void inferlab_row_softmax_f32(
    const float* input, float* output, int rows, int cols);

namespace {

void check_cuda(const cudaError_t status, const char* operation) {
  if (status != cudaSuccess) {
    throw std::runtime_error(std::string(operation) + ": " +
                             cudaGetErrorString(status));
  }
}

void test_non_power_of_two_block_matches_cpu() {
  constexpr int kRows = 3;
  constexpr int kCols = 521;
  constexpr int kThreads = 191;
  std::vector<float> input(static_cast<std::size_t>(kRows * kCols));
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      input[static_cast<std::size_t>(row * kCols + col)] =
          static_cast<float>(((row * 17 + col * 7) % 31) - 15) / 8.0F;
    }
  }
  const auto expected = inferlab::row_softmax_cpu(input, kRows, kCols);

  float* device_input = nullptr;
  float* device_output = nullptr;
  const std::size_t bytes = input.size() * sizeof(float);
  check_cuda(cudaMalloc(&device_input, bytes), "cudaMalloc input");
  try {
    check_cuda(cudaMalloc(&device_output, bytes), "cudaMalloc output");
    check_cuda(cudaMemcpy(device_input, input.data(), bytes,
                          cudaMemcpyHostToDevice),
               "cudaMemcpy input");
    inferlab_row_softmax_f32<<<kRows, kThreads,
                               kThreads * sizeof(float)>>>(
        device_input, device_output, kRows, kCols);
    check_cuda(cudaGetLastError(), "inferlab_row_softmax_f32 launch");
    check_cuda(cudaDeviceSynchronize(), "cudaDeviceSynchronize");
    std::vector<float> actual(input.size());
    check_cuda(cudaMemcpy(actual.data(), device_output, bytes,
                          cudaMemcpyDeviceToHost),
               "cudaMemcpy output");
    for (std::size_t index = 0; index < actual.size(); ++index) {
      if (std::fabs(actual[index] - expected[index]) >= 1e-6F) {
        throw std::runtime_error(
            "non-power-of-two softmax reduction mismatch");
      }
    }
  } catch (...) {
    (void)cudaFree(device_output);
    (void)cudaFree(device_input);
    throw;
  }
  (void)cudaFree(device_output);
  (void)cudaFree(device_input);
}

}  // namespace

int main() {
  try {
    test_non_power_of_two_block_matches_cpu();
  } catch (const std::exception& error) {
    std::cerr << "softmax_kernel_cuda_test failed: "
              << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
