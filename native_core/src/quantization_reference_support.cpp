#include "quantization_reference_support.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace inferlab::quantization_reference {
namespace {

void validate_fixture_spec(const PeriodicFixtureSpec& spec) {
  if (spec.row_factor < 0 || spec.col_factor < 0 ||
      spec.period <= 0 || !std::isfinite(spec.divisor) ||
      spec.divisor <= 0.0F) {
    throw std::invalid_argument(
        "periodic quantization fixture spec is invalid");
  }
}

bool same_matrix(const QuantizedMatrix& lhs,
                 const QuantizedMatrix& rhs) {
  return lhs.rows == rhs.rows &&
         lhs.cols == rhs.cols &&
         lhs.group_size == rhs.group_size &&
         lhs.bits == rhs.bits &&
         lhs.scales == rhs.scales &&
         lhs.int8_data == rhs.int8_data &&
         lhs.int4_packed_data == rhs.int4_packed_data;
}

Result finish_reference(const std::vector<float>& weights,
                        QuantizedMatrix matrix) {
  Result result;
  result.matrix = std::move(matrix);
  result.error = error_stats(weights, dequantize(result.matrix));
  result.storage_bytes = quantized_storage_bytes(result.matrix);
  return result;
}

}  // namespace

std::vector<float> build_periodic_fixture(
    const int rows, const int cols,
    const PeriodicFixtureSpec& spec,
    const std::string& label) {
  validate_fixture_spec(spec);
  std::vector<float> values(
      inferlab_plan::checked_element_count(label, rows, cols));
  const std::int64_t period = spec.period;
  const std::int64_t row_factor = spec.row_factor % spec.period;
  const std::int64_t col_factor = spec.col_factor % spec.period;
  for (int row = 0; row < rows; ++row) {
    const std::int64_t row_term =
        (static_cast<std::int64_t>(row) % period) * row_factor;
    for (int col = 0; col < cols; ++col) {
      const std::int64_t col_term =
          (static_cast<std::int64_t>(col) % period) * col_factor;
      const std::int64_t centered =
          (row_term + col_term) % period - spec.center;
      values[internal::checked_flat_index(
          row, cols, col, label.c_str())] =
          static_cast<float>(centered) / spec.divisor;
    }
  }
  return values;
}

Result run_group_reference(const std::vector<float>& weights,
                           const int rows, const int cols,
                           const int group_size,
                           const QuantizationBits bits) {
  return finish_reference(
      weights, quantize_symmetric_per_group(
                   weights, rows, cols, group_size, bits));
}

Result run_channel_reference(const std::vector<float>& weights,
                             const int rows, const int cols,
                             const QuantizationBits bits) {
  return finish_reference(
      weights, quantize_symmetric_per_channel(
                   weights, rows, cols, bits));
}

bool same_error_stats(const QuantizationErrorStats& lhs,
                      const QuantizationErrorStats& rhs) {
  return lhs.max_abs_error == rhs.max_abs_error &&
         lhs.mean_abs_error == rhs.mean_abs_error;
}

bool same_result(const Result& lhs, const Result& rhs) {
  return same_matrix(lhs.matrix, rhs.matrix) &&
         same_error_stats(lhs.error, rhs.error) &&
         lhs.storage_bytes == rhs.storage_bytes;
}

Summary summarize(const Result& result) {
  return {result.error, result.matrix.scales.size(),
          result.storage_bytes};
}

bool same_summary(const Summary& lhs, const Summary& rhs) {
  return same_error_stats(lhs.error, rhs.error) &&
         lhs.scale_count == rhs.scale_count &&
         lhs.storage_bytes == rhs.storage_bytes;
}

}  // namespace inferlab::quantization_reference
