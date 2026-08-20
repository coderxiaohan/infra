#pragma once

#include <vector>

namespace inferlab {

std::vector<float> layer_norm_cpu(const std::vector<float>& matrix, int rows,
                                  int cols, const std::vector<float>& gamma,
                                  const std::vector<float>& beta,
                                  float epsilon);

std::vector<float> rms_norm_cpu(const std::vector<float>& matrix, int rows,
                                int cols, const std::vector<float>& gamma,
                                float epsilon);

}  // namespace inferlab
