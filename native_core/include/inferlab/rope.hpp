#pragma once

#include <vector>

namespace inferlab {

std::vector<float> apply_rope_cpu(const std::vector<float>& input, int seq_len,
                                  int dim, float base);

std::vector<float> apply_rope_cpu(const std::vector<float>& input, int seq_len,
                                  int dim, float base, int position_offset);

}  // namespace inferlab
