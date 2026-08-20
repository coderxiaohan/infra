#include "inferlab/matrix_ops.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "checked_index.hpp"

namespace inferlab {
namespace {

void validate_dense_dims(const int m, const int k, const int n,
                         const char* name) {
  if (m <= 0 || k <= 0 || n <= 0) {
    throw std::invalid_argument(std::string(name) +
                                " dimensions must be positive");
  }
}

void validate_rows_cols(const int rows, const int cols, const char* name) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument(std::string(name) +
                                " rows and cols must be positive");
  }
}

std::int64_t checked_sum_i64(const std::int64_t first,
                             const std::int64_t second,
                             const std::int64_t third, const char* name) {
  return internal::checked_add_i64(
      internal::checked_add_i64(first, second, name), third, name);
}

std::int64_t checked_ceil_div_positive_i64(const std::int64_t value,
                                           const std::int64_t divisor,
                                           const char* name) {
  if (value <= 0 || divisor <= 0) {
    throw std::invalid_argument(std::string(name) +
                                " requires positive value and divisor");
  }
  return internal::checked_add_i64(value, divisor - 1, name) / divisor;
}

}  // namespace

std::vector<float> matvec_cpu(const std::vector<float>& matrix, const int rows,
                              const int cols, const std::vector<float>& vector) {
  validate_rows_cols(rows, cols, "matvec_cpu");
  if (matrix.size() !=
      internal::checked_product_size(rows, cols, "matvec_cpu matrix")) {
    throw std::invalid_argument("matvec_cpu matrix size mismatch");
  }
  if (vector.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument("matvec_cpu vector size must equal cols");
  }
  internal::validate_all_finite(matrix, "matvec_cpu matrix");
  internal::validate_all_finite(vector, "matvec_cpu vector");

  std::vector<float> output(static_cast<std::size_t>(rows), 0.0F);
  for (int row = 0; row < rows; ++row) {
    float sum = 0.0F;
    for (int col = 0; col < cols; ++col) {
      sum += matrix[internal::checked_flat_index(row, cols, col,
                                                 "matvec_cpu matrix")] *
             vector[static_cast<std::size_t>(col)];
    }
    if (!std::isfinite(sum)) {
      throw std::invalid_argument("matvec_cpu output must be finite");
    }
    output[static_cast<std::size_t>(row)] = sum;
  }
  return output;
}

std::vector<float> matmul_cpu(const std::vector<float>& a, const int m,
                              const int k, const std::vector<float>& b,
                              const int n) {
  validate_dense_dims(m, k, n, "matmul_cpu");
  if (a.size() != internal::checked_product_size(m, k, "matmul_cpu lhs")) {
    throw std::invalid_argument("matmul_cpu lhs size mismatch");
  }
  if (b.size() != internal::checked_product_size(k, n, "matmul_cpu rhs")) {
    throw std::invalid_argument("matmul_cpu rhs size mismatch");
  }
  internal::validate_all_finite(a, "matmul_cpu lhs");
  internal::validate_all_finite(b, "matmul_cpu rhs");

  std::vector<float> output(
      internal::checked_product_size(m, n, "matmul_cpu output"), 0.0F);
  for (int row = 0; row < m; ++row) {
    for (int col = 0; col < n; ++col) {
      float sum = 0.0F;
      for (int inner = 0; inner < k; ++inner) {
        sum += a[internal::checked_flat_index(row, k, inner,
                                              "matmul_cpu lhs")] *
               b[internal::checked_flat_index(inner, n, col,
                                              "matmul_cpu rhs")];
      }
      if (!std::isfinite(sum)) {
        throw std::invalid_argument("matmul_cpu output must be finite");
      }
      output[internal::checked_flat_index(row, n, col, "matmul_cpu output")] =
          sum;
    }
  }
  return output;
}

