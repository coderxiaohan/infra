#include "inferlab/vector_add.hpp"
#include "vector_add_runtime.hpp"
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

void test_cuda_vector_add_matches_cpu() {
  std::vector<float> a(1024, 0.0F);
  std::vector<float> b(1024, 0.0F);
  for (std::size_t i = 0; i < a.size(); ++i) {
    a[i] = static_cast<float>(static_cast<int>(i % 17) - 8) / 7.0F;
    b[i] = static_cast<float>(static_cast<int>(i % 11) - 5) / 5.0F;
  }
  const auto expected = inferlab::vector_add_cpu(a, b);
  const auto result = inferlab::vector_add_cuda_runtime_profiled(a, b);
  const auto& actual = result.output;

  require(actual.size() == expected.size(), "CUDA output size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-6F,
            "CUDA output mismatch");
  }
  require(result.elapsed_ms >= 0.0F, "CUDA elapsed time should be non-negative");
}

}  // namespace

int main() {
  try {
    test_cuda_vector_add_matches_cpu();
  } catch (const std::exception& error) {
    std::cerr << "vector_add_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
