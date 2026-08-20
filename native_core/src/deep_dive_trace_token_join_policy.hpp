#pragma once

#include "deep_dive_trace_token_join.hpp"

namespace inferlab_deep_dive_trace {

bool trace_token_input_contains_metric(
    const TraceTokenArtifactInput& input, const std::string& metric);

void initialize_trace_token_source(
    TraceTokenSourceObservation& source,
    const TraceTokenArtifactInput& input, TraceTokenSourceRole role);

void append_trace_token_source_blockers_and_actions(
    TraceTokenJoinProjection& projection,
    const TraceTokenSourceObservation& source);

}  // namespace inferlab_deep_dive_trace
