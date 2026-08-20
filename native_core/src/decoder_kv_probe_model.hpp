#pragma once

#include "inferlab/decoder_kv.hpp"

#include "decoder_probe_fixture.hpp"

#include <vector>

namespace inferlab::decoder_kv_probe {

struct Request {
  DecoderBlockConfig decoder_config;
  int decode_steps = 0;
  int block_size_tokens = 0;
};

struct Plan {
  Request request;
  PagedKVCacheConfig cache_config;
  probe::DecoderProbeFixturePlan fixture;
  DecoderKVTrace trace;
  DecoderKVCacheProfile cache_profile;
  std::vector<float> last_token_hidden;
  int cache_token_capacity = 0;
  double last_token_output_abs_sum = 0.0;
};

Plan build_plan(const Request& request);

void validate_plan(const Plan& plan);

}  // namespace inferlab::decoder_kv_probe
