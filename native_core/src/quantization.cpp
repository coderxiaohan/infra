#include "inferlab/quantization.hpp"

#include "checked_index.hpp"
#include "quantization_internal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace inferlab {
namespace {

int quant_max(const QuantizationBits bits) {
  switch (bits) {
    case QuantizationBits::kInt8:
      return 127;
    case QuantizationBits::kInt4:
      return 7;
  }
  throw std::invalid_argument("unhandled quantization bits");
}

void validate_positive_finite_values(const std::vector<float>& values,
                                     const char* name) {
  for (const auto value : values) {
    if (!std::isfinite(value) || value <= 0.0F) {
      throw std::invalid_argument(std::string(name) +
                                  " values must be finite and positive");
    }
  }
}

void validate_shape(const std::vector<float>& weights, const int rows,
                    const int cols, const int group_size) {
  if (rows <= 0) {
    throw std::invalid_argument("rows must be positive");
  }
  if (cols <= 0) {
    throw std::invalid_argument("cols must be positive");
  }
  if (group_size <= 0) {
    throw std::invalid_argument("group_size must be positive");
  }
  if (weights.size() != internal::checked_product_size(rows, cols, "weights")) {
    throw std::invalid_argument("weights size does not match rows * cols");
  }
  internal::validate_all_finite(weights, "weights");
}

std::int8_t clamp_int4(const std::int8_t value) {
  return static_cast<std::int8_t>(std::max(-8, std::min(7, static_cast<int>(value))));
}

std::int8_t sign_extend_4bit(const std::uint8_t value) {
  const auto nibble = static_cast<std::uint8_t>(value & 0x0F);
  if ((nibble & 0x08U) != 0U) {
    return static_cast<std::int8_t>(static_cast<int>(nibble) - 16);
  }
  return static_cast<std::int8_t>(nibble);
}

std::int8_t read_int4(const std::vector<std::uint8_t>& data,
                      const std::size_t index) {
  const auto packed = data[index / 2U];
  if (index % 2U == 0U) {
    return unpack_int4_low(packed);
  }
  return unpack_int4_high(packed);
}

float quantization_scale(const float max_abs, const int qmax) {
  if (!std::isfinite(max_abs) || max_abs < 0.0F || qmax <= 0) {
    throw std::invalid_argument("invalid quantization scale inputs");
  }
  if (max_abs == 0.0F) return 1.0F;
  const double exact_scale =
      static_cast<double>(max_abs) / static_cast<double>(qmax);
  const double bounded_scale = std::max(
      exact_scale, static_cast<double>(std::numeric_limits<float>::min()));
  return internal::checked_float_from_double(bounded_scale,
                                             "quantization scale");
}

int quantized_value(const float weight, const float scale, const int qmax) {
  if (!std::isfinite(weight) || !std::isfinite(scale) || scale <= 0.0F ||
      qmax <= 0) {
    throw std::invalid_argument("invalid quantized value inputs");
  }
  const double normalized =
      static_cast<double>(weight) / static_cast<double>(scale);
  const int rounded = internal::checked_round_to_even_double_to_int(
      normalized, "normalized quantized value");
  return std::max(-qmax, std::min(qmax, rounded));
}

std::vector<float> dequantize_validated(const QuantizedMatrix& matrix,
                                        const int groups) {
  std::vector<float> output(
      internal::checked_product_size(matrix.rows, matrix.cols,
                                     "dequantized matrix"));
  for (int row = 0; row < matrix.rows; ++row) {
    for (int col = 0; col < matrix.cols; ++col) {
      const auto index =
          internal::checked_flat_index(row, matrix.cols, col,
                                       "dequantized matrix");
      output[index] =
          internal::dequantized_value_at(matrix, groups, row, col, index);
    }
  }
  return output;
}

}  // namespace

