#include "inferlab/matrix_ops.hpp"
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

void test_matvec_cpu_matches_reference() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F,
      4.0F, 5.0F, 6.0F,
  };
  const std::vector<float> vector = {1.0F, 0.5F, -1.0F};
  const auto output = inferlab::matvec_cpu(matrix, 2, 3, vector);
  require(output == std::vector<float>({-1.0F, 0.5F}),
          "matvec output mismatch");
}

void test_matmul_cpu_matches_reference() {
  const std::vector<float> a = {
      1.0F, 2.0F, 3.0F,
      4.0F, 5.0F, 6.0F,
  };
  const std::vector<float> b = {
      7.0F, 8.0F,
      9.0F, 10.0F,
      11.0F, 12.0F,
  };
  const auto output = inferlab::matmul_cpu(a, 2, 3, b, 2);
  require(output == std::vector<float>({58.0F, 64.0F, 139.0F, 154.0F}),
          "matmul output mismatch");
}

void test_matmul_cpu_rejects_bad_shape() {
  bool threw = false;
  try {
    (void)inferlab::matmul_cpu({1.0F, 2.0F}, 1, 3, {1.0F}, 1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "bad shape did not throw");
}

void test_row_softmax_cpu_rows_sum_to_one() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F,
      -1.0F, 0.0F, 1.0F,
  };
  const auto output = inferlab::row_softmax_cpu(matrix, 2, 3);
  const float row0 = output[0] + output[1] + output[2];
  const float row1 = output[3] + output[4] + output[5];
  require(std::fabs(row0 - 1.0F) < 1e-6F, "row 0 softmax sum mismatch");
  require(std::fabs(row1 - 1.0F) < 1e-6F, "row 1 softmax sum mismatch");
  require(output[2] > output[1] && output[1] > output[0],
          "softmax order mismatch");
}

void test_row_softmax_cpu_is_shift_invariant() {
  const std::vector<float> a = {1.0F, 2.0F, 3.0F};
  const std::vector<float> b = {101.0F, 102.0F, 103.0F};
  const auto out_a = inferlab::row_softmax_cpu(a, 1, 3);
  const auto out_b = inferlab::row_softmax_cpu(b, 1, 3);
  for (std::size_t i = 0; i < out_a.size(); ++i) {
    require(std::fabs(out_a[i] - out_b[i]) < 1e-6F,
            "softmax shift invariance mismatch");
  }
}

void test_matmul_roofline_profile_counts_bytes_and_flops() {
  const auto profile = inferlab::profile_matmul_roofline(2, 3, 4);
  require(profile.lhs_unique_elements == 6, "matmul lhs unique mismatch");
  require(profile.rhs_unique_elements == 12, "matmul rhs unique mismatch");
  require(profile.output_elements == 8, "matmul output elements mismatch");
  require(profile.fma_count == 24, "matmul FMA count mismatch");
  require(profile.flop_count == 48, "matmul flop count mismatch");
  require(profile.compulsory_bytes == 104, "matmul compulsory bytes mismatch");
  require(profile.naive_lhs_element_reads == 24,
          "matmul naive lhs reads mismatch");
  require(profile.naive_rhs_element_reads == 24,
          "matmul naive rhs reads mismatch");
  require(profile.naive_loop_bytes == 224,
          "matmul naive loop bytes mismatch");
  require(std::fabs(profile.compulsory_arithmetic_intensity -
                    (48.0 / 104.0)) < 1e-9,
          "matmul compulsory intensity mismatch");
  require(std::fabs(profile.naive_loop_arithmetic_intensity -
                    (48.0 / 224.0)) < 1e-9,
          "matmul naive intensity mismatch");
}

void test_matvec_roofline_profile_matches_single_column_matmul() {
  const auto profile = inferlab::profile_matvec_roofline(2, 3);
  require(profile.m == 2 && profile.k == 3 && profile.n == 1,
          "matvec profile dimensions mismatch");
  require(profile.flop_count == 12, "matvec flop count mismatch");
  require(profile.compulsory_bytes == 44, "matvec compulsory bytes mismatch");
  require(profile.naive_loop_bytes == 56, "matvec naive bytes mismatch");
}

void test_row_softmax_roofline_profile_counts_passes() {
  const auto profile = inferlab::profile_row_softmax_roofline(2, 3);
  require(profile.elements == 6, "softmax element count mismatch");
  require(profile.input_read_bytes == 48, "softmax input read bytes mismatch");
  require(profile.output_read_bytes == 24, "softmax output read bytes mismatch");
  require(profile.output_write_bytes == 48,
          "softmax output write bytes mismatch");
  require(profile.total_memory_bytes == 120,
          "softmax total memory bytes mismatch");
  require(profile.compare_ops == 4, "softmax compare ops mismatch");
  require(profile.exp_ops == 6, "softmax exp ops mismatch");
  require(profile.add_ops == 6, "softmax add ops mismatch");
  require(profile.div_ops == 6, "softmax div ops mismatch");
  require(profile.scalar_ops_excluding_exp == 16,
          "softmax scalar ops mismatch");
  require(std::fabs(profile.scalar_ops_per_byte - (16.0 / 120.0)) < 1e-9,
          "softmax ops per byte mismatch");
}

void test_tiled_matmul_roofline_profile_counts_tile_loads() {
  const auto profile = inferlab::profile_tiled_matmul_roofline(4, 8, 6, 2, 4, 3);
  require(profile.tiled_lhs_element_loads == 64,
          "tiled matmul lhs loads mismatch");
  require(profile.tiled_rhs_element_loads == 96,
          "tiled matmul rhs loads mismatch");
  require(profile.output_element_writes == 24,
          "tiled matmul output writes mismatch");
  require(profile.flop_count == 384, "tiled matmul flop count mismatch");
  require(profile.tiled_global_bytes == 736,
          "tiled matmul global bytes mismatch");
  require(std::fabs(profile.tiled_arithmetic_intensity -
                    (384.0 / 736.0)) < 1e-9,
          "tiled matmul intensity mismatch");
  require(std::fabs(profile.naive_to_tiled_byte_reduction -
                    (1632.0 / 736.0)) < 1e-9,
          "tiled matmul byte reduction mismatch");
}

}  // namespace

int main() {
  try {
    test_matvec_cpu_matches_reference();
    test_matmul_cpu_matches_reference();
    test_matmul_cpu_rejects_bad_shape();
    test_row_softmax_cpu_rows_sum_to_one();
    test_row_softmax_cpu_is_shift_invariant();
    test_matmul_roofline_profile_counts_bytes_and_flops();
    test_matvec_roofline_profile_matches_single_column_matmul();
    test_row_softmax_roofline_profile_counts_passes();
    test_tiled_matmul_roofline_profile_counts_tile_loads();
  } catch (const std::exception& error) {
    std::cerr << "matrix_ops_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
