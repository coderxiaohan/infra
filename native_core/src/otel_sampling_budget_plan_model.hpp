#pragma once

#include "inferlab/advanced_serving_observability.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::otel_sampling_budget_plan {

struct Summary {
  std::size_t max_risk_index = 0U;
  std::size_t max_storage_index = 0U;
  std::size_t max_collector_index = 0U;
};

struct Plan {
  std::vector<OtelSamplingBudgetScenario> scenarios;
  std::vector<OtelSamplingBudgetResult> records;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);

}  // namespace inferlab::otel_sampling_budget_plan
