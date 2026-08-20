#pragma once

#include "inferlab/edge_runtime.hpp"

#include <memory>

namespace inferlab {

std::unique_ptr<IEdgeRuntime> make_executorch_edge_runtime();

}  // namespace inferlab
