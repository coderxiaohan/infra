#pragma once

#include "artifact_schema_registry_model.hpp"

#include <string>

namespace inferlab::artifact_schema_registry {

std::string artifact_json(const RegistryPlan& plan);

}  // namespace inferlab::artifact_schema_registry
