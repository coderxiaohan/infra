#pragma once

#include "release_deployment_resource_profile.hpp"

#include <string>

namespace inferlab_release_deployment_resource {

std::string binding_json(const Binding& binding, int indent);

}  // namespace inferlab_release_deployment_resource
