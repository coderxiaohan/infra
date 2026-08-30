#include "inferlab/normalization.hpp"
#include "normalization_runtime.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void test_rms_norm_cuda_matches_cpu() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F, 4.0F,
      4.0F, 3.0F, 2.0F, 1.0F,
  };
  const std::vector<float> gamma = {1.0F, 0.5F, 1.5F, 2.0F};
  const auto expected = inferlab::rms_norm_cpu(matrix, 2, 4, gamma, 1e-6F);
  const auto profiled =
      inferlab::rms_norm_cuda_runtime_profiled(matrix, 2, 4, gamma, 1e-6F);
  const auto& actual = profiled.output;

  require(actual.size() == expected.size(), "RMSNorm CUDA size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-5F,
            "RMSNorm CUDA output mismatch");
  }
  require(profiled.elapsed_ms >= 0.0F, "RMSNorm CUDA elapsed time invalid");
}

void test_rms_norm_cuda_covers_multiple_blocks_and_long_rows() {
  constexpr int kRows = 129;
  constexpr int kCols = 257;
  std::vector<float> matrix(static_cast<std::size_t>(kRows * kCols));
  std::vector<float> gamma(static_cast<std::size_t>(kCols));
  for (std::size_t index = 0; index < matrix.size(); ++index) {
    matrix[index] =
        static_cast<float>(static_cast<int>(index % 17) - 8) * 0.125F;
  }
  for (int col = 0; col < kCols; ++col) {
    gamma[static_cast<std::size_t>(col)] =
        0.5F + static_cast<float>(col % 5) * 0.125F;
  }

  const auto expected =
      inferlab::rms_norm_cpu(matrix, kRows, kCols, gamma, 1e-6F);
  const auto actual =
      inferlab::rms_norm_cuda_runtime(matrix, kRows, kCols, gamma, 1e-6F);
  require(actual.size() == expected.size(),
          "multi-block RMSNorm CUDA size mismatch");
  for (std::size_t index = 0; index < actual.size(); ++index) {
    require(std::fabs(actual[index] - expected[index]) < 1e-5F,
            "multi-block RMSNorm CUDA output mismatch");
  }
}

}  // namespace

int main() {
  try {
    test_rms_norm_cuda_matches_cpu();
    test_rms_norm_cuda_covers_multiple_blocks_and_long_rows();
  } catch (const std::exception& error) {
    std::cerr << "normalization_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
