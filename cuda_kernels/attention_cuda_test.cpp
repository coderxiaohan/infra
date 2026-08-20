#include "attention_runtime.hpp"
#include "test_error_redaction.hpp"
#include "inferlab/attention.hpp"

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

void require_close(const std::vector<float>& actual,
                   const std::vector<float>& expected,
                   const std::string& message) {
  require(actual.size() == expected.size(), message + " size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-5F, message);
  }
}

void test_attention_cuda_matches_cpu() {
  const inferlab::AttentionShape shape{2, 1, 2, 2};
  const std::vector<float> query = {
      1.0F, 0.0F,
      0.0F, 1.0F,
  };
  const std::vector<float> key = {
      1.0F, 0.0F,
      0.0F, 1.0F,
      0.0F, 1.0F,
      1.0F, 0.0F,
  };
  const std::vector<float> value = {
      10.0F, 0.0F,
      0.0F, 20.0F,
      30.0F, 40.0F,
      50.0F, 60.0F,
  };
  const auto profiled =
      inferlab::scaled_dot_product_attention_cuda_runtime_profiled(
          query, key, value, shape, false);
  require_close(profiled.output,
                inferlab::scaled_dot_product_attention_cpu(query, key, value,
                                                           shape, false),
                "CUDA attention mismatch");
  require(profiled.elapsed_ms >= 0.0F, "CUDA attention elapsed time invalid");
  const auto shared_profiled =
      inferlab::scaled_dot_product_attention_shared_cuda_runtime_profiled(
          query, key, value, shape, false);
  require_close(shared_profiled.output,
                inferlab::scaled_dot_product_attention_cpu(query, key, value,
                                                           shape, false),
                "CUDA shared attention mismatch");
  require(shared_profiled.elapsed_ms >= 0.0F,
          "CUDA shared attention elapsed time invalid");
}

void test_causal_attention_cuda_matches_cpu() {
  const inferlab::AttentionShape shape{1, 2, 2, 1};
  const std::vector<float> query = {1.0F, 1.0F};
  const std::vector<float> key = {1.0F, 1.0F};
  const std::vector<float> value = {10.0F, 20.0F};
  const auto profiled =
      inferlab::scaled_dot_product_attention_cuda_runtime_profiled(
          query, key, value, shape, true);
  require_close(profiled.output,
                inferlab::scaled_dot_product_attention_cpu(query, key, value,
                                                           shape, true),
                "CUDA causal attention mismatch");
  require(profiled.elapsed_ms >= 0.0F,
          "CUDA causal attention elapsed time invalid");
  const auto shared_profiled =
      inferlab::scaled_dot_product_attention_shared_cuda_runtime_profiled(
          query, key, value, shape, true);
  require_close(shared_profiled.output,
                inferlab::scaled_dot_product_attention_cpu(query, key, value,
                                                           shape, true),
                "CUDA shared causal attention mismatch");
  require(shared_profiled.elapsed_ms >= 0.0F,
          "CUDA shared causal attention elapsed time invalid");
}

}  // namespace

int main() {
  try {
    test_attention_cuda_matches_cpu();
    test_causal_attention_cuda_matches_cpu();
  } catch (const std::exception& error) {
    std::cerr << "attention_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