namespace internal {

int quantization_groups_per_row(const int cols, const int group_size) {
  if (cols <= 0 || group_size <= 0) {
    throw std::invalid_argument("cols and group_size must be positive");
  }
  return checked_ceil_div_non_negative_int(cols, group_size,
                                           "quantization group count");
}

int validate_quantized_matrix_storage(const QuantizedMatrix& matrix) {
  if (matrix.rows <= 0 || matrix.cols <= 0 || matrix.group_size <= 0) {
    throw std::invalid_argument("invalid quantized matrix shape");
  }
  const int groups =
      quantization_groups_per_row(matrix.cols, matrix.group_size);
  const auto element_count =
      checked_product_size(matrix.rows, matrix.cols,
                           "quantized matrix elements");
  const auto scale_count =
      checked_product_size(matrix.rows, groups, "quantized matrix scales");
  if (matrix.scales.size() != scale_count) {
    throw std::invalid_argument(
        "quantized matrix scales size does not match shape");
  }
  validate_positive_finite_values(matrix.scales, "quantized matrix scales");
  if (matrix.bits == QuantizationBits::kInt8) {
    if (matrix.int8_data.size() != element_count) {
      throw std::invalid_argument(
          "int8 quantized data size does not match shape");
    }
    if (!matrix.int4_packed_data.empty()) {
      throw std::invalid_argument(
          "int8 quantized matrix must not contain int4 payload data");
    }
  } else if (matrix.bits == QuantizationBits::kInt4) {
    if (matrix.int4_packed_data.size() !=
        checked_pair_count(element_count, "int4 packed data size")) {
      throw std::invalid_argument(
          "int4 packed data size does not match shape");
    }
    if (element_count % 2U != 0U &&
        (matrix.int4_packed_data.back() & 0xF0U) != 0U) {
      throw std::invalid_argument(
          "int4 quantized matrix must use zero high-nibble padding");
    }
    if (!matrix.int8_data.empty()) {
      throw std::invalid_argument(
          "int4 quantized matrix must not contain int8 payload data");
    }
  } else {
    throw std::invalid_argument("unhandled quantization bits");
  }
  return groups;
}

float dequantized_value_at(const QuantizedMatrix& matrix, const int groups,
                           const int row, const int col,
                           const std::size_t index) {
  const int group = col / matrix.group_size;
  const float scale =
      matrix.scales[checked_flat_index(row, groups, group,
                                      "quantized scales")];
  const int value = matrix.bits == QuantizationBits::kInt8
                        ? matrix.int8_data[index]
                        : read_int4(matrix.int4_packed_data, index);
  return checked_float_from_double(
      static_cast<double>(value) * static_cast<double>(scale),
      "dequantized value");
}

}  // namespace internal

std::uint8_t pack_int4_pair(const std::int8_t low, const std::int8_t high) {
  const auto low4 = static_cast<std::uint8_t>(clamp_int4(low)) & 0x0FU;
  const auto high4 = static_cast<std::uint8_t>(clamp_int4(high)) & 0x0FU;
  return static_cast<std::uint8_t>(low4 | (high4 << 4U));
}

std::int8_t unpack_int4_low(const std::uint8_t packed) {
  return sign_extend_4bit(packed);
}

std::int8_t unpack_int4_high(const std::uint8_t packed) {
  return sign_extend_4bit(static_cast<std::uint8_t>(packed >> 4U));
}

QuantizedMatrix quantize_symmetric_per_group(const std::vector<float>& weights,
                                             const int rows, const int cols,
                                             const int group_size,
                                             const QuantizationBits bits) {
  validate_shape(weights, rows, cols, group_size);
  const int qmax = quant_max(bits);

  QuantizedMatrix matrix;
  matrix.rows = rows;
  matrix.cols = cols;
  matrix.group_size = group_size;
  matrix.bits = bits;

  const int groups = internal::quantization_groups_per_row(cols, group_size);
  matrix.scales.assign(
      internal::checked_product_size(rows, groups, "quantization groups"),
      1.0F);

  if (bits == QuantizationBits::kInt8) {
    matrix.int8_data.assign(weights.size(), 0);
  } else if (bits == QuantizationBits::kInt4) {
    matrix.int4_packed_data.assign(
        internal::checked_pair_count(weights.size(), "int4 packed data size"),
        0);
  } else {
    throw std::invalid_argument("unhandled quantization bits");
  }

  for (int row = 0; row < rows; ++row) {
    for (int group = 0; group < groups; ++group) {
      const int begin_col = internal::checked_multiply_int(
          group, group_size, "quantization group begin column");
      if (begin_col < 0 || begin_col >= cols) {
        throw std::invalid_argument(
            "quantization group begin column is outside matrix shape");
      }
      const int group_width = std::min(group_size, cols - begin_col);
      const int end_col = internal::checked_add_int(
          begin_col, group_width, "quantization group end column");
      float max_abs = 0.0F;
      for (int col = begin_col; col < end_col; ++col) {
        max_abs = std::max(
            max_abs,
            std::fabs(weights[internal::checked_flat_index(row, cols, col,
                                                           "weights")]));
      }
      const float scale = quantization_scale(max_abs, qmax);
      matrix.scales[internal::checked_flat_index(row, groups, group,
                                                 "quantization scales")] = scale;

      for (int col = begin_col; col < end_col; ++col) {
        const auto index =
            internal::checked_flat_index(row, cols, col, "quantized data");
        const int quantized = quantized_value(weights[index], scale, qmax);
        if (bits == QuantizationBits::kInt8) {
          matrix.int8_data[index] = static_cast<std::int8_t>(quantized);
        } else {
          const auto q4 = static_cast<std::int8_t>(quantized);
          const auto packed_index = index / 2U;
          if (index % 2U == 0U) {
            const auto old_high =
                unpack_int4_high(matrix.int4_packed_data[packed_index]);
            matrix.int4_packed_data[packed_index] =
                pack_int4_pair(q4, old_high);
          } else {
            const auto old_low =
                unpack_int4_low(matrix.int4_packed_data[packed_index]);
            matrix.int4_packed_data[packed_index] =
                pack_int4_pair(old_low, q4);
          }
        }
      }
    }
  }
  return matrix;
}

