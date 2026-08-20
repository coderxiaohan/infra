#include "inferlab/rope.hpp"

#include <cmath>
#include <stdexcept>

#include "checked_index.hpp"
#include "rope_internal.hpp"

namespace inferlab {

namespace internal {

namespace {

bool finite_positive_rope_value(const float value) {
  return std::isfinite(value) && value > 0.0F;
}

}  // namespace

void validate_rope_inputs(const std::vector<float>& input, const int seq_len,
                          const int dim, const float base,
                          const int position_offset) {
  if (seq_len <= 0 || dim <= 0 || dim % 2 != 0) {
    throw std::invalid_argument("RoPE requires positive seq_len and even dim");
  }
  if (!finite_positive_rope_value(base)) {
    throw std::invalid_argument("RoPE base must be finite and positive");
  }
  if (position_offset < 0) {
    throw std::invalid_argument("RoPE position_offset must be non-negative");
  }
  if (input.size() != checked_product_size(seq_len, dim, "RoPE input")) {
    throw std::invalid_argument("RoPE input size mismatch");
  }
  validate_all_finite(input, "RoPE input");
}

void rope_rotation(const int pair_column, const int dim, const float base,
                   const int position, const int position_offset,
                   float* cosine, float* sine) {
  if (cosine == nullptr || sine == nullptr || pair_column < 0 ||
      pair_column >= dim || pair_column % 2 != 0 || dim <= 0 ||
      position < 0 || position_offset < 0 ||
      !finite_positive_rope_value(base)) {
    throw std::invalid_argument("invalid RoPE rotation request");
  }
  const float frequency =
      1.0F / std::pow(base, static_cast<float>(pair_column) /
                                static_cast<float>(dim));
  if (!std::isfinite(frequency)) {
    throw std::invalid_argument("RoPE frequency must be finite");
  }
  const auto absolute_position = static_cast<long long>(position) +
                                 static_cast<long long>(position_offset);
  const float angle = static_cast<float>(absolute_position) * frequency;
  if (!std::isfinite(angle)) {
    throw std::invalid_argument("RoPE angle must be finite");
  }
  *cosine = std::cos(angle);
  *sine = std::sin(angle);
  if (!std::isfinite(*cosine) || !std::isfinite(*sine)) {
    throw std::invalid_argument("RoPE sin/cos values must be finite");
  }
}

}  // namespace internal

std::vector<float> apply_rope_cpu(const std::vector<float>& input,
                                  const int seq_len, const int dim,
                                  const float base) {
  return apply_rope_cpu(input, seq_len, dim, base, 0);
}

std::vector<float> apply_rope_cpu(const std::vector<float>& input,
                                  const int seq_len, const int dim,
                                  const float base,
                                  const int position_offset) {
  internal::validate_rope_inputs(input, seq_len, dim, base, position_offset);

  std::vector<float> output(input.size(), 0.0F);
  for (int position = 0; position < seq_len; ++position) {
    for (int i = 0; i < dim; i += 2) {
      float cos_value = 0.0F;
      float sin_value = 0.0F;
      internal::rope_rotation(i, dim, base, position, position_offset,
                              &cos_value, &sin_value);

      const auto index0 =
          internal::checked_flat_index(position, dim, i, "RoPE input");
      const auto index1 =
          internal::checked_flat_index(position, dim, i + 1, "RoPE input");
      const float x0 = input[index0];
      const float x1 = input[index1];
      const float out0 = x0 * cos_value - x1 * sin_value;
      const float out1 = x0 * sin_value + x1 * cos_value;
      if (!std::isfinite(out0) || !std::isfinite(out1)) {
        throw std::invalid_argument("RoPE output must be finite");
      }
      output[index0] = out0;
      output[index1] = out1;
    }
  }
  return output;
}

}  // namespace inferlab
