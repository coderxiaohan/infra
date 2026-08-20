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

}  // namespace

int main() {
  try {
    test_int4_fused_dequant_matvec_cuda_matches_cpu();
  } catch (const std::exception& error) {
    std::cerr << "quantization_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
