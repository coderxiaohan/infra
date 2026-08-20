#pragma once

#include "paged_kv_locality_probe_model.hpp"

#include <string>

namespace inferlab::paged_kv_locality_probe {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::paged_kv_locality_probe
