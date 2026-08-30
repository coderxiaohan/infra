#include "inferlab/rope.hpp"
#include "rope_runtime.hpp"
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

void test_rope_cuda_matches_cpu_with_offset() {
  const std::vector<float> input = {
      1.0F, 2.0F, 3.0F, 4.0F,
      5.0F, 6.0F, 7.0F, 8.0F,
      9.0F, 10.0F, 11.0F, 12.0F,
  };
  const auto expected = inferlab::apply_rope_cpu(input, 3, 4, 10000.0F, 5);
  const auto profiled =
      inferlab::apply_rope_cuda_runtime_profiled(input, 3, 4, 10000.0F, 5);
  const auto& actual = profiled.output;

  require(actual.size() == expected.size(), "RoPE CUDA size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-5F,
            "RoPE CUDA output mismatch");
  }
  require(profiled.elapsed_ms >= 0.0F, "RoPE CUDA elapsed time invalid");
}

void test_rope_cuda_covers_multiple_blocks() {
  constexpr int kSeqLen = 17;
  constexpr int kDim = 34;
  std::vector<float> input(static_cast<std::size_t>(kSeqLen * kDim));
  for (std::size_t index = 0; index < input.size(); ++index) {
    input[index] =
        static_cast<float>(static_cast<int>(index % 29) - 14) * 0.0625F;
  }
  const auto expected =
      inferlab::apply_rope_cpu(input, kSeqLen, kDim, 10000.0F, 11);
  const auto actual =
      inferlab::apply_rope_cuda_runtime(input, kSeqLen, kDim, 10000.0F, 11);

  require(actual.size() == expected.size(),
          "multi-block RoPE CUDA size mismatch");
  for (std::size_t index = 0; index < actual.size(); ++index) {
    require(std::fabs(actual[index] - expected[index]) < 1e-4F,
            "multi-block RoPE CUDA output mismatch");
  }
}

}  // namespace

int main() {
  try {
    test_rope_cuda_matches_cpu_with_offset();
    test_rope_cuda_covers_multiple_blocks();
  } catch (const std::exception& error) {
    std::cerr << "rope_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
