#pragma once

#include "inferlab/advanced_serving_observability.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::otel_trace_plan {

struct TraceSchema {
  std::vector<std::string> attribute_contract;
  std::vector<std::string> required_spans;
};

struct Summary {
  std::size_t max_telemetry_index = 0U;
  int over_budget_count = 0;
};

struct Plan {
  std::vector<OtelTraceScenario> scenarios;
  std::vector<OtelTraceResult> records;
  TraceSchema schema;
  Summary summary;
};

Plan build_default_plan();
void validate_plan(const Plan& plan);

}  // namespace inferlab::otel_trace_plan
