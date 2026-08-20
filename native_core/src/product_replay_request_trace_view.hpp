#pragma once

#include "product_replay_semantic_events_model.hpp"

#include <string>

namespace inferlab::product_replay_semantic_events {

std::string request_trace_projection_json(
    const RequestTraceProjection& projection, int indent);

}  // namespace inferlab::product_replay_semantic_events
