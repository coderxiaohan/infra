#include "inferlab/rope.hpp"
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

float squared_norm(const std::vector<float>& values, const int begin, const int end) {
  float total = 0.0F;
  for (int i = begin; i < end; ++i) {
    total += values[static_cast<std::size_t>(i)] *
             values[static_cast<std::size_t>(i)];
  }
  return total;
}

void test_rope_position_zero_is_identity() {
  const std::vector<float> input = {
      1.0F, 2.0F, 3.0F, 4.0F,
      5.0F, 6.0F, 7.0F, 8.0F,
  };
  const auto output = inferlab::apply_rope_cpu(input, 2, 4, 10000.0F);
  for (int i = 0; i < 4; ++i) {
    require(std::fabs(output[static_cast<std::size_t>(i)] -
                      input[static_cast<std::size_t>(i)]) < 1e-6F,
            "position zero should be unchanged");
  }
}

void test_rope_preserves_pairwise_norm() {
  const std::vector<float> input = {
      1.0F, 2.0F,
      3.0F, 4.0F,
  };
  const auto output = inferlab::apply_rope_cpu(input, 2, 2, 10000.0F);
  require(std::fabs(squared_norm(input, 2, 4) - squared_norm(output, 2, 4)) <
              1e-5F,
          "RoPE should preserve pair norm");
}

void test_rope_rejects_odd_dim() {
  bool threw = false;
  try {
    (void)inferlab::apply_rope_cpu({1.0F, 2.0F, 3.0F}, 1, 3, 10000.0F);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "odd dim did not throw");
}

}  // namespace

int main() {
  try {
    test_rope_position_zero_is_identity();
    test_rope_preserves_pairwise_norm();
    test_rope_rejects_odd_dim();
  } catch (const std::exception& error) {
    std::cerr << "rope_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
