#pragma once

#include "release_traffic_profile.hpp"

#include <string>

namespace inferlab_release_traffic {

std::string binding_json(const Binding& binding, int indent);

}  // namespace inferlab_release_traffic
