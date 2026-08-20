#pragma once

#include "inferlab/decoder_block.hpp"

#include "decoder_probe_fixture.hpp"

namespace inferlab::decoder_probe {

struct Plan {
  DecoderBlockConfig config;
  probe::DecoderProbeFixturePlan fixture;
  DecoderBlockOutput attention_only;
  DecoderBlockOutput output;
  double output_abs_sum = 0.0;
};

DecoderBlockConfig default_config();
Plan build_plan(const DecoderBlockConfig& config);
void validate_plan(const Plan& plan);

}  // namespace inferlab::decoder_probe
