#pragma once

#include <string>
#include <vector>

namespace inferlab {

struct SpeculativeDecodeScenario {
  std::string name;
  int output_tokens = 0;
  int verify_window = 4;
  double acceptance_rate = 0.70;
  double draft_tokens_per_second = 2500.0;
  double target_tokens_per_second = 700.0;
  double target_verify_ms_per_token = 0.35;
  double round_overhead_ms = 0.15;
};

struct SpeculativeDecodeResult {
  SpeculativeDecodeScenario scenario;
  int target_verify_rounds = 0;
  int draft_tokens = 0;
  int accepted_tokens = 0;
  int rejected_tokens = 0;
  double target_only_ms = 0.0;
  double speculative_ms = 0.0;
  double speedup = 1.0;
  double tokens_per_second = 0.0;
  double break_even_acceptance_rate = 0.0;
};

SpeculativeDecodeResult estimate_speculative_decode(
    const SpeculativeDecodeScenario& scenario);

std::vector<SpeculativeDecodeScenario> default_speculative_decode_scenarios();

}  // namespace inferlab
