#pragma once

#include "failure_drill_model.hpp"
#include "observability_slo_plan_model.hpp"
#include "otel_sampling_budget_plan_model.hpp"
#include "otel_trace_plan_model.hpp"

#include <string>

namespace inferlab::observability_evidence {

std::string artifact_json(
    const observability_slo_plan::Plan& slo,
    const otel_trace_plan::Plan& trace,
    const otel_sampling_budget_plan::Plan& sampling,
    const failure_drill::Plan& drill);

}  // namespace inferlab::observability_evidence
