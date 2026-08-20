#pragma once

#include <cstdint>
#include <vector>

namespace inferlab {

struct DenseOpRooflineProfile {
  int m = 0;
  int k = 0;
  int n = 0;
  std::int64_t lhs_unique_elements = 0;
  std::int64_t rhs_unique_elements = 0;
  std::int64_t output_elements = 0;
  std::int64_t fma_count = 0;
  std::int64_t flop_count = 0;
  std::int64_t compulsory_bytes = 0;
  std::int64_t naive_lhs_element_reads = 0;
  std::int64_t naive_rhs_element_reads = 0;
  std::int64_t naive_output_element_writes = 0;
  std::int64_t naive_loop_bytes = 0;
  double compulsory_arithmetic_intensity = 0.0;
  double naive_loop_arithmetic_intensity = 0.0;
};

struct RowSoftmaxRooflineProfile {
  int rows = 0;
  int cols = 0;
  std::int64_t elements = 0;
  std::int64_t input_read_bytes = 0;
  std::int64_t output_read_bytes = 0;
  std::int64_t output_write_bytes = 0;
  std::int64_t total_memory_bytes = 0;
  std::int64_t compare_ops = 0;
  std::int64_t exp_ops = 0;
  std::int64_t add_ops = 0;
  std::int64_t div_ops = 0;
  std::int64_t scalar_ops_excluding_exp = 0;
  double scalar_ops_per_byte = 0.0;
};

struct TiledMatmulRooflineProfile {
  int m = 0;
  int k = 0;
  int n = 0;
  int tile_m = 0;
  int tile_k = 0;
  int tile_n = 0;
  std::int64_t flop_count = 0;
  std::int64_t tiled_lhs_element_loads = 0;
  std::int64_t tiled_rhs_element_loads = 0;
  std::int64_t output_element_writes = 0;
  std::int64_t tiled_global_bytes = 0;
  double tiled_arithmetic_intensity = 0.0;
  double naive_to_tiled_byte_reduction = 0.0;
};

std::vector<float> matvec_cpu(const std::vector<float>& matrix, int rows,
                              int cols, const std::vector<float>& vector);

std::vector<float> matmul_cpu(const std::vector<float>& a, int m, int k,
                              const std::vector<float>& b, int n);

std::vector<float> row_softmax_cpu(const std::vector<float>& matrix, int rows,
                                   int cols);

DenseOpRooflineProfile profile_matmul_roofline(int m, int k, int n);

DenseOpRooflineProfile profile_matvec_roofline(int rows, int cols);

RowSoftmaxRooflineProfile profile_row_softmax_roofline(int rows, int cols);

TiledMatmulRooflineProfile profile_tiled_matmul_roofline(
    int m, int k, int n, int tile_m, int tile_k, int tile_n);

}  // namespace inferlab
