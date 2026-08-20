#include "inferlab/normalization.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void test_layer_norm_unit_scale_has_zero_mean() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F,
      2.0F, 4.0F, 6.0F,
  };
  const std::vector<float> gamma = {1.0F, 1.0F, 1.0F};
  const std::vector<float> beta = {0.0F, 0.0F, 0.0F};
  const auto output = inferlab::layer_norm_cpu(matrix, 2, 3, gamma, beta, 1e-5F);
  for (int row = 0; row < 2; ++row) {
    const float mean =
        (output[static_cast<std::size_t>(row * 3)] +
         output[static_cast<std::size_t>(row * 3 + 1)] +
         output[static_cast<std::size_t>(row * 3 + 2)]) /
        3.0F;
    require(std::fabs(mean) < 1e-5F, "layer norm row mean is not zero");
  }
}

void test_rms_norm_simple_row() {
  const std::vector<float> matrix = {3.0F, 4.0F};
  const std::vector<float> gamma = {1.0F, 1.0F};
  const auto output = inferlab::rms_norm_cpu(matrix, 1, 2, gamma, 1e-6F);
  const float inv_rms = 1.0F / std::sqrt((9.0F + 16.0F) / 2.0F + 1e-6F);
  require(std::fabs(output[0] - 3.0F * inv_rms) < 1e-6F,
          "rms norm value 0 mismatch");
  require(std::fabs(output[1] - 4.0F * inv_rms) < 1e-6F,
          "rms norm value 1 mismatch");
}

void test_normalization_rejects_bad_shape() {
  bool threw = false;
  try {
    (void)inferlab::rms_norm_cpu({1.0F, 2.0F}, 1, 2, {1.0F}, 1e-6F);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "bad gamma shape did not throw");
}

}  // namespace

int main() {
  try {
    test_layer_norm_unit_scale_has_zero_mean();
    test_rms_norm_simple_row();
    test_normalization_rejects_bad_shape();
  } catch (const std::exception& error) {
    std::cerr << "normalization_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
