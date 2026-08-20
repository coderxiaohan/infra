#pragma once

#include <vector>

namespace inferlab::internal {

void validate_rope_inputs(const std::vector<float>& input, int seq_len,
                          int dim, float base, int position_offset);
void rope_rotation(int pair_column, int dim, float base, int position,
                   int position_offset, float* cosine, float* sine);

}  // namespace inferlab::internal