std::vector<float> row_softmax_cpu(const std::vector<float>& matrix,
                                   const int rows, const int cols) {
  validate_rows_cols(rows, cols, "row_softmax_cpu");
  if (matrix.size() !=
      internal::checked_product_size(rows, cols, "row_softmax_cpu matrix")) {
    throw std::invalid_argument("row_softmax_cpu matrix size mismatch");
  }

  std::vector<float> output(matrix.size(), 0.0F);
  for (int row = 0; row < rows; ++row) {
    float row_max =
        matrix[internal::checked_flat_index(row, cols, 0,
                                            "row_softmax_cpu matrix")];
    if (std::isnan(row_max) ||
        row_max == std::numeric_limits<float>::infinity()) {
      throw std::invalid_argument(
          "row_softmax_cpu rows must not contain NaN or positive infinity");
    }
    for (int col = 1; col < cols; ++col) {
      const float candidate =
          matrix[internal::checked_flat_index(row, cols, col,
                                              "row_softmax_cpu matrix")];
      if (std::isnan(candidate) ||
          candidate == std::numeric_limits<float>::infinity()) {
        throw std::invalid_argument(
            "row_softmax_cpu rows must not contain NaN or positive infinity");
      }
      row_max = std::max(row_max, candidate);
    }
    if (!std::isfinite(row_max)) {
      throw std::invalid_argument(
          "row_softmax_cpu each row must contain at least one finite value");
    }

    float row_sum = 0.0F;
    for (int col = 0; col < cols; ++col) {
      const auto index =
          internal::checked_flat_index(row, cols, col, "row_softmax_cpu row");
      const float value =
          std::exp(matrix[index] - row_max);
      output[index] = value;
      row_sum += value;
    }
    if (!std::isfinite(row_sum) || row_sum <= 0.0F) {
      throw std::invalid_argument(
          "row_softmax_cpu row sum must be finite and positive");
    }

    for (int col = 0; col < cols; ++col) {
      output[internal::checked_flat_index(row, cols, col,
                                          "row_softmax_cpu output")] /= row_sum;
    }
  }
  return output;
}

DenseOpRooflineProfile profile_matmul_roofline(const int m, const int k,
                                               const int n) {
  validate_dense_dims(m, k, n, "profile_matmul_roofline");

  const auto rows = static_cast<std::int64_t>(m);
  const auto inner = static_cast<std::int64_t>(k);
  const auto cols = static_cast<std::int64_t>(n);
  constexpr std::int64_t element_bytes = static_cast<std::int64_t>(sizeof(float));

  DenseOpRooflineProfile profile;
  profile.m = m;
  profile.k = k;
  profile.n = n;
  profile.lhs_unique_elements =
      internal::checked_product_i64(rows, inner, "matmul lhs elements");
  profile.rhs_unique_elements =
      internal::checked_product_i64(inner, cols, "matmul rhs elements");
  profile.output_elements =
      internal::checked_product_i64(rows, cols, "matmul output elements");
  profile.fma_count =
      internal::checked_product_i64(rows, cols, inner, "matmul FMA count");
  profile.flop_count =
      internal::checked_product_i64(profile.fma_count, 2, "matmul FLOPs");
  const auto compulsory_elements = checked_sum_i64(
      profile.lhs_unique_elements, profile.rhs_unique_elements,
      profile.output_elements, "matmul compulsory elements");
  profile.compulsory_bytes = internal::checked_product_i64(
      compulsory_elements, element_bytes, "matmul compulsory bytes");
  profile.naive_lhs_element_reads = profile.fma_count;
  profile.naive_rhs_element_reads = profile.fma_count;
  profile.naive_output_element_writes = profile.output_elements;
  const auto naive_loop_elements = checked_sum_i64(
      profile.naive_lhs_element_reads, profile.naive_rhs_element_reads,
      profile.naive_output_element_writes, "matmul naive loop elements");
  profile.naive_loop_bytes = internal::checked_product_i64(
      naive_loop_elements, element_bytes, "matmul naive loop bytes");
  profile.compulsory_arithmetic_intensity =
      static_cast<double>(profile.flop_count) /
      static_cast<double>(profile.compulsory_bytes);
  profile.naive_loop_arithmetic_intensity =
      static_cast<double>(profile.flop_count) /
      static_cast<double>(profile.naive_loop_bytes);
  return profile;
}

DenseOpRooflineProfile profile_matvec_roofline(const int rows,
                                               const int cols) {
  validate_rows_cols(rows, cols, "profile_matvec_roofline");
  return profile_matmul_roofline(rows, cols, 1);
}

