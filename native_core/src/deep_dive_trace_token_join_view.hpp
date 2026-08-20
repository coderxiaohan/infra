#pragma once

#include "deep_dive_trace_token_join.hpp"

#include <string>

namespace inferlab_deep_dive_trace {

std::string trace_token_join_json(
    const TraceTokenJoinProjection& projection, int indent);

}  // namespace inferlab_deep_dive_trace
