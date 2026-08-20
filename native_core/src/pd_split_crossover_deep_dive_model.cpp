#include "pd_split_crossover_deep_dive_model.hpp"

#include <string>
#include <utility>
#include <vector>

namespace inferlab::pd_split_crossover_deep_dive {
namespace {

std::vector<std::string> trace_record_keys(
    const PdSplitCrossoverPlan& plan) {
  std::vector<std::string> keys;
  keys.reserve(plan.records.size());
  for (const auto& record : plan.records) {
    keys.push_back(record.scenario.name);
  }
  return keys;
}

}  // namespace

Plan build_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios,
    inferlab_native_trace::Source trace_source) {
  Plan plan;
  plan.crossover = build_pd_split_crossover_plan(assumptions, scenarios);
  plan.trace_context = inferlab_native_trace::bind_trace_context(
      inferlab_native_trace::Domain::pd_split_crossover,
      trace_record_keys(plan.crossover), std::move(trace_source));
  return plan;
}

Plan build_plan(
    const PdSplitCrossoverAssumptions& assumptions,
    const std::vector<PdSplitCrossoverScenario>& scenarios) {
  return build_plan(assumptions, scenarios,
                    inferlab_native_trace::Source{});
}

void validate_plan(const Plan& plan) {
  validate_pd_split_crossover_plan(plan.crossover);
  inferlab_native_trace::validate_trace_context_alignment(
      plan.trace_context,
      inferlab_native_trace::Domain::pd_split_crossover,
      trace_record_keys(plan.crossover));
}

}  // namespace inferlab::pd_split_crossover_deep_dive