RowSoftmaxRooflineProfile profile_row_softmax_roofline(const int rows,
                                                       const int cols) {
  validate_rows_cols(rows, cols, "profile_row_softmax_roofline");

  constexpr std::int64_t element_bytes = static_cast<std::int64_t>(sizeof(float));
  const auto row_count = static_cast<std::int64_t>(rows);
  const auto col_count = static_cast<std::int64_t>(cols);

  RowSoftmaxRooflineProfile profile;
  profile.rows = rows;
  profile.cols = cols;
  profile.elements =
      internal::checked_product_i64(row_count, col_count, "softmax elements");
  profile.input_read_bytes = internal::checked_product_i64(
      profile.elements, element_bytes, 2, "softmax input read bytes");
  profile.output_read_bytes = internal::checked_product_i64(
      profile.elements, element_bytes, "softmax output read bytes");
  profile.output_write_bytes = internal::checked_product_i64(
      profile.elements, element_bytes, 2, "softmax output write bytes");
  profile.total_memory_bytes = checked_sum_i64(
      profile.input_read_bytes, profile.output_read_bytes,
      profile.output_write_bytes, "softmax total memory bytes");
  profile.compare_ops = internal::checked_product_i64(
      row_count, col_count - 1, "softmax compare ops");
  profile.exp_ops = profile.elements;
  profile.add_ops = profile.elements;
  profile.div_ops = profile.elements;
  profile.scalar_ops_excluding_exp = checked_sum_i64(
      profile.compare_ops, profile.add_ops, profile.div_ops,
      "softmax scalar ops excluding exp");
  profile.scalar_ops_per_byte =
      static_cast<double>(profile.scalar_ops_excluding_exp) /
      static_cast<double>(profile.total_memory_bytes);
  return profile;
}

TiledMatmulRooflineProfile profile_tiled_matmul_roofline(
    const int m, const int k, const int n, const int tile_m, const int tile_k,
    const int tile_n) {
  validate_dense_dims(m, k, n, "profile_tiled_matmul_roofline");
  validate_dense_dims(tile_m, tile_k, tile_n, "profile_tiled_matmul_roofline");

  constexpr std::int64_t element_bytes = static_cast<std::int64_t>(sizeof(float));
  TiledMatmulRooflineProfile profile;
  profile.m = m;
  profile.k = k;
  profile.n = n;
  profile.tile_m = tile_m;
  profile.tile_k = tile_k;
  profile.tile_n = tile_n;

  const auto row_count = static_cast<std::int64_t>(m);
  const auto inner_count = static_cast<std::int64_t>(k);
  const auto col_count = static_cast<std::int64_t>(n);
  const auto tile_rows = static_cast<std::int64_t>(tile_m);
  const auto tile_cols = static_cast<std::int64_t>(tile_n);
  const auto row_tile_count = checked_ceil_div_positive_i64(
      row_count, tile_rows, "tiled matmul row tile count");
  const auto col_tile_count = checked_ceil_div_positive_i64(
      col_count, tile_cols, "tiled matmul column tile count");
  profile.tiled_lhs_element_loads = internal::checked_product_i64(
      row_count, inner_count, col_tile_count, "tiled matmul lhs element loads");
  profile.tiled_rhs_element_loads = internal::checked_product_i64(
      row_tile_count, inner_count, col_count, "tiled matmul rhs element loads");

  profile.flop_count = internal::checked_product_i64(
      row_count, col_count, inner_count, 2, "tiled matmul FLOPs");
  profile.output_element_writes = internal::checked_product_i64(
      row_count, col_count, "tiled matmul output writes");
  const auto tiled_global_elements = checked_sum_i64(
      profile.tiled_lhs_element_loads, profile.tiled_rhs_element_loads,
      profile.output_element_writes, "tiled matmul global elements");
  profile.tiled_global_bytes = internal::checked_product_i64(
      tiled_global_elements, element_bytes, "tiled matmul global bytes");
  profile.tiled_arithmetic_intensity =
      static_cast<double>(profile.flop_count) /
      static_cast<double>(profile.tiled_global_bytes);

  const auto naive = profile_matmul_roofline(m, k, n);
  profile.naive_to_tiled_byte_reduction =
      static_cast<double>(naive.naive_loop_bytes) /
      static_cast<double>(profile.tiled_global_bytes);
  return profile;
}

}  // namespace inferlab
