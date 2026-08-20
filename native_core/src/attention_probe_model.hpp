#pragma once

#include "inferlab/attention.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::attention_probe {

inline constexpr std::size_t kMaxWorkingElements = 33554432U;

struct Footprint {
  std::size_t query_elements = 0U;
  std::size_t kv_elements = 0U;
  std::size_t score_elements = 0U;
  std::size_t working_elements = 0U;
};

struct Request {
  AttentionShape shape;
  int causal_profile_length = 0;
};

struct Plan {
  Request request;
  Footprint footprint;
  AttentionShape causal_shape;
  AttentionReadProfile non_causal_profile;
  AttentionReadProfile causal_profile;
  std::vector<float> output;
  double output_sum_checksum = 0.0;
};

Plan build_plan(const Request& request);

void validate_plan(const Plan& plan);

}  // namespace inferlab::attention_probe
