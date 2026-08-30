#include "inferlab/quantization.hpp"
#include "quantization_runtime.hpp"
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

void test_int4_fused_dequant_matvec_cuda_matches_cpu() {
  const std::vector<float> weights = {
      1.0F, -0.25F, 0.5F, 2.0F,
      -1.0F, 0.75F, 1.5F, -2.0F,
  };
  const std::vector<float> activation = {1.0F, -2.0F, 0.5F, 0.25F};
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 2, 4, 2, inferlab::QuantizationBits::kInt4);
  const auto expected = inferlab::fused_dequant_matvec(matrix, activation);
  const auto profiled =
      inferlab::fused_dequant_matvec_cuda_runtime_profiled(matrix, activation);
  const auto& actual = profiled.output;

  require(actual.size() == expected.size(), "INT4 CUDA matvec size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-5F,
            "INT4 CUDA matvec output mismatch");
  }
  require(profiled.elapsed_ms >= 0.0F,
          "INT4 CUDA matvec elapsed time invalid");
}

void test_int4_cuda_covers_multiple_blocks_and_groups() {
  constexpr int kRows = 129;
  constexpr int kCols = 259;
  constexpr int kGroupSize = 37;
  std::vector<float> weights(static_cast<std::size_t>(kRows * kCols));
  std::vector<float> activation(static_cast<std::size_t>(kCols));
  for (std::size_t index = 0; index < weights.size(); ++index) {
    weights[index] =
        static_cast<float>(static_cast<int>(index % 19) - 9) * 0.125F;
  }
  for (int col = 0; col < kCols; ++col) {
    activation[static_cast<std::size_t>(col)] =
        static_cast<float>(col % 11 - 5) * 0.0625F;
  }
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, kRows, kCols, kGroupSize, inferlab::QuantizationBits::kInt4);
  const auto expected = inferlab::fused_dequant_matvec(matrix, activation);
  const auto actual =
      inferlab::fused_dequant_matvec_cuda_runtime(matrix, activation);

  require(actual.size() == expected.size(),
          "multi-block INT4 CUDA matvec size mismatch");
  for (std::size_t index = 0; index < actual.size(); ++index) {
    require(std::fabs(actual[index] - expected[index]) < 1e-5F,
            "multi-block INT4 CUDA matvec output mismatch");
  }
}

}  // namespace

int main() {
  try {
    test_int4_fused_dequant_matvec_cuda_matches_cpu();
    test_int4_cuda_covers_multiple_blocks_and_groups();
  } catch (const std::exception& error) {
    std::cerr << "quantization_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
