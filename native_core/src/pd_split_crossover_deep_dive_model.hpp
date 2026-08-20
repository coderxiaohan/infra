#pragma once

#include "inferlab/advanced_serving_runtime_communication.hpp"
#include "native_trace_context.hpp"

#include <vector>

namespace inferlab::pd_split_crossover_deep_dive {

struct Plan {
  PdSplitCrossoverPlan crossover;
  inferlab_native_trace::Projection trace_context;
};

Plan build_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios);

Plan build_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios,
    inferlab_native_trace::Source trace_source);

void validate_plan(const Plan& plan);

}  // namespace inferlab::pd_split_crossover_deep_dive
