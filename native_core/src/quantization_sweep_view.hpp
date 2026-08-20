#pragma once

#include "quantization_sweep_model.hpp"

#include <string>

namespace inferlab::quantization_sweep {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::quantization_sweep
