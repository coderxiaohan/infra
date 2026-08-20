#include "inferlab/quantization.hpp"
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

void test_int4_pack_roundtrip_signed_values() {
  const auto packed = inferlab::pack_int4_pair(-3, 7);
  require(inferlab::unpack_int4_low(packed) == -3, "low int4 mismatch");
  require(inferlab::unpack_int4_high(packed) == 7, "high int4 mismatch");
}

void test_int8_per_group_metadata() {
  const std::vector<float> weights = {
      -1.0F, -0.5F, 0.5F, 1.0F,
      -2.0F, 0.0F, 1.0F, 2.0F,
  };
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 2, 4, 2, inferlab::QuantizationBits::kInt8);
  require(matrix.scales.size() == 4, "scale count mismatch");
  require(matrix.int8_data.size() == weights.size(), "int8 data size mismatch");
  require(matrix.group_size == 2, "group size mismatch");
}

void test_per_channel_uses_one_scale_per_row() {
  const std::vector<float> weights = {
      -1.0F, -0.5F, 0.5F, 1.0F,
      -2.0F, 0.0F, 1.0F, 2.0F,
  };
  const auto matrix = inferlab::quantize_symmetric_per_channel(
      weights, 2, 4, inferlab::QuantizationBits::kInt8);
  require(matrix.group_size == 4, "per-channel group size should equal cols");
  require(matrix.scales.size() == 2, "per-channel should have one scale per row");
}

void test_int8_dequant_error_is_small_for_simple_values() {
  const std::vector<float> weights = {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F};
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 1, 5, 5, inferlab::QuantizationBits::kInt8);
  const auto decoded = inferlab::dequantize(matrix);
  const auto stats = inferlab::error_stats(weights, decoded);
  require(stats.max_abs_error < 0.005F, "int8 error too large");
}

void test_int4_dequant_error_is_bounded() {
  const std::vector<float> weights = {-1.0F, -0.5F, 0.0F, 0.5F, 1.0F};
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 1, 5, 5, inferlab::QuantizationBits::kInt4);
  require(matrix.int4_packed_data.size() == 3, "packed int4 size mismatch");
  const auto decoded = inferlab::dequantize(matrix);
  const auto stats = inferlab::error_stats(weights, decoded);
  require(stats.max_abs_error < 0.08F, "int4 error too large");
}

void test_weight_only_matvec_matches_dequantized_reference() {
  const std::vector<float> weights = {
      1.0F, 2.0F, 3.0F,
      -1.0F, 0.5F, 2.0F,
  };
  const std::vector<float> activation = {0.5F, 1.0F, -1.0F};
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 2, 3, 3, inferlab::QuantizationBits::kInt8);
  const auto decoded = inferlab::dequantize(matrix);
  const auto output = inferlab::weight_only_matvec(matrix, activation);

  std::vector<float> reference(2, 0.0F);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      reference[static_cast<std::size_t>(row)] +=
          decoded[static_cast<std::size_t>(row * 3 + col)] *
          activation[static_cast<std::size_t>(col)];
    }
  }
  require(std::fabs(reference[0] - output[0]) < 1e-6F, "row 0 matvec mismatch");
  require(std::fabs(reference[1] - output[1]) < 1e-6F, "row 1 matvec mismatch");
}

void test_fused_dequant_matvec_matches_materialized_reference() {
  const std::vector<float> weights = {
      1.0F, -0.25F, 0.5F, 2.0F,
      -1.0F, 0.75F, 1.5F, -2.0F,
  };
  const std::vector<float> activation = {1.0F, -2.0F, 0.5F, 0.25F};
  const auto matrix = inferlab::quantize_symmetric_per_group(
      weights, 2, 4, 2, inferlab::QuantizationBits::kInt4);
  const auto materialized = inferlab::weight_only_matvec(matrix, activation);
  const auto fused = inferlab::fused_dequant_matvec(matrix, activation);

  require(std::fabs(materialized[0] - fused[0]) < 1e-6F,
          "row 0 fused matvec mismatch");
  require(std::fabs(materialized[1] - fused[1]) < 1e-6F,
          "row 1 fused matvec mismatch");
}

}  // namespace

int main() {
  try {
    test_int4_pack_roundtrip_signed_values();
    test_int8_per_group_metadata();
    test_per_channel_uses_one_scale_per_row();
    test_int8_dequant_error_is_small_for_simple_values();
    test_int4_dequant_error_is_bounded();
    test_weight_only_matvec_matches_dequantized_reference();
    test_fused_dequant_matvec_matches_materialized_reference();
  } catch (const std::exception& error) {
    std::cerr << "quantization_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
