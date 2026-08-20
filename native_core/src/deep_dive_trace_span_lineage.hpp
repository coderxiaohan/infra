#pragma once

#include "deep_dive_trace_token_join.hpp"

namespace inferlab_deep_dive_trace {

void evaluate_trace_span_lineage(TraceTokenJoinProjection& projection);

void validate_trace_span_lineage_projection(
    const TraceTokenJoinProjection& projection);

}  // namespace inferlab_deep_dive_trace
