#include "inferlab/normalization.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

#include "checked_index.hpp"
#include "normalization_internal.hpp"

namespace inferlab {
namespace internal {

bool finite_positive_normalization_value(const float value) noexcept {
  return std::isfinite(value) && value > 0.0F;
}

float checked_norm_denominator(const float statistic, const float epsilon,
                               const char* name) {
  const float denominator = statistic + epsilon;
  if (!finite_positive_normalization_value(denominator)) {
    throw std::invalid_argument(std::string(name) +
                                " denominator must be finite and positive");
  }
  return denominator;
}

void validate_norm_inputs(const std::vector<float>& matrix, const int rows,
                          const int cols, const std::vector<float>& gamma) {
  if (rows <= 0 || cols <= 0) {
    throw std::invalid_argument("normalization rows and cols must be positive");
  }
  if (matrix.size() !=
      internal::checked_product_size(rows, cols, "normalization matrix")) {
    throw std::invalid_argument("normalization matrix size mismatch");
  }
  if (gamma.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument("normalization gamma size must equal cols");
  }
  internal::validate_all_finite(matrix, "normalization matrix");
  internal::validate_all_finite(gamma, "normalization gamma");
}

}  // namespace internal

std::vector<float> layer_norm_cpu(const std::vector<float>& matrix,
                                  const int rows, const int cols,
                                  const std::vector<float>& gamma,
                                  const std::vector<float>& beta,
                                  const float epsilon) {
  internal::validate_norm_inputs(matrix, rows, cols, gamma);
  if (beta.size() != static_cast<std::size_t>(cols)) {
    throw std::invalid_argument("layer_norm_cpu beta size must equal cols");
  }
  internal::validate_all_finite(beta, "layer_norm_cpu beta");
  if (!internal::finite_positive_normalization_value(epsilon)) {
    throw std::invalid_argument(
        "layer_norm_cpu epsilon must be finite and positive");
  }

  std::vector<float> output(matrix.size(), 0.0F);
  for (int row = 0; row < rows; ++row) {
    double mean_sum = 0.0;
    for (int col = 0; col < cols; ++col) {
      mean_sum += static_cast<double>(
          matrix[internal::checked_flat_index(row, cols, col,
                                              "layer_norm_cpu matrix")]);
    }
    const float mean = internal::checked_float_from_double(
        mean_sum / static_cast<double>(cols), "layer_norm_cpu mean");

    double variance_sum = 0.0;
    for (int col = 0; col < cols; ++col) {
      const double centered =
          static_cast<double>(
              matrix[internal::checked_flat_index(row, cols, col,
                                                  "layer_norm_cpu matrix")]) -
          static_cast<double>(mean);
      variance_sum += centered * centered;
    }
    const float variance = internal::checked_float_from_double(
        variance_sum / static_cast<double>(cols), "layer_norm_cpu variance");
    const float denominator = internal::checked_norm_denominator(
        variance, epsilon, "layer_norm_cpu");
    const float inv_std = 1.0F / std::sqrt(denominator);
    if (!std::isfinite(inv_std)) {
      throw std::invalid_argument("layer_norm_cpu inv_std must be finite");
    }

    for (int col = 0; col < cols; ++col) {
      const auto index = internal::checked_flat_index(
          row, cols, col, "layer_norm_cpu output");
      const float normalized =
          (matrix[index] - mean) * inv_std;
      const float value =
          normalized * gamma[static_cast<std::size_t>(col)] +
          beta[static_cast<std::size_t>(col)];
      if (!std::isfinite(value)) {
        throw std::invalid_argument("layer_norm_cpu output must be finite");
      }
      output[index] = value;
    }
  }
  return output;
}

std::vector<float> rms_norm_cpu(const std::vector<float>& matrix, const int rows,
                                const int cols,
                                const std::vector<float>& gamma,
                                const float epsilon) {
  internal::validate_norm_inputs(matrix, rows, cols, gamma);
  if (!internal::finite_positive_normalization_value(epsilon)) {
    throw std::invalid_argument(
        "rms_norm_cpu epsilon must be finite and positive");
  }

  std::vector<float> output(matrix.size(), 0.0F);
  for (int row = 0; row < rows; ++row) {
    double mean_square_sum = 0.0;
    for (int col = 0; col < cols; ++col) {
      const double value = static_cast<double>(
          matrix[internal::checked_flat_index(row, cols, col,
                                              "rms_norm_cpu matrix")]);
      mean_square_sum += value * value;
    }
    const float mean_square = internal::checked_float_from_double(
        mean_square_sum / static_cast<double>(cols),
        "rms_norm_cpu mean square");
    const float denominator = internal::checked_norm_denominator(
        mean_square, epsilon, "rms_norm_cpu");
    const float inv_rms = 1.0F / std::sqrt(denominator);
    if (!std::isfinite(inv_rms)) {
      throw std::invalid_argument("rms_norm_cpu inv_rms must be finite");
    }

    for (int col = 0; col < cols; ++col) {
      const auto index =
          internal::checked_flat_index(row, cols, col, "rms_norm_cpu output");
      const float value =
          matrix[index] * inv_rms *
          gamma[static_cast<std::size_t>(col)];
      if (!std::isfinite(value)) {
        throw std::invalid_argument("rms_norm_cpu output must be finite");
      }
      output[index] = value;
    }
  }
  return output;
}

}  // namespace inferlab
