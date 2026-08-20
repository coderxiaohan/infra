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
                   const std::string& message) {
  require(actual.size() == expected.size(), message + " size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-5F, message);
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

}  // namespace

int main() {
  try {
    test_matvec_cuda_matches_cpu();
    test_matmul_cuda_matches_cpu();
    test_row_softmax_cuda_matches_cpu();
  } catch (const std::exception& error) {
    std::cerr << "matrix_ops_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
