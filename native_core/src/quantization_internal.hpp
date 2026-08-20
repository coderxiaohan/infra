#pragma once

#include "inferlab/quantization.hpp"

#include <cstddef>

namespace inferlab::internal {

int quantization_groups_per_row(int cols, int group_size);
int validate_quantized_matrix_storage(const QuantizedMatrix& matrix);
float dequantized_value_at(const QuantizedMatrix& matrix, int groups, int row,
                           int col, std::size_t index);

}  // namespace inferlab::internal
