#pragma once

#include "advanced_kv_router_artifact_model.hpp"

#include <string>

namespace inferlab::advanced_kv_router_artifact_view {

inline constexpr char kCatalogId[] = "advanced_kv_router";

std::string artifact_json(
    const advanced_kv_router_artifact::Plan& plan);

}  // namespace inferlab::advanced_kv_router_artifact_view
