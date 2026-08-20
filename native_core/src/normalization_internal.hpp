#pragma once

#include <vector>

namespace inferlab::internal {

bool finite_positive_normalization_value(float value) noexcept;
float checked_norm_denominator(float statistic, float epsilon,
                               const char* name);
void validate_norm_inputs(const std::vector<float>& matrix, int rows, int cols,
                          const std::vector<float>& gamma);

}  // namespace inferlab::internal
