#pragma once

#include "cuda_quant_microkernel_deep_dive_model.hpp"

#include <string>

namespace inferlab::cuda_quant_microkernel {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::cuda_quant_microkernel
