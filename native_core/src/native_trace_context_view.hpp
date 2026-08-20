#pragma once

#include "native_trace_context.hpp"

#include <string>

namespace inferlab_native_trace {

std::string trace_context_projection_json(const Projection& projection,
                                          int indent);

}  // namespace inferlab_native_trace
