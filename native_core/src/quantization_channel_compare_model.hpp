#pragma once

#include "quantization_reference_support.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::quantization_channel_compare {

struct Config {
  int rows = 4;
  int cols = 16;
  int group_size = 4;
};

struct Record {
  int group_size = 0;
  quantization_reference::Summary summary;
};

struct Plan {
  Config config;
  std::vector<float> weights;
  Record per_channel;
  Record per_group;
  std::size_t fp32_bytes = 0U;
};

Config default_config();
Plan build_plan(const Config& config);
void validate_plan(const Plan& plan);

}  // namespace inferlab::quantization_channel_compare
