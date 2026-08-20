#pragma once

#include "quantization_probe_model.hpp"

#include <string>

namespace inferlab::quantization_probe {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::quantization_probe
