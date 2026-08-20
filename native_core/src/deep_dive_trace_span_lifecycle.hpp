#pragma once

#include "deep_dive_trace_token_join.hpp"

#include <vector>

namespace inferlab_deep_dive_trace {

void evaluate_trace_span_lifecycle(
    const std::vector<TraceTokenArtifactInput>& inputs,
    TraceTokenJoinProjection& projection);

void validate_trace_span_lifecycle_projection(
    const TraceTokenJoinProjection& projection);

}  // namespace inferlab_deep_dive_trace
