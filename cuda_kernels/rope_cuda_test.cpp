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

}  // namespace

int main() {
  try {
    test_rope_cuda_matches_cpu_with_offset();
  } catch (const std::exception& error) {
    std::cerr << "rope_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
