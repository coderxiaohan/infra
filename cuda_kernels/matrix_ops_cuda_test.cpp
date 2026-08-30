#include "inferlab/matrix_ops.hpp"
#include "matrix_ops_runtime.hpp"
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

void require_close(const std::vector<float>& actual,
                   const std::vector<float>& expected,
                   const std::string& message,
                   const float tolerance = 1e-5F) {
  require(actual.size() == expected.size(), message + " size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < tolerance, message);
  }
}

void test_matvec_cuda_matches_cpu() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F,
      4.0F, 5.0F, 6.0F,
  };
  const std::vector<float> vector = {1.0F, 0.5F, -1.0F};
  require_close(inferlab::matvec_cuda_runtime(matrix, 2, 3, vector),
                inferlab::matvec_cpu(matrix, 2, 3, vector),
                "matvec CUDA mismatch");
  const auto profiled = inferlab::matvec_cuda_runtime_profiled(matrix, 2, 3, vector);
  require_close(profiled.output, inferlab::matvec_cpu(matrix, 2, 3, vector),
                "profiled matvec CUDA mismatch");
  require(profiled.elapsed_ms >= 0.0F, "profiled matvec elapsed time invalid");
  const auto shared_profiled =
      inferlab::matvec_shared_cuda_runtime_profiled(matrix, 2, 3, vector);
  require_close(shared_profiled.output,
                inferlab::matvec_cpu(matrix, 2, 3, vector),
                "shared matvec CUDA mismatch");
  require(shared_profiled.elapsed_ms >= 0.0F,
          "shared matvec elapsed time invalid");
}

void test_matmul_cuda_matches_cpu() {
  const std::vector<float> a = {
      1.0F, 2.0F, 3.0F,
      4.0F, 5.0F, 6.0F,
  };
  const std::vector<float> b = {
      7.0F, 8.0F,
      9.0F, 10.0F,
      11.0F, 12.0F,
  };
  require_close(inferlab::matmul_cuda_runtime(a, 2, 3, b, 2),
                inferlab::matmul_cpu(a, 2, 3, b, 2),
                "matmul CUDA mismatch");
  const auto profiled = inferlab::matmul_cuda_runtime_profiled(a, 2, 3, b, 2);
  require_close(profiled.output, inferlab::matmul_cpu(a, 2, 3, b, 2),
                "profiled matmul CUDA mismatch");
  require(profiled.elapsed_ms >= 0.0F, "profiled matmul elapsed time invalid");
  const auto tiled_profiled =
      inferlab::matmul_tiled_cuda_runtime_profiled(a, 2, 3, b, 2);
  require_close(tiled_profiled.output, inferlab::matmul_cpu(a, 2, 3, b, 2),
                "tiled matmul CUDA mismatch");
  require(tiled_profiled.elapsed_ms >= 0.0F,
          "tiled matmul elapsed time invalid");
}

void test_row_softmax_cuda_matches_cpu() {
  const std::vector<float> matrix = {
      1.0F, 2.0F, 3.0F,
      -1.0F, 0.0F, 1.0F,
  };
  require_close(inferlab::row_softmax_cuda_runtime(matrix, 2, 3),
                inferlab::row_softmax_cpu(matrix, 2, 3),
                "row softmax CUDA mismatch");
  const auto profiled = inferlab::row_softmax_cuda_runtime_profiled(matrix, 2, 3);
  require_close(profiled.output, inferlab::row_softmax_cpu(matrix, 2, 3),
                "profiled row softmax CUDA mismatch");
  require(profiled.elapsed_ms >= 0.0F,
          "profiled row softmax elapsed time invalid");
  const auto block_profiled =
      inferlab::row_softmax_block_cuda_runtime_profiled(matrix, 2, 3);
  require_close(block_profiled.output, inferlab::row_softmax_cpu(matrix, 2, 3),
                "block row softmax CUDA mismatch");
  require(block_profiled.elapsed_ms >= 0.0F,
          "block row softmax elapsed time invalid");
}

void test_cuda_kernels_cover_multiple_blocks_tiles_and_iterations() {
  constexpr int kRows = 257;
  constexpr int kCols = 513;
  std::vector<float> matrix(static_cast<std::size_t>(kRows * kCols));
  std::vector<float> vector(static_cast<std::size_t>(kCols));
  for (int row = 0; row < kRows; ++row) {
    for (int col = 0; col < kCols; ++col) {
      matrix[static_cast<std::size_t>(row * kCols + col)] =
          static_cast<float>((row + col) % 3 - 1) * 0.25F;
    }
  }
  for (int col = 0; col < kCols; ++col) {
    vector[static_cast<std::size_t>(col)] =
        static_cast<float>((col % 2) * 2 - 1) * 0.5F;
  }
  const auto expected_matvec =
      inferlab::matvec_cpu(matrix, kRows, kCols, vector);
  require_close(inferlab::matvec_cuda_runtime(matrix, kRows, kCols, vector),
                expected_matvec, "multi-block matvec CUDA mismatch");
  require_close(
      inferlab::matvec_shared_cuda_runtime(matrix, kRows, kCols, vector),
      expected_matvec, "multi-iteration shared matvec CUDA mismatch");

  constexpr int kM = 17;
  constexpr int kK = 33;
  constexpr int kN = 18;
  std::vector<float> a(static_cast<std::size_t>(kM * kK));
  std::vector<float> b(static_cast<std::size_t>(kK * kN));
  for (std::size_t index = 0; index < a.size(); ++index) {
    a[index] = static_cast<float>(static_cast<int>(index % 5) - 2) * 0.25F;
  }
  for (std::size_t index = 0; index < b.size(); ++index) {
    b[index] = static_cast<float>(static_cast<int>(index % 7) - 3) * 0.125F;
  }
  const auto expected_matmul = inferlab::matmul_cpu(a, kM, kK, b, kN);
  require_close(inferlab::matmul_cuda_runtime(a, kM, kK, b, kN),
                expected_matmul, "multi-block matmul CUDA mismatch");
  require_close(inferlab::matmul_tiled_cuda_runtime(a, kM, kK, b, kN),
                expected_matmul, "multi-tile matmul CUDA mismatch");

  constexpr int kSoftmaxRows = 129;
  constexpr int kSoftmaxCols = 513;
  std::vector<float> softmax_input(
      static_cast<std::size_t>(kSoftmaxRows * kSoftmaxCols));
  for (std::size_t index = 0; index < softmax_input.size(); ++index) {
    softmax_input[index] =
        static_cast<float>(static_cast<int>(index % 23) - 11) * 0.125F;
  }
  const auto expected_softmax =
      inferlab::row_softmax_cpu(softmax_input, kSoftmaxRows, kSoftmaxCols);
  require_close(
      inferlab::row_softmax_cuda_runtime(softmax_input, kSoftmaxRows,
                                         kSoftmaxCols),
      expected_softmax, "multi-block row softmax CUDA mismatch");
  require_close(
      inferlab::row_softmax_block_cuda_runtime(softmax_input, kSoftmaxRows,
                                               kSoftmaxCols),
      expected_softmax, "multi-iteration block softmax CUDA mismatch");
}

}  // namespace

int main() {
  try {
    test_matvec_cuda_matches_cpu();
    test_matmul_cuda_matches_cpu();
    test_row_softmax_cuda_matches_cpu();
    test_cuda_kernels_cover_multiple_blocks_tiles_and_iterations();
  } catch (const std::exception& error) {
    std::cerr << "matrix_ops_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