QuantizedMatrix quantize_symmetric_per_channel(
    const std::vector<float>& weights, const int rows, const int cols,
    const QuantizationBits bits) {
  return quantize_symmetric_per_group(weights, rows, cols, cols, bits);
}

std::vector<float> dequantize(const QuantizedMatrix& matrix) {
  const int groups = internal::validate_quantized_matrix_storage(matrix);
  return dequantize_validated(matrix, groups);
}

std::size_t quantized_storage_bytes(const QuantizedMatrix& matrix) {
  internal::validate_quantized_matrix_storage(matrix);
  const auto scale_bytes = internal::checked_product_size(
      matrix.scales.size(), sizeof(float), "quantization scale bytes");
  const auto payload_bytes =
      matrix.bits == QuantizationBits::kInt8 ? matrix.int8_data.size()
                                             : matrix.int4_packed_data.size();
  return internal::checked_add_size(scale_bytes, payload_bytes,
                                    "quantization storage bytes");
}

std::vector<float> weight_only_matvec(const QuantizedMatrix& matrix,
                                      const std::vector<float>& activation) {
  const int groups = internal::validate_quantized_matrix_storage(matrix);
  if (activation.size() != static_cast<std::size_t>(matrix.cols)) {
    throw std::invalid_argument("activation size must equal matrix cols");
  }
  internal::validate_all_finite(activation, "activation");
  const auto weights = dequantize_validated(matrix, groups);
  std::vector<float> output(static_cast<std::size_t>(matrix.rows), 0.0F);
  for (int row = 0; row < matrix.rows; ++row) {
    double sum = 0.0;
    for (int col = 0; col < matrix.cols; ++col) {
      sum += static_cast<double>(
                 weights[internal::checked_flat_index(
                     row, matrix.cols, col, "dequantized weights")]) *
             static_cast<double>(activation[static_cast<std::size_t>(col)]);
    }
    output[static_cast<std::size_t>(row)] =
        internal::checked_float_from_double(sum, "weight-only matvec output");
  }
  return output;
}

std::vector<float> fused_dequant_matvec(const QuantizedMatrix& matrix,
                                        const std::vector<float>& activation) {
  const int groups = internal::validate_quantized_matrix_storage(matrix);
  if (activation.size() != static_cast<std::size_t>(matrix.cols)) {
    throw std::invalid_argument("activation size must equal matrix cols");
  }
  internal::validate_all_finite(activation, "activation");
  std::vector<float> output(static_cast<std::size_t>(matrix.rows), 0.0F);
  for (int row = 0; row < matrix.rows; ++row) {
    double sum = 0.0;
    for (int col = 0; col < matrix.cols; ++col) {
      const auto index =
          internal::checked_flat_index(row, matrix.cols, col, "quantized data");
      sum += static_cast<double>(
                 internal::dequantized_value_at(matrix, groups, row, col,
                                                index)) *
             static_cast<double>(activation[static_cast<std::size_t>(col)]);
    }
    output[static_cast<std::size_t>(row)] = internal::checked_float_from_double(
        sum, "fused dequant matvec output");
  }
  return output;
}

QuantizationErrorStats error_stats(const std::vector<float>& reference,
                                   const std::vector<float>& actual) {
  if (reference.size() != actual.size()) {
    throw std::invalid_argument("error stats require equal vector sizes");
  }
  internal::validate_all_finite(reference, "reference");
  internal::validate_all_finite(actual, "actual");
  QuantizationErrorStats stats;
  if (reference.empty()) {
    return stats;
  }
  double max_abs_error = 0.0;
  double total_abs = 0.0;
  for (std::size_t i = 0; i < reference.size(); ++i) {
    const double error = std::fabs(static_cast<double>(reference[i]) -
                                   static_cast<double>(actual[i]));
    max_abs_error = std::max(max_abs_error, error);
    total_abs += error;
  }
  stats.max_abs_error = internal::checked_float_from_double(
      max_abs_error, "maximum absolute quantization error");
  stats.mean_abs_error = internal::checked_float_from_double(
      total_abs / static_cast<double>(reference.size()),
      "mean absolute quantization error");
  return stats;
}

}  // namespace inferlab
