#pragma once

#include "quantization_reference_support.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::quantization_sweep {

struct Config {
  int rows = 4;
  int cols = 16;
};

struct Record {
  int group_size = 0;
  quantization_reference::Summary summary;
};

struct Plan {
  Config config;
  std::vector<int> group_size_inventory;
  std::vector<float> weights;
  std::vector<Record> records;
  std::size_t fp32_bytes = 0U;
};

Config default_config();
Plan build_plan(const Config& config);
void validate_plan(const Plan& plan);

}  // namespace inferlab::quantization_sweep
