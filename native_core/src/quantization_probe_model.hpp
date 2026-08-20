#pragma once

#include "quantization_reference_support.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::quantization_probe {

struct Config {
  int rows = 4;
  int cols = 16;
  int group_size = 8;
};

struct Plan {
  Config config;
  std::vector<float> weights;
  std::vector<float> activation;
  quantization_reference::Result int8;
  quantization_reference::Result int4;
  std::vector<float> int4_materialized;
  std::vector<float> int4_fused;
  QuantizationErrorStats int4_matvec_diff;
  std::size_t fp32_bytes = 0U;
};

Config default_config();
Plan build_plan(const Config& config);
void validate_plan(const Plan& plan);

}  // namespace inferlab::quantization_probe
